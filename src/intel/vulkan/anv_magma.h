// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANV_MAGMA_H
#define ANV_MAGMA_H

// Don't include anv_private.h here; this header is included by the
// c++ implementation anv_magma_connection.cc.
#include "drm-uapi/i915_drm.h"
#include "magma.h"

#include <stdio.h>

#if DEBUG
#define ANV_MAGMA_DRET(ret) (ret == 0 ? ret : anv_magma_dret(__FILE__, __LINE__, ret))
#else
#define ANV_MAGMA_DRET(ret) (ret)
#endif

struct anv_connection {
   magma_connection_t connection;
};

struct anv_magma_buffer {
   magma_buffer_t buffer;
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
                                      int64_t* timeout_ns);

int AnvMagmaConnectionIsBusy(struct anv_connection* connection, uint64_t buffer_id);

int AnvMagmaConnectionExec(struct anv_connection* connection, uint32_t context_id,
                           struct drm_i915_gem_execbuffer2* execbuf);

// Transfers ownership of the |buffer|.
struct anv_magma_buffer* AnvMagmaCreateBuffer(struct anv_connection* connection,
                                              magma_buffer_t buffer);

void AnvMagmaReleaseBuffer(struct anv_connection* connection, struct anv_magma_buffer* buffer);

static inline int anv_magma_dret(const char* file, const int line, const int64_t ret)
{
   printf("%s:%d returning %ld\n", file, line, ret);
   return ret;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANV_MAGMA_H
