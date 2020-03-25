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

#include "inflight_list.h"
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

static magma_status_t wait_notification_channel(magma_connection_t connection, int64_t timeout_ns)
{
   magma_poll_item_t item = {
       .handle = magma_get_notification_channel_handle(connection),
       .type = MAGMA_POLL_TYPE_HANDLE,
       .condition = MAGMA_POLL_CONDITION_READABLE,
   };
   return magma_poll(&item, 1, timeout_ns);
}

struct InflightList* InflightList_Create()
{
   struct InflightList* list = (struct InflightList*)malloc(sizeof(struct InflightList));
   if (pthread_mutex_init(&list->mutex_, NULL) != 0) {
      free(list);
      return NULL;
   }

   list->wait_ = wait_notification_channel;
   list->read_ = magma_read_notification_channel;
   u_vector_init(&list->buffers_, sizeof(uint64_t), sizeof(uint64_t) * 8 /* initial byte size */);
   list->size_ = 0;
   return list;
}

void InflightList_Destroy(struct InflightList* list)
{
   u_vector_finish(&list->buffers_);
   pthread_mutex_destroy(&list->mutex_);
   free(list);
}

void InflightList_add(struct InflightList* list, uint64_t buffer_id)
{
   assert(buffer_id != 0);
   *(uint64_t*)u_vector_add(&list->buffers_) = buffer_id;
   list->size_ += 1;
}

bool InflightList_remove(struct InflightList* list, uint64_t buffer_id)
{
   bool foundit = false;
   void* element;

   // Find the buffer_id, mark it for removal
   u_vector_foreach(element, &list->buffers_)
   {
      if (*(uint64_t*)element == buffer_id) {
         *(uint64_t*)element = 0;
         foundit = true;
         break;
      }
   }

   if (!foundit)
      return false; // Not found

   assert(list->size_ > 0);
   list->size_ -= 1;

   // Remove all marked nodes at the tail
   const int length = u_vector_length(&list->buffers_);

   for (int i = 0; i < length; i++) {
      element = u_vector_tail(&list->buffers_);
      assert(element);
      if (*(uint64_t*)element == 0) {
         u_vector_remove(&list->buffers_);
      } else {
         break;
      }
   }

   return true;
}

uint32_t InflightList_size(struct InflightList* list) { return list->size_; }

bool InflightList_is_inflight(struct InflightList* list, uint64_t buffer_id)
{
   void* element;
   u_vector_foreach(element, &list->buffers_)
   {
      if (*(uint64_t*)element == buffer_id)
         return true;
   }
   return false;
}

bool InflightList_TryUpdate(struct InflightList* list, magma_connection_t connection)
{
   if (pthread_mutex_trylock(&list->mutex_) != 0) {
      return false;
   }

   InflightList_update(list, connection);

   pthread_mutex_unlock(&list->mutex_);

   return true;
}

magma_status_t InflightList_WaitForBuffer(struct InflightList* list, magma_connection_t connection,
                                          uint64_t buffer_id, uint64_t timeout_ns)
{
   int result = pthread_mutex_lock(&list->mutex_);
   assert(result == 0);

   uint64_t start = gettime_ns();
   uint64_t deadline = start + timeout_ns;

   magma_status_t status = MAGMA_STATUS_OK;

   while (InflightList_is_inflight(list, buffer_id)) {
      status = list->wait_(connection, get_relative_timeout(deadline));

      if (status != MAGMA_STATUS_OK) {
         break;
      }

      InflightList_update(list, connection);
   }

   pthread_mutex_unlock(&list->mutex_);

   return status;
}

void InflightList_AddAndUpdate(struct InflightList* list, magma_connection_t connection,
                               struct magma_system_exec_resource* resources, uint32_t count)
{
   int result = pthread_mutex_lock(&list->mutex_);
   assert(result == 0);

   for (uint32_t i = 0; i < count; i++) {
      InflightList_add(list, resources[i].buffer_id);
   }

   InflightList_update(list, connection);

   pthread_mutex_unlock(&list->mutex_);
}

void InflightList_update(struct InflightList* list, magma_connection_t connection)
{
   uint64_t buffer_ids[8];
   uint64_t bytes_available = 0;
   while (true) {
      magma_status_t status =
          list->read_(connection, buffer_ids, sizeof(buffer_ids), &bytes_available);
      if (status != MAGMA_STATUS_OK) {
         return;
      }
      if (bytes_available == 0)
         return;
      assert(bytes_available % sizeof(uint64_t) == 0);
      for (uint32_t i = 0; i < bytes_available / sizeof(uint64_t); i++) {
         assert(InflightList_is_inflight(list, buffer_ids[i]));
         InflightList_remove(list, buffer_ids[i]);
      }
   }
}
