// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANV_MAGMA_H
#define ANV_MAGMA_H

// Don't include anv_private.h here; this header is included by the
// c++ implementation anv_magma_connection.cc.
#include "i915_drm.h"
#include "magma.h"

struct anv_connection {
   magma_connection_t* connection;
};

#ifdef __cplusplus
extern "C" {
#endif

// Transfer ownership of the |connection|.
struct anv_connection* AnvMagmaCreateConnection(magma_connection_t* connection);

void AnvMagmaReleaseConnection(struct anv_connection* connection);

void AnvMagmaConnectionWait(struct anv_connection* connection, uint64_t buffer_id, int64_t* timeout_ns);

int AnvMagmaConnectionIsBusy(struct anv_connection* connection, uint64_t buffer_id);

int AnvMagmaConnectionExec(struct anv_connection* connection, uint32_t context_id, struct drm_i915_gem_execbuffer2* execbuf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANV_MAGMA_H
