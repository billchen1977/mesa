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

#include "magma_wait.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

static uint64_t gettime_ns(void)
{
   struct timespec current;
   clock_gettime(CLOCK_MONOTONIC, &current);
#define NSEC_PER_SEC 1000000000
   return (uint64_t)current.tv_sec * NSEC_PER_SEC + current.tv_nsec;
#undef NSEC_PER_SEC
}

static uint64_t get_relative_timeout(uint64_t abs_timeout)
{
   uint64_t now = gettime_ns();

   if (abs_timeout < now)
      return 0;
   return abs_timeout - now;
}

int magma_wait(magma_handle_t notification_handle, magma_semaphore_t* semaphores,
               uint32_t semaphore_count, int64_t abs_timeout_ns, bool wait_all,
               void (*notification_callback)(void* context), void* callback_context)
{
   if (semaphore_count == 0)
      return 0;

   uint32_t count = semaphore_count + 1;
   uint32_t channel_index = semaphore_count;

   magma_poll_item_t* items = (magma_poll_item_t*)calloc(count, sizeof(magma_poll_item_t));

   for (uint32_t i = 0; i < semaphore_count; i++) {
      items[i].semaphore = semaphores[i];
      items[i].type = MAGMA_POLL_TYPE_SEMAPHORE;
      items[i].condition = MAGMA_POLL_CONDITION_SIGNALED;
   }
   items[channel_index].handle = notification_handle;
   items[channel_index].type = MAGMA_POLL_TYPE_HANDLE;
   items[channel_index].condition = MAGMA_POLL_CONDITION_READABLE;

   uint32_t signalled_count = 0;

   int result = 1;
   while (result > 0) {
      uint64_t timeout_ns = get_relative_timeout(abs_timeout_ns);

      magma_status_t status = magma_poll(items, count, timeout_ns);

      switch (status) {
      case MAGMA_STATUS_OK:
         for (uint32_t i = 0; i < count; i++) {
            if (items[i].result) {
               if (i == channel_index) {
                  notification_callback(callback_context);
               } else {
                  signalled_count += 1;
                  items[i].condition = 0;
                  if (!wait_all || (signalled_count == semaphore_count)) {
                     result = 0;
                  }
               }
            }
         }
         break;
      case MAGMA_STATUS_TIMED_OUT:
         errno = ETIME;
         // fall through
      default:
         result = -1;
      }
   }

   free(items);
   assert(result < 1);
   return result;
}
