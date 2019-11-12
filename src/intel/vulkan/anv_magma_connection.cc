/*
 * Copyright © 2018 Google, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "anv_magma.h"
#include "gen_gem.h"
#include "magma_sysmem.h"
#include "magma_util/inflight_list.h"
#include "common/intel_log.h"
#include <chrono>
#include <map>
#include <vector>

#if VK_USE_PLATFORM_FUCHSIA
#include "os/fuchsia.h"
#endif

#define LOG_VERBOSE(...) do { if (false) intel_logd(__VA_ARGS__); } while (0)

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
#if VK_USE_PLATFORM_FUCHSIA
      if (sysmem_connection_) {
         magma_sysmem_connection_release(sysmem_connection_);
      }
#endif // VK_USE_PLATFORM_FUCHSIA
      magma_release_connection(magma_connection());
   }

   magma_connection_t magma_connection() { return anv_connection::connection; }

   magma::InflightList* inflight_list() { return &inflight_list_; }

#if VK_USE_PLATFORM_FUCHSIA
   magma_status_t GetSysmemConnection(magma_sysmem_connection_t* sysmem_connection_out)
   {
      if (!sysmem_connection_) {
         zx_handle_t client_handle;
         if (!fuchsia_open("/svc/fuchsia.sysmem.Allocator", &client_handle))
            return DRET(MAGMA_STATUS_INTERNAL_ERROR);
         magma_status_t status = magma_sysmem_connection_import(client_handle, &sysmem_connection_);
         if (status != MAGMA_STATUS_OK)
            return DRET(status);
      }
      *sysmem_connection_out = sysmem_connection_;
      return MAGMA_STATUS_OK;
   }
#endif // VK_USE_PLATFORM_FUCHSIA

   static Connection* cast(anv_connection* connection)
   {
      return static_cast<Connection*>(connection);
   }

private:
#if VK_USE_PLATFORM_FUCHSIA
   magma_sysmem_connection_t sysmem_connection_{};
#endif // #if VK_USE_PLATFORM_FUCHSIA
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

#if VK_USE_PLATFORM_FUCHSIA
magma_status_t AnvMagmaGetSysmemConnection(struct anv_connection* connection,
                                           magma_sysmem_connection_t* sysmem_connection_out)
{
   return Connection::cast(connection)->GetSysmemConnection(sysmem_connection_out);
}
#endif // VK_USE_PLATFORM_FUCHSIA

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

   auto exec_objects = reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr);

   std::vector<magma_system_exec_resource> resources;
   resources.reserve(execbuf->buffer_count);

   for (uint32_t i = 0; i < execbuf->buffer_count; i++) {
      auto buffer = reinterpret_cast<Buffer*>(exec_objects[i].handle);

      uint64_t buffer_id = magma_get_buffer_id(buffer->get());
      uint64_t offset = exec_objects[i].rsvd1;
      uint64_t length = exec_objects[i].rsvd2;

      resources.push_back({ .buffer_id = buffer_id,
         .offset = offset,
         .length = length,
      });

      if (!magma::is_page_aligned(offset))
         return DRET_MSG(-1, "offset (0x%lx) not page aligned", offset);

      uint64_t gpu_addr = gen_48b_address(exec_objects[i].offset);
      uint64_t page_offset = offset / magma::page_size();
      uint64_t page_count =
          magma::round_up(length, magma::page_size()) / magma::page_size();

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
         LOG_VERBOSE("mapping to gpu addr 0x%lx: id %lu page_offset %lu page_count %lu", gpu_addr,
              buffer_id, page_offset, page_count);
         magma_map_buffer_gpu(Connection::cast(connection)->magma_connection(), buffer->get(),
                              page_offset, page_count, gpu_addr, 0);

         buffer->AddMapping(page_offset, page_count, gpu_addr);
      }
   }

   uint32_t syncobj_count = execbuf->num_cliprects;

   std::vector<uint64_t> semaphore_ids;
   semaphore_ids.reserve(syncobj_count);
   uint64_t wait_semaphore_count = 0;

   // Wait semaphores first, then signal
   for (uint32_t i = 0; i < syncobj_count; i++) {
      auto& syncobj = reinterpret_cast<drm_i915_gem_exec_fence*>(execbuf->cliprects_ptr)[i];
      if (syncobj.flags & I915_EXEC_FENCE_WAIT) {
         semaphore_ids.push_back(magma_get_semaphore_id(syncobj.handle));
         wait_semaphore_count++;
      }
   }
   for (uint32_t i = 0; i < syncobj_count; i++) {
      auto& syncobj = reinterpret_cast<drm_i915_gem_exec_fence*>(execbuf->cliprects_ptr)[i];
      if (syncobj.flags & I915_EXEC_FENCE_SIGNAL) {
         semaphore_ids.push_back(magma_get_semaphore_id(syncobj.handle));
      }
   }

   magma_system_command_buffer command_buffer = {
      .batch_buffer_resource_index = execbuf->buffer_count - 1, // by drm convention
      .batch_start_offset = execbuf->batch_start_offset,
      .num_resources = execbuf->buffer_count,
      .wait_semaphore_count = wait_semaphore_count,
      .signal_semaphore_count = semaphore_ids.size() - wait_semaphore_count
   };

   magma_execute_command_buffer_with_resources(Connection::cast(connection)->magma_connection(),
      context_id,
      &command_buffer,
      resources.data(),
      semaphore_ids.data());

   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();

   for (uint32_t i = 0; i < execbuf->buffer_count; i++) {
      inflight_list->add(resources[i].buffer_id);
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
