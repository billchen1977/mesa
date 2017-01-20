// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WSI_COMMON_MAGMA_H
#define WSI_COMMON_MAGMA_H

extern "C" {

#include "wsi_common.h"

VkBool32 wsi_get_physical_device_magma_presentation_support(struct wsi_device* wsi_device,
                                                            VkAllocationCallbacks* alloc,
                                                            uint32_t queueFamilyIndex);

VkResult wsi_create_magma_surface(const VkAllocationCallbacks* pAllocator,
                                  const VkMagmaSurfaceCreateInfoKHR* pCreateInfo,
                                  VkSurfaceKHR* pSurface);
}

#endif
