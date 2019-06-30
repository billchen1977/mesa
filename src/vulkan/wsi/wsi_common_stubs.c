/*
 * Copyright © 2017 Intel Corporation
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

#include "wsi_common_private.h"
#include "vk_util.h"

VkResult
wsi_device_init(struct wsi_device *wsi,
                VkPhysicalDevice pdevice,
                WSI_FN_GetPhysicalDeviceProcAddr proc_addr,
                const VkAllocationCallbacks *alloc,
                int display_fd,
                const struct driOptionCache *dri_options)
{
   return VK_SUCCESS;
}

void
wsi_device_finish(struct wsi_device *wsi,
                  const VkAllocationCallbacks *alloc)
{
}

VkResult
wsi_common_get_surface_support(struct wsi_device *wsi_device,
                               uint32_t queueFamilyIndex,
                               VkSurfaceKHR _surface,
                               VkBool32* pSupported)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_surface_capabilities(struct wsi_device *wsi_device,
                                    VkSurfaceKHR _surface,
                                    VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_surface_capabilities2(struct wsi_device *wsi_device,
                                     const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                     VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_surface_capabilities2ext(
   struct wsi_device *wsi_device,
   VkSurfaceKHR _surface,
   VkSurfaceCapabilities2EXT *pSurfaceCapabilities)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_surface_formats(struct wsi_device *wsi_device,
                               VkSurfaceKHR _surface,
                               uint32_t *pSurfaceFormatCount,
                               VkSurfaceFormatKHR *pSurfaceFormats)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_surface_formats2(struct wsi_device *wsi_device,
                                const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                uint32_t *pSurfaceFormatCount,
                                VkSurfaceFormat2KHR *pSurfaceFormats)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_surface_present_modes(struct wsi_device *wsi_device,
                                     VkSurfaceKHR _surface,
                                     uint32_t *pPresentModeCount,
                                     VkPresentModeKHR *pPresentModes)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_get_present_rectangles(struct wsi_device *wsi_device,
                                  VkSurfaceKHR _surface,
                                  uint32_t* pRectCount,
                                  VkRect2D* pRects)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_create_swapchain(struct wsi_device *wsi,
                            VkDevice device,
                            const VkSwapchainCreateInfoKHR *pCreateInfo,
                            const VkAllocationCallbacks *pAllocator,
                            VkSwapchainKHR *pSwapchain)
{
   return VK_SUCCESS;
}

void
wsi_common_destroy_swapchain(VkDevice device,
                             VkSwapchainKHR _swapchain,
                             const VkAllocationCallbacks *pAllocator)
{
}

VkResult
wsi_common_get_images(VkSwapchainKHR _swapchain,
                      uint32_t *pSwapchainImageCount,
                      VkImage *pSwapchainImages)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_acquire_next_image2(const struct wsi_device *wsi,
                               VkDevice device,
                               const VkAcquireNextImageInfoKHR *pAcquireInfo,
                               uint32_t *pImageIndex)
{
   return VK_SUCCESS;
}

VkResult
wsi_common_queue_present(const struct wsi_device *wsi,
                         VkDevice device,
                         VkQueue queue,
                         int queue_family_index,
                         const VkPresentInfoKHR *pPresentInfo)
{
   return VK_SUCCESS;
}
