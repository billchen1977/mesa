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

#ifndef ANV_MAGMA_H
#define ANV_MAGMA_H

// Don't include anv_private.h here; this header is included by the
// c++ implementation anv_magma_connection.cc.
#include "common/intel_log.h"
#include "drm-uapi/i915_drm.h"
#include "magma.h"

#include <stdio.h>

#define ANV_MAGMA_DRET(ret)                                                                        \
   (ret != 0 ? intel_loge("%s:%d Returning error %ld", __FILE__, __LINE__, (int64_t)ret), ret : ret)

#define ANV_MAGMA_DRET_MSG(ret, format, ...)                                                       \
   (ret != 0 ? intel_loge("%s:%d Returning error %ld: " format, __FILE__, __LINE__, (int64_t)ret,  \
                          ##__VA_ARGS__),                                                          \
    ret : ret)

struct anv_connection {
   magma_connection_t connection;
   struct BufferMap* buffer_map;
   magma_handle_t notification_channel;
};

struct anv_magma_buffer {
   magma_buffer_t buffer;
   uint64_t id;
};

#ifdef __cplusplus
extern "C" {
#endif

// Transfer ownership of the |connection|.
struct anv_connection* AnvMagmaCreateConnection(magma_connection_t connection);

void AnvMagmaReleaseConnection(struct anv_connection* connection);

magma_status_t AnvMagmaGetSysmemConnection(struct anv_connection* connection,
                                           magma_sysmem_connection_t* sysmem_connection_out);

magma_status_t AnvMagmaConnectionWait(struct anv_connection* connection, uint64_t buffer_id,
                                      uint64_t timeout_ns);

void AnvMagmaConnectionServiceNotifications(struct anv_connection* connection);

int AnvMagmaConnectionExec(struct anv_connection* connection, uint32_t context_id,
                           struct drm_i915_gem_execbuffer2* execbuf);

// Transfers ownership of the |buffer|.
struct anv_magma_buffer* AnvMagmaCreateBuffer(struct anv_connection* connection,
                                              magma_buffer_t buffer);

void AnvMagmaReleaseBuffer(struct anv_connection* connection, struct anv_magma_buffer* buffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANV_MAGMA_H
