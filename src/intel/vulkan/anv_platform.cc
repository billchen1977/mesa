// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "anv_private.h"
#include "magma.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "platform_futex.h"
#include <errno.h>

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

int anv_platform_create_semaphore(anv_device* device, anv_platform_semaphore_t* semaphore_out)
{
   DLOG("anv_platform_create_semaphore");
   magma_semaphore_t semaphore;
   magma_status_t status = magma_create_semaphore(device->connection, &semaphore);
   if (status != MAGMA_STATUS_OK)
      return DRET_MSG(-EINVAL, "magma_system_create_semaphore failed: %d", status);
   *semaphore_out = reinterpret_cast<anv_platform_semaphore_t>(semaphore);
   return 0;
}

void anv_platform_destroy_semaphore(anv_device* device, anv_platform_semaphore_t semaphore)
{
   DLOG("anv_platform_destroy_semaphore");
   magma_release_semaphore(device->connection, reinterpret_cast<magma_semaphore_t>(semaphore));
}

void anv_platform_reset_semaphore(anv_platform_semaphore_t semaphore)
{
   DLOG("anv_platform_reset_semaphore");
   magma_reset_semaphore(reinterpret_cast<magma_semaphore_t>(semaphore));
}

int anv_platform_wait_semaphore(anv_platform_semaphore_t semaphore, uint64_t timeout)
{
   DLOG("anv_platform_wait_semaphore");
   magma_status_t status =
       magma_wait_semaphore(reinterpret_cast<magma_semaphore_t>(semaphore), timeout);
   switch (status) {
   case MAGMA_STATUS_OK:
      return 0;
   case MAGMA_STATUS_TIMED_OUT:
      return -ETIME;
   }
   return DRET_MSG(-EINVAL, "unhandled magma status: %d", status);
}

int anv_platform_import_semaphore(anv_device* device, uint32_t semaphore_handle,
                                  anv_platform_semaphore_t* semaphore_out)
{
   DLOG("anv_platform_import_semaphore");
   magma_status_t status = magma_import_semaphore(
       device->connection, semaphore_handle, reinterpret_cast<magma_semaphore_t*>(semaphore_out));
   // TODO() - distinguish case where connection/device lost vs invalid argument
   if (status != MAGMA_STATUS_OK)
      return DRET_MSG(-EINVAL, "magma_import_semaphore failed: %d", status);
   return 0;
}

int anv_platform_export_semaphore(anv_device* device, anv_platform_semaphore_t semaphore,
                                  uint32_t* semaphore_handle_out)
{
   DLOG("anv_platform_export_semaphore");
   magma_status_t status = magma_export_semaphore(
       device->connection, reinterpret_cast<magma_semaphore_t>(semaphore), semaphore_handle_out);
   if (status != MAGMA_STATUS_OK)
      return DRET_MSG(-EINVAL, "magma_export_semaphore failed: %d", status);
   return 0;
}
