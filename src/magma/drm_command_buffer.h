// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRM_COMMAND_BUFFER_H
#define DRM_COMMAND_BUFFER_H

#include "i915_drm.h"
#include <vector>

class DrmCommandBuffer {
public:
   // Returns the number of bytes needed for the magma_system_command_buffer
   // and the associated data structures for |execbuf| with |semaphore_count| semaphores.
   static uint64_t RequiredSize(drm_i915_gem_execbuffer2* execbuf, uint32_t semaphore_count);

   // Writes the magma_system_command_buffer and associated data structures
   // into |command_buffer_out|. |command_buffer_out| must point to a buffer
   // that is sufficiently large, see RequiredSize.
   static bool Translate(drm_i915_gem_execbuffer2* execbuf, const std::vector<uint64_t>& buffer_ids,
                         const std::vector<uint64_t>& wait_semaphore_ids,
                         const std::vector<uint64_t>& signal_semaphore_ids,
                         void* command_buffer_out);

private:
   DrmCommandBuffer() {}
};

#endif // DRM_COMMAND_BUFFER_H
