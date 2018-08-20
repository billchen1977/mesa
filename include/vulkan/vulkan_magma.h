#ifndef VULKAN_MAGMA_H_
#define VULKAN_MAGMA_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

/*
** Copyright (c) 2015-2018 The Khronos Group Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
** This header is generated from the Khronos Vulkan XML API Registry.
**
*/


#ifdef VK_USE_PLATFORM_MAGMA_KHR
#define VK_KHR_magma_surface 1
#define VK_KHR_MAGMA_SURFACE_SPEC_VERSION 1
#define VK_KHR_MAGMA_SURFACE_EXTENSION_NAME "VK_KHR_magma_surface"

typedef struct VkMagmaSurfaceCreateInfoKHR {
    VkStructureType    sType;
    const void*        pNext;
    zx_handle_t        imagePipeHandle;
    uint32_t           width;
    uint32_t           height;
} VkMagmaSurfaceCreateInfoKHR;


typedef VkResult (VKAPI_PTR *PFN_vkCreateMagmaSurfaceKHR)(VkInstance instance, const VkMagmaSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
typedef VkBool32 (VKAPI_PTR *PFN_vkGetPhysicalDeviceMagmaPresentationSupportKHR)(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateMagmaSurfaceKHR(
    VkInstance                                  instance,
    const VkMagmaSurfaceCreateInfoKHR*          pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface);

VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceMagmaPresentationSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex);
#endif
#endif /* VK_USE_PLATFORM_MAGMA_KHR */

#ifdef __cplusplus
}
#endif

#endif
