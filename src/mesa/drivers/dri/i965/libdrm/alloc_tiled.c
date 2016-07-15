/**************************************************************************
 *
 * Copyright � 2007 Red Hat Inc.
 * Copyright � 2007-2012 Intel Corporation
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstr�m <thomas-at-tungstengraphics-dot-com>
 *          Keith Whitwell <keithw-at-tungstengraphics-dot-com>
 *      Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 */

#include <magma.h>
#include <i915_drm.h>
#include <macros.h>

#define ROUND_UP_TO(x, y)  (((x) + (y) - 1) / (y) * (y))

/*
 * Round a given pitch up to the minimum required for X tiling on a
 * given chip.  We use 512 as the minimum to allow for a later tiling
 * change.
 */
static unsigned long
drm_intel_gem_bo_tile_pitch(unsigned long pitch, uint32_t *tiling_mode)
{
    unsigned long tile_width;
    unsigned long i;

    /* If untiled, then just align it so that we can do rendering
     * to it with the 3D engine.
     */
    if (*tiling_mode == I915_TILING_NONE)
        return ALIGN(pitch, 64);

    if (*tiling_mode == I915_TILING_X)
        tile_width = 512;
    else
        tile_width = 128;

    /* Assume gen >= 4; 965 is flexible */
    return ROUND_UP_TO(pitch, tile_width);

    /* The older hardware has a maximum pitch of 8192 with tiled
     * surfaces, so fallback to untiled if it's too large.
     */
    if (pitch > 8192) {
        *tiling_mode = I915_TILING_NONE;
        return ALIGN(pitch, 64);
    }

    /* Pre-965 needs power of two tile width */
    for (i = tile_width; i < pitch; i <<= 1)
        ;

    return i;
}

static unsigned long
drm_intel_gem_bo_tile_size(unsigned long size, uint32_t *tiling_mode)
{
    unsigned long min_size, max_size;
    unsigned long i;

    if (*tiling_mode == I915_TILING_NONE)
        return size;

    /* Assume gen >= 4; 965+ just need multiples of page size for tiling */
    return ROUND_UP_TO(size, 4096);

    min_size = 512*1024;
    max_size = 64*1024*1024;

    if (size > max_size) {
        *tiling_mode = I915_TILING_NONE;
        return size;
    }

    /* Assume has_relaxed_fencing */
    return ROUND_UP_TO(size, 4096);

    for (i = min_size; i < size; i <<= 1)
        ;

    return i;
}

drm_intel_bo* i965_bo_alloc_tiled(drm_intel_bufmgr* bufmgr, const char* name, int x, int y,
                                    int cpp, uint32_t* tiling_mode, unsigned long* pitch,
                                    unsigned long flags)
{
    unsigned long size, stride;
    uint32_t tiling;

    do {
        unsigned long aligned_y, height_alignment;

        tiling = *tiling_mode;

        /* If we're tiled, our allocations are in 8 or 32-row blocks,
         * so failure to align our height means that we won't allocate
         * enough pages.
         *
         * If we're untiled, we still have to align to 2 rows high
         * because the data port accesses 2x2 blocks even if the
         * bottom row isn't to be rendered, so failure to align means
         * we could walk off the end of the GTT and fault.  This is
         * documented on 965, and may be the case on older chipsets
         * too so we try to be careful.
         */
        aligned_y = y;
        height_alignment = 2;

        if (tiling == I915_TILING_X)
            height_alignment = 8;
        else if (tiling == I915_TILING_Y)
            height_alignment = 32;
        aligned_y = ALIGN(y, height_alignment);

        stride = x * cpp;
        stride = drm_intel_gem_bo_tile_pitch(stride, tiling_mode);
        size = stride * aligned_y;
        size = drm_intel_gem_bo_tile_size(size, tiling_mode);
    } while (*tiling_mode != tiling);
    *pitch = stride;

    if (tiling == I915_TILING_NONE)
        stride = 0;

    return magma_bo_alloc_tiled(bufmgr, name, size, flags, tiling, stride);
}
