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

#ifndef INFLIGHT_LIST_H
#define INFLIGHT_LIST_H

#include "u_vector.h"

#include <assert.h>
#include <magma.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

// A convenience utility for maintaining a list of inflight command buffers,
// by reading completed buffer ids from the magma notification channel.
// Not threadsafe.

typedef magma_status_t (*wait_notification_channel_t)(magma_connection_t connection,
                                                      int64_t timeout_ns);

typedef magma_status_t (*read_notification_channel_t)(magma_connection_t connection, void* buffer,
                                                      uint64_t buffer_size,
                                                      uint64_t* buffer_size_out);

struct InflightList {
   wait_notification_channel_t wait_;
   read_notification_channel_t read_;
   struct u_vector buffers_;
   uint32_t size_;
   pthread_mutex_t mutex_;
   uint64_t notification_buffer[4096 / sizeof(uint64_t)];
};

#ifdef __cplusplus
extern "C" {
#endif

struct InflightList* InflightList_Create(void);

void InflightList_Destroy(struct InflightList* list);

static inline void InflightList_inject_for_test(struct InflightList* list,
                                                wait_notification_channel_t wait,
                                                read_notification_channel_t read)
{
   list->wait_ = wait;
   list->read_ = read;
}

// Add to the end of the list.
void InflightList_add(struct InflightList* list, uint64_t buffer_id);

// Remove from anywhere in the list.
bool InflightList_remove(struct InflightList* list, uint64_t buffer_id);

uint32_t InflightList_size(struct InflightList* list);

bool InflightList_is_inflight(struct InflightList* list, uint64_t buffer_id);

void InflightList_update(struct InflightList* list, magma_connection_t connection);

// Returns true if the list lock was obtained. Threadsafe.
bool InflightList_TryUpdate(struct InflightList* list, magma_connection_t magma_connection);

// Wait for the given |buffer_id| to be removed from the inflight list. Threadsafe.
magma_status_t InflightList_WaitForBuffer(struct InflightList* list, magma_connection_t connection,
                                          uint64_t buffer_id, uint64_t timeout_ns);

// Adds the give buffers to the inflight list and services the notification channel. Threadsafe.
void InflightList_AddAndUpdate(struct InflightList* list, magma_connection_t connection,
                               struct magma_system_exec_resource* resources, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif // INFLIGHT_LIST_H
