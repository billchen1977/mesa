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

#ifndef ANV_MAGMA_MAP_H
#define ANV_MAGMA_MAP_H

#include "util/sparse_array.h"
#include <stdatomic.h>

struct BufferMapEntry {
   struct anv_magma_buffer* buffer;
   uint32_t gem_handle;
   uint32_t free_index;
};

/* This is used to assign 32 bit gem handles to magma buffers. */
struct BufferMap {
   struct util_sparse_array array;
   struct util_sparse_array_free_list free_list;
   atomic_uint next_index;
};

void BufferMap_Init(struct BufferMap* map);
void BufferMap_Release(struct BufferMap* map);
void BufferMap_Get(struct BufferMap* map, struct BufferMapEntry** entry_out);
void BufferMap_Put(struct BufferMap* map, uint32_t gem_handle);
void BufferMap_Query(struct BufferMap* map, uint32_t gem_handle, struct BufferMapEntry** entry_out);

#endif /* ANV_MAGMA_MAP_H */
