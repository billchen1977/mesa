/*
 * Copyright © 2019 Google, LLC
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

#include "fuchsia.h"

#include <magma.h>
#include <magma_util/dlog.h>
#include <magma_util/macros.h>
#include <pthread.h>
#include <zircon/syscalls.h>

static fuchsia_open_callback_t g_open_callback;
static pthread_once_t g_initialize_flag = PTHREAD_ONCE_INIT;

static void initialize()
{
   uint32_t client_handle;
   if (!fuchsia_open("/svc/fuchsia.tracing.provider.Registry", &client_handle)) {
      DLOG("Connecting to trace provider failed");
      return;
   }
   magma_status_t status = magma_initialize_tracing(client_handle);
   if (status != MAGMA_STATUS_OK) {
      DLOG("Initializing tracing failed: %d", status);
   }
}

void fuchsia_init(fuchsia_open_callback_t open_callback)
{
   g_open_callback = open_callback;

   // Multiple loader instances may call this multiple times.
   pthread_once(&g_initialize_flag, &initialize);
}

bool fuchsia_open(const char* name, zx_handle_t* channel_out)
{
   if (!g_open_callback)
      return DRETF(false, "No open callback");

   zx_handle_t client_handle, service_handle;
   zx_status_t status = zx_channel_create(0, &client_handle, &service_handle);
   if (status != ZX_OK)
      return DRETF(false, "Channel create failed: %d", status);

   int result = g_open_callback(name, service_handle);
   if (result != 0)
      return DRETF(false, "g_open_callback failed: %d", result);

   *channel_out = client_handle;
   return true;
}
