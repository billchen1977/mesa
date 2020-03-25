/*
 * Copyright © 2020 Google, LLC
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

#ifndef MAGMA_WAIT_H
#define MAGMA_WAIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "magma.h"
#include <stdbool.h>

/* Wait for one (or all) |semaphores| to be signalled, while invoking the given
 * |notification_callback| when there is data readable on |notification_handle|.
 * Returns 0 on success, else -1 on error, with errno set to ETIME on timeout.
 */
int magma_wait(magma_handle_t notification_handle, magma_semaphore_t* semaphores,
               uint32_t semaphore_count, int64_t abs_timeout_ns, bool wait_all,
               void (*notification_callback)(void* context), void* callback_context);

#ifdef __cplusplus
}
#endif

#endif /* MAGMA_WAIT_H */
