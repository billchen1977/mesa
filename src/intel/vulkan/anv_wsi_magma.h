// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANV_WSI_MAGMA_H
#define ANV_WSI_MAGMA_H

#include "anv_private.h"

#ifdef __cplusplus
extern "C" {
#endif

void* anv_wsi_magma_get_connection(VkDevice device);

VkResult anv_wsi_magma_image_create(VkDevice device_h, const VkSwapchainCreateInfoKHR* pCreateInfo,
                                    const VkAllocationCallbacks* pAllocator, VkImage* image_p,
                                    VkDeviceMemory* memory_p, uint32_t* size, uint32_t* offset,
                                    uint32_t* row_pitch, uintptr_t* buffer_handle_p);

void anv_wsi_magma_image_free(VkDevice device, const VkAllocationCallbacks* pAllocator,
                              VkImage image_h, VkDeviceMemory memory_h);

#ifdef __cplusplus
}
#endif

#endif // ANV_WSI_MAGMA_H
