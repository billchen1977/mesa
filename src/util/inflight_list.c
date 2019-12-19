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

struct InflightList* InflightList_Create()
{
   struct InflightList* list = (struct InflightList*)malloc(sizeof(struct InflightList));
   list->wait_ = magma_wait_notification_channel;
   list->read_ = magma_read_notification_channel;
   u_vector_init(&list->buffers_, sizeof(uint64_t), sizeof(uint64_t) * 8 /* initial byte size */);
   list->size_ = 0;
   return list;
}

void InflightList_Destroy(struct InflightList* list)
{
   u_vector_finish(&list->buffers_);
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

magma_status_t InflightList_WaitForCompletion(struct InflightList* list,
                                              magma_connection_t connection, int64_t timeout_ns)
{
   return list->wait_(connection, timeout_ns);
}

void InflightList_ServiceCompletions(struct InflightList* list, magma_connection_t connection)
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
