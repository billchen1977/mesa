// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "drm_command_buffer.h"
#include "magma.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"

uint64_t DrmCommandBuffer::RequiredSize(drm_i915_gem_execbuffer2* execbuf, uint32_t semaphore_count)
{
   auto execobjects = reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr);

   const uint32_t num_resources = execbuf->buffer_count;

   for (uint32_t res_index = 0; res_index < num_resources; res_index++) {
      assert(execobjects[res_index].relocation_count == 0);
   }

   return sizeof(magma_system_command_buffer) + semaphore_count * sizeof(uint64_t) +
          sizeof(magma_system_exec_resource) * num_resources;
}

bool DrmCommandBuffer::Translate(drm_i915_gem_execbuffer2* execbuf,
                                 const std::vector<uint64_t>& buffer_ids,
                                 const std::vector<uint64_t>& wait_semaphore_ids,
                                 const std::vector<uint64_t>& signal_semaphore_ids,
                                 void* command_buffer_out)
{
   DASSERT((execbuf->flags & I915_EXEC_HANDLE_LUT) != 0);

   auto execobjects = reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr);
   const uint32_t num_resources = execbuf->buffer_count;

   magma_system_command_buffer* command_buffer =
       reinterpret_cast<magma_system_command_buffer*>(command_buffer_out);
   uint64_t* dst_wait_semaphore_ids = reinterpret_cast<uint64_t*>(command_buffer + 1);
   uint64_t* dst_signal_semaphore_ids =
       reinterpret_cast<uint64_t*>(dst_wait_semaphore_ids + wait_semaphore_ids.size());
   magma_system_exec_resource* exec_resources = reinterpret_cast<magma_system_exec_resource*>(
       dst_signal_semaphore_ids + signal_semaphore_ids.size());

   for (uint32_t i = 0; i < wait_semaphore_ids.size(); i++) {
      dst_wait_semaphore_ids[i] = wait_semaphore_ids[i];
   }
   for (uint32_t i = 0; i < signal_semaphore_ids.size(); i++) {
      dst_signal_semaphore_ids[i] = signal_semaphore_ids[i];
   }

   for (uint32_t res_index = 0; res_index < num_resources; res_index++) {
      auto dst_res = &exec_resources[res_index];
      auto src_res = &execobjects[res_index];

      dst_res->buffer_id = buffer_ids[res_index];
      dst_res->offset = src_res->rsvd1;
      dst_res->length = src_res->rsvd2;
   }

   command_buffer->num_resources = num_resources;
   command_buffer->batch_buffer_resource_index = num_resources - 1; // by drm convention
   command_buffer->batch_start_offset = execbuf->batch_start_offset;
   command_buffer->wait_semaphore_count = wait_semaphore_ids.size();
   command_buffer->signal_semaphore_count = signal_semaphore_ids.size();

   return true;
}
