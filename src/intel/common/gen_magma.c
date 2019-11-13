/*
 * Copyright © 2018 Google, LLC
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

#include "gen_gem.h"
#include "drm-uapi/i915_drm.h"
#include <assert.h>
#include <magma.h>
#include <msd_intel_gen_query.h>

bool gen_getparam(uintptr_t handle, uint32_t param, int* value_out)
{
   magma_status_t status = MAGMA_STATUS_OK;
   uint64_t value;

   switch (param) {
     case I915_PARAM_CHIPSET_ID:
        status = magma_query2(handle, MAGMA_QUERY_DEVICE_ID, &value);
        break;
     case I915_PARAM_SUBSLICE_TOTAL:
        status = magma_query2(handle, kMsdIntelGenQuerySubsliceAndEuTotal, &value);
        value >>= 32;
        break;
     case I915_PARAM_EU_TOTAL:
        status = magma_query2(handle, kMsdIntelGenQuerySubsliceAndEuTotal, &value);
        value = (uint32_t)value;
        break;
     case I915_PARAM_HAS_WAIT_TIMEOUT:
     case I915_PARAM_HAS_EXECBUF2:
        value = 1;
        break;
     case I915_PARAM_HAS_EXEC_FENCE_ARRAY: // Used for semaphores
        value = 1;
        break;
     case I915_PARAM_HAS_EXEC_SOFTPIN: {
        // client driver manages GPU address space
        status = magma_query2(handle, kMsdIntelGenQueryExtraPageCount, &value);
        break;
     case I915_PARAM_REVISION:
        value = 1;
        break;
     }
     default:
        status = MAGMA_STATUS_INVALID_ARGS;
   }
   if (status != MAGMA_STATUS_OK)
      return false;

   *value_out = value;
   assert(*value_out == value);
   return true;
}
