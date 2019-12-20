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

#include "futex.h"
#include "os/fuchsia.h"
#include "util/timespec.h"
#include <assert.h>
#include <errno.h>
#include <zircon/syscalls.h>

static_assert(sizeof(zx_futex_t) == sizeof(uint32_t), "futex type incompatible size");

enum WaitResult { AWOKE, TIMED_OUT, RETRY };

static inline bool Wake(uint32_t* value_ptr, int32_t wake_count)
{
   zx_status_t status;
   if ((status = zx_futex_wake((zx_futex_t*)value_ptr, wake_count)) != ZX_OK) {
      FUCHSIA_DLOG("zx_futex_wake failed: %d", status);
      return false;
   }
   return true;
}

static inline bool Wait(uint32_t* value_ptr, int32_t current_value, uint64_t timeout_ns,
                        enum WaitResult* result_out)
{
   const zx_time_t deadline =
       (timeout_ns == UINT64_MAX) ? ZX_TIME_INFINITE : zx_deadline_after(timeout_ns);
   zx_status_t status =
       zx_futex_wait((zx_futex_t*)value_ptr, current_value, ZX_HANDLE_INVALID, deadline);
   switch (status) {
   case ZX_OK:
      *result_out = AWOKE;
      break;
   case ZX_ERR_TIMED_OUT:
      *result_out = TIMED_OUT;
      break;
   case ZX_ERR_BAD_STATE:
      *result_out = RETRY;
      break;
   default:
      FUCHSIA_DLOG("zx_futex_wait returned: %d", status);
      return false;
   }
   return true;
}

int futex_wake(uint32_t* addr, int count)
{
   if (!Wake(addr, count)) {
      FUCHSIA_DLOG("PlatformFutex::Wake failed");
      return -1;
   }
   return 0;
}

int futex_wait(uint32_t* addr, int32_t value, const struct timespec* timeout)
{
   uint64_t timeout_ns;
   if (timeout == NULL) {
      timeout_ns = UINT64_MAX;
   } else {
      timeout_ns = timespec_to_nsec(timeout);
   }

   enum WaitResult result;
   if (!Wait(addr, value, timeout_ns, &result)) {
      FUCHSIA_DLOG("PlatformFutex::WaitForever failed");
      return -EINVAL;
   }
   switch (result) {
   case RETRY:
      return -EAGAIN;
   case TIMED_OUT:
      return -ETIMEDOUT;
   default:
      break;
   }
   assert(result == AWOKE);
   return 0;
}
