// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "platform_futex.h"
#include <errno.h>

extern "C" {

int anv_platform_futex_wake(uint32_t* addr, int count)
{
   if (!magma::PlatformFutex::Wake(addr, count))
      return DRET_MSG(-1, "Wake failed");
   return 0;
}

int anv_platform_futex_wait(uint32_t* addr, int32_t value)
{
  magma::PlatformFutex::WaitResult result;
  if (!magma::PlatformFutex::WaitForever(addr, value, &result))
    return DRET_MSG(-EINVAL, "WaitForever failed");
  if (result == magma::PlatformFutex::WaitResult::RETRY)
    return -EAGAIN;
  assert(result == magma::PlatformFutex::WaitResult::AWOKE);
  return 0;
}

} // extern "C"