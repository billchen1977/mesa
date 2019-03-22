// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "anv_magma.h"
#include "drm_command_buffer.h"
#include "gen_gem.h"
#include "magma_sysmem.h"
#include "magma_util/inflight_list.h"
#include "magma_util/macros.h"
#include <chrono>
#include <map>
#include <vector>

class Buffer : public anv_magma_buffer {
public:
   Buffer(magma_buffer_t buffer) { anv_magma_buffer::buffer = buffer; }

   ~Buffer() { DASSERT(!get()); }

   magma_buffer_t get() { return anv_magma_buffer::buffer; }

   void release(magma_connection_t connection)
   {
      magma_release_buffer(connection, get());
      anv_magma_buffer::buffer = 0;
   }

   void AddMapping(uint64_t page_offset, uint64_t page_count, uint64_t addr)
   {
      mappings_[addr] = {page_offset, page_count};
   }

   struct Segment {
      uint64_t page_offset = 0;
      uint64_t page_count = 0;
   };

   bool HasMapping(uint64_t addr, Segment* segment_out) const
   {
      auto iter = mappings_.find(addr);
      if (iter != mappings_.end()) {
         if (segment_out) {
            *segment_out = iter->second;
         }
         return true;
      }
      return false;
   }

   void RemoveMapping(uint64_t addr) { mappings_.erase(addr); }

private:
   std::map<uint64_t, Segment> mappings_;
};

class Connection : public anv_connection {
public:
   Connection(magma_connection_t magma_connection)
   {
      anv_connection::connection = magma_connection;
   }

   ~Connection()
   {
      if (sysmem_connection_) {
         magma_sysmem_connection_release(sysmem_connection_);
      }
      magma_release_connection(magma_connection());
   }

   magma_connection_t magma_connection() { return anv_connection::connection; }

   magma::InflightList* inflight_list() { return &inflight_list_; }

   magma_status_t GetSysmemConnection(magma_sysmem_connection_t* sysmem_connection_out)
   {
      if (!sysmem_connection_) {
         magma_status_t status = magma_sysmem_connection_create(&sysmem_connection_);
         if (status != MAGMA_STATUS_OK)
            return DRET(status);
      }
      *sysmem_connection_out = sysmem_connection_;
      return MAGMA_STATUS_OK;
   }

   static Connection* cast(anv_connection* connection)
   {
      return static_cast<Connection*>(connection);
   }

private:
   magma_sysmem_connection_t sysmem_connection_{};
   magma::InflightList inflight_list_;
};

anv_connection* AnvMagmaCreateConnection(magma_connection_t connection)
{
   return new Connection(connection);
}

void AnvMagmaReleaseConnection(anv_connection* connection)
{
   delete static_cast<Connection*>(connection);
}

magma_status_t AnvMagmaGetSysmemConnection(struct anv_connection* connection,
                                           magma_sysmem_connection_t* sysmem_connection_out)
{
   return Connection::cast(connection)->GetSysmemConnection(sysmem_connection_out);
}

magma_status_t AnvMagmaConnectionWait(anv_connection* connection, uint64_t buffer_id,
                                      int64_t* timeout_ns)
{
   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();
   auto start = std::chrono::high_resolution_clock::now();

   while (inflight_list->is_inflight(buffer_id) &&
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now() - start)
                  .count() < *timeout_ns) {

      magma_connection_t magma_connection = Connection::cast(connection)->magma_connection();

      magma::Status status = inflight_list->WaitForCompletion(magma_connection, *timeout_ns);
      if (status.ok()) {
         inflight_list->ServiceCompletions(magma_connection);
      } else {
         return status.get();
      }
   }
   return MAGMA_STATUS_OK;
}

int AnvMagmaConnectionIsBusy(anv_connection* connection, uint64_t buffer_id)
{
   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();

   inflight_list->ServiceCompletions(Connection::cast(connection)->magma_connection());

   return inflight_list->is_inflight(buffer_id) ? 1 : 0;
}

int AnvMagmaConnectionExec(anv_connection* connection, uint32_t context_id,
                           struct drm_i915_gem_execbuffer2* execbuf)
{
   if (execbuf->buffer_count == 0)
      return 0;

   std::vector<uint64_t> buffer_ids(execbuf->buffer_count);
   auto exec_objects = reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr);
   for (uint32_t i = 0; i < buffer_ids.size(); i++) {
      auto buffer = reinterpret_cast<Buffer*>(exec_objects[i].handle);
      buffer_ids[i] = magma_get_buffer_id(buffer->get());

      uint64_t offset = exec_objects[i].rsvd1;
      if (!magma::is_page_aligned(offset))
         return DRET_MSG(-1, "offset (0x%lx) not page aligned", offset);

      uint64_t gpu_addr = gen_48b_address(exec_objects[i].offset);
      uint64_t page_offset = offset / magma::page_size();
      uint64_t page_count =
          magma::round_up(exec_objects[i].rsvd2, magma::page_size()) / magma::page_size();

      Buffer::Segment segment;
      bool has_mapping = buffer->HasMapping(gpu_addr, &segment);
      if (has_mapping) {
         assert(page_offset == segment.page_offset);
         if (page_count > segment.page_count) {
            // Growing an existing mapping.
            buffer->RemoveMapping(gpu_addr);
            has_mapping = false;
         } else {
            assert(page_count == segment.page_count);
         }
      }

      if (!has_mapping) {
         DLOG("mapping to gpu addr 0x%lx: id %lu page_offset %lu page_count %lu", gpu_addr,
              buffer_ids[i], page_offset, page_count);
         magma_map_buffer_gpu(Connection::cast(connection)->magma_connection(), buffer->get(),
                              page_offset, page_count, gpu_addr, 0);

         buffer->AddMapping(page_offset, page_count, gpu_addr);
      }
   }

   uint32_t syncobj_count = execbuf->num_cliprects;

   uint64_t required_size = DrmCommandBuffer::RequiredSize(execbuf, syncobj_count);

   uint64_t cmd_buf_id;
   magma_status_t status = magma_create_command_buffer(
       Connection::cast(connection)->magma_connection(), required_size, &cmd_buf_id);
   if (status != MAGMA_STATUS_OK)
      return DRET_MSG(-1, "magma_alloc_command_buffer failed size 0x%" PRIx64 " : %d",
                      required_size, status);

   void* cmd_buf_data;
   status = magma_map(Connection::cast(connection)->magma_connection(), cmd_buf_id, &cmd_buf_data);
   if (status != MAGMA_STATUS_OK) {
      magma_release_command_buffer(Connection::cast(connection)->magma_connection(), cmd_buf_id);
      return DRET_MSG(-1, "magma_system_map failed: %d", status);
   }

   std::vector<uint64_t> wait_semaphore_ids;
   std::vector<uint64_t> signal_semaphore_ids;

   for (uint32_t i = 0; i < syncobj_count; i++) {
      auto& syncobj = reinterpret_cast<drm_i915_gem_exec_fence*>(execbuf->cliprects_ptr)[i];
      if (syncobj.flags & I915_EXEC_FENCE_WAIT) {
         wait_semaphore_ids.push_back(magma_get_semaphore_id(syncobj.handle));
      } else if (syncobj.flags & I915_EXEC_FENCE_SIGNAL) {
         signal_semaphore_ids.push_back(magma_get_semaphore_id(syncobj.handle));
      } else {
         return DRET_MSG(-1, "syncobj not wait or signal");
      }
   }

   if (!DrmCommandBuffer::Translate(execbuf, std::move(buffer_ids), std::move(wait_semaphore_ids),
                                    std::move(signal_semaphore_ids), cmd_buf_data)) {
      status = magma_unmap(Connection::cast(connection)->magma_connection(), cmd_buf_id);
      DASSERT(status == MAGMA_STATUS_OK);
      magma_release_command_buffer(Connection::cast(connection)->magma_connection(), cmd_buf_id);
      return DRET_MSG(-1, "DrmCommandBuffer::Translate failed");
   }

   status = magma_unmap(Connection::cast(connection)->magma_connection(), cmd_buf_id);
   DASSERT(status == MAGMA_STATUS_OK);

   magma_submit_command_buffer(Connection::cast(connection)->magma_connection(), cmd_buf_id,
                               context_id);

   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();

   for (uint32_t i = 0; i < execbuf->buffer_count; i++) {
      inflight_list->add(
          magma_get_buffer_id(reinterpret_cast<Buffer*>(exec_objects[i].handle)->get()));
   }

   inflight_list->ServiceCompletions(Connection::cast(connection)->magma_connection());

   return 0;
}

anv_magma_buffer* AnvMagmaCreateBuffer(anv_connection* connection, magma_buffer_t buffer)
{
   return new Buffer(buffer);
}

void AnvMagmaReleaseBuffer(anv_connection* connection, anv_magma_buffer* anv_buffer)
{
   auto buffer = static_cast<Buffer*>(anv_buffer);
   // Hardware mappings are released when the buffer is released.
   buffer->release(Connection::cast(connection)->magma_connection());
   delete buffer;
}
