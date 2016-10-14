// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DRM_COMMAND_BUFFER_H
#define DRM_COMMAND_BUFFER_H

#include "i915_drm.h"
#include "magma_system.h"
#include <vector>

class DrmCommandBuffer {
public:
   static std::unique_ptr<DrmCommandBuffer> Create(drm_i915_gem_execbuffer2* execbuf);

   magma_system_command_buffer* system_command_buffer()
   {
      return reinterpret_cast<magma_system_command_buffer*>(buffer_.data());
   }

   bool Translate(drm_i915_gem_execbuffer2* execbuf);

private:
   DrmCommandBuffer() {}

   std::vector<uint8_t> buffer_;
};

#endif // DRM_COMMAND_BUFFER_H
