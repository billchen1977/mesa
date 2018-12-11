// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "anv_magma.h"
#include "drm_command_buffer.h"
#include "magma_util/inflight_list.h"
#include "magma_util/macros.h"
#include <chrono>

class Connection : public anv_connection {
public:
   Connection(magma_connection_t* magma_connection)
       : inflight_list_(magma_connection)
   {
      anv_connection::connection = magma_connection;
   }

   ~Connection() { magma_release_connection(magma_connection()); }

   magma_connection_t* magma_connection() { return anv_connection::connection; }

   magma::InflightList* inflight_list() { return &inflight_list_; }

   static Connection* cast(anv_connection* connection) { return static_cast<Connection*>(connection); }

private:
   magma::InflightList inflight_list_;
};

anv_connection* AnvMagmaCreateConnection(magma_connection_t* connection)
{
   return new Connection(connection);
}

void AnvMagmaReleaseConnection(anv_connection* connection)
{
   delete static_cast<Connection*>(connection);
}

void AnvMagmaConnectionWait(anv_connection* connection, uint64_t buffer_id, int64_t* timeout_ns)
{
   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();

   auto start = std::chrono::high_resolution_clock::now();

   while (inflight_list->is_inflight(buffer_id) &&
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now() - start)
                  .count() < *timeout_ns) {

      if (inflight_list->WaitForCompletion(magma::ns_to_ms(*timeout_ns))) {
         inflight_list->ServiceCompletions(Connection::cast(connection)->magma_connection());
      }
   }
}

int AnvMagmaConnectionIsBusy(anv_connection* connection, uint64_t buffer_id)
{
   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();

   inflight_list->ServiceCompletions(Connection::cast(connection)->magma_connection());

   return inflight_list->is_inflight(buffer_id) ? 1 : 0;
}

int AnvMagmaConnectionExec(anv_connection* connection, uint32_t context_id, struct drm_i915_gem_execbuffer2* execbuf)
{
   if (execbuf->buffer_count == 0)
      return 0;

   uint32_t syncobj_count = execbuf->num_cliprects;

   uint64_t required_size = DrmCommandBuffer::RequiredSize(execbuf, syncobj_count);

   uint64_t cmd_buf_id;
   magma_status_t status =
       magma_create_command_buffer(Connection::cast(connection)->magma_connection(), required_size, &cmd_buf_id);
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

   if (!DrmCommandBuffer::Translate(execbuf, std::move(wait_semaphore_ids),
                                    std::move(signal_semaphore_ids), cmd_buf_data)) {
      status = magma_unmap(Connection::cast(connection)->magma_connection(), cmd_buf_id);
      DASSERT(status == MAGMA_STATUS_OK);
      magma_release_command_buffer(Connection::cast(connection)->magma_connection(), cmd_buf_id);
      return DRET_MSG(-1, "DrmCommandBuffer::Translate failed");
   }

   status = magma_unmap(Connection::cast(connection)->magma_connection(), cmd_buf_id);
   DASSERT(status == MAGMA_STATUS_OK);

   magma_submit_command_buffer(Connection::cast(connection)->magma_connection(), cmd_buf_id, context_id);

   magma::InflightList* inflight_list = Connection::cast(connection)->inflight_list();

   for (uint32_t i = 0; i < execbuf->buffer_count; i++) {
      inflight_list->add(magma_get_buffer_id(
          reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr)[i].handle));
   }

   inflight_list->ServiceCompletions(Connection::cast(connection)->magma_connection());

   return 0;
}
