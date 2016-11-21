// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "drm_command_buffer.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"

uint64_t DrmCommandBuffer::RequiredSize(drm_i915_gem_execbuffer2* execbuf)
{
   auto execobjects = reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr);

   const uint32_t num_resources = execbuf->buffer_count;

   uint32_t num_relocations = 0;
   for (uint32_t res_index = 0; res_index < num_resources; res_index++) {
      num_relocations += execobjects[res_index].relocation_count;
   }

   return sizeof(magma_system_command_buffer) + sizeof(magma_system_exec_resource) * num_resources +
          sizeof(magma_system_relocation_entry) * num_relocations;
}

bool DrmCommandBuffer::Translate(drm_i915_gem_execbuffer2* execbuf, void* command_buffer_out)
{
   DASSERT((execbuf->flags & I915_EXEC_HANDLE_LUT) != 0);

   auto execobjects = reinterpret_cast<drm_i915_gem_exec_object2*>(execbuf->buffers_ptr);
   const uint32_t num_resources = execbuf->buffer_count;

   magma_system_command_buffer* command_buffer =
       reinterpret_cast<magma_system_command_buffer*>(command_buffer_out);
   magma_system_exec_resource* exec_resources =
       reinterpret_cast<magma_system_exec_resource*>(command_buffer + 1);
   magma_system_relocation_entry* relocation_entries =
       reinterpret_cast<magma_system_relocation_entry*>(exec_resources + num_resources);

   uint32_t res_reloc_base = 0;

   for (uint32_t res_index = 0; res_index < num_resources; res_index++) {
      auto dst_res = &exec_resources[res_index];
      auto src_res = &execobjects[res_index];
      auto src_res_relocs = reinterpret_cast<drm_i915_gem_relocation_entry*>(src_res->relocs_ptr);

      DLOG("translating res_index %u handle 0x%x start_offset 0x%lx length 0x%lx", res_index,
           src_res->handle, src_res->rsvd1, src_res->rsvd2);

      uint32_t num_relocations = dst_res->num_relocations = src_res->relocation_count;

      auto relocations = &relocation_entries[res_reloc_base];
      dst_res->buffer_id = src_res->handle;
      dst_res->offset = src_res->rsvd1;
      dst_res->length = src_res->rsvd2;

      for (uint32_t reloc_index = 0; reloc_index < dst_res->num_relocations; reloc_index++) {
         auto dst_reloc = &relocations[reloc_index];
         auto src_reloc = &src_res_relocs[reloc_index];

         DLOG("translating reloc_index %u: target_handle 0x%x", reloc_index,
              src_reloc->target_handle);

         // anvil sends indices for target_handles
         uint32_t target_index = src_reloc->target_handle;
         if (target_index >= num_resources)
            return DRET_MSG(-1, "invalid target index %u", src_reloc->target_handle);

         dst_reloc->offset = src_reloc->offset; // offset in the batch buffer;
         dst_reloc->target_resource_index = src_reloc->target_handle;
         dst_reloc->target_offset = src_reloc->delta; // offset in the target buffer
         dst_reloc->read_domains_bitfield =
             src_reloc->read_domains; // memory domains in which the target is readable
         dst_reloc->write_domains_bitfield = src_reloc->write_domain; // memory
      }

      res_reloc_base += num_relocations;
   }

   command_buffer->num_resources = num_resources;
   command_buffer->batch_buffer_resource_index = num_resources - 1; // by drm convention

   return true;
}
