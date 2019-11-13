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
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "platform_futex.h"
#include <errno.h>

int futex_wake(uint32_t* addr, int count)
{
   if (!magma::PlatformFutex::Wake(addr, count))
      return DRET_MSG(-1, "Wake failed");
   return 0;
}

int futex_wait(uint32_t* addr, int32_t value, const struct timespec* timeout)
{
   // Timeouts not implemented.
   assert(timeout == nullptr);
   magma::PlatformFutex::WaitResult result;
   if (!magma::PlatformFutex::WaitForever(addr, value, &result))
      return DRET_MSG(-EINVAL, "PlatformFutex::WaitForever failed");
   if (result == magma::PlatformFutex::WaitResult::RETRY)
      return -EAGAIN;
   assert(result == magma::PlatformFutex::WaitResult::AWOKE);
   return 0;
}
