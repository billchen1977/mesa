// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _FUCHSIA_H_
#define _FUCHSIA_H_

#include <stdbool.h>
#include <stdio.h>
#include <zircon/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*fuchsia_open_callback_t)(const char* name, zx_handle_t handle);

void fuchsia_init(fuchsia_open_callback_t open_callback);

// Open a channel to the given namespace element, returns a channel handle
// to the backing service.
bool fuchsia_open(const char* name, zx_handle_t* channel_out);

#ifdef __cplusplus
}
#endif

#endif // _FUCHSIA_H_
