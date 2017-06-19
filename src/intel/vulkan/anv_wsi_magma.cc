// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "anv_wsi_magma.h"
#include "magma.h"
#include "wsi_common_magma.h"

#include "anv_private.h"
#include "vk_format_info.h"

VkBool32 anv_GetPhysicalDeviceMagmaPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                          uint32_t queueFamilyIndex)
{
   ANV_FROM_HANDLE(anv_physical_device, device, physicalDevice);

   return wsi_get_physical_device_magma_presentation_support(
       &device->wsi_device, &device->instance->alloc, queueFamilyIndex);
}

VkResult anv_CreateMagmaSurfaceKHR(VkInstance _instance,
                                   const VkMagmaSurfaceCreateInfoKHR* pCreateInfo,
                                   const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
   ANV_FROM_HANDLE(anv_instance, instance, _instance);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_MAGMA_SURFACE_CREATE_INFO_KHR);

   if (!pAllocator)
      pAllocator = &instance->alloc;

   auto surface = reinterpret_cast<VkIcdSurfaceMagma*>(
       vk_alloc(pAllocator, sizeof(VkIcdSurfaceMagma), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
   if (!surface)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->base.platform = VK_ICD_WSI_PLATFORM_MAGMA;

   *pSurface = VkIcdSurfaceBase_to_handle(&surface->base);

   return VK_SUCCESS;
}

void* anv_wsi_magma_get_render_connection(VkDevice device)
{
   return anv_device_from_handle(device)->connection;
}

void* anv_wsi_magma_open_display_connection(VkDevice device)
{
   return magma_open(anv_device_from_handle(device)->fd, MAGMA_CAPABILITY_DISPLAY);
}

void anv_wsi_magma_close_display_connection(void* connection)
{
   magma_close(reinterpret_cast<magma_connection_t*>(connection));
}

VkResult anv_wsi_magma_image_create(VkDevice device_h, const VkSwapchainCreateInfoKHR* pCreateInfo,
                                    const VkAllocationCallbacks* pAllocator, VkImage* image_p,
                                    VkDeviceMemory* memory_p, uint32_t* size, uint32_t* offset,
                                    uint32_t* row_pitch, uintptr_t* buffer_handle_p)
{
   struct anv_device* device = anv_device_from_handle(device_h);
   VkImage image_h;
   struct anv_image* image;
   struct anv_surface* surface;
   int ret;

   VkImageCreateInfo create_info = {
       .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
       .imageType = VK_IMAGE_TYPE_2D,
       .format = pCreateInfo->imageFormat,
       .extent = {.width = pCreateInfo->imageExtent.width,
                  .height = pCreateInfo->imageExtent.height,
                  .depth = 1},
       .mipLevels = 1,
       .arrayLayers = 1,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .tiling = VK_IMAGE_TILING_OPTIMAL,
       .usage = (pCreateInfo->imageUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
       .flags = 0,
   };

   anv_image_create_info image_create_info = {.isl_tiling_flags = ISL_TILING_X_BIT,
                                              .stride = 0,
                                              .extended_usage = ANV_IMAGE_EXTENDED_USAGE_SCANOUT,
                                              .vk_info = &create_info};

   VkResult result;
   result = anv_image_create(anv_device_to_handle(device), &image_create_info, NULL, &image_h);

   if (result != VK_SUCCESS)
      return result;

   image = anv_image_from_handle(image_h);
   assert(vk_format_is_color(image->vk_format));

   VkMemoryAllocateInfo allocate_info = {
       .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
       .allocationSize = image->size,
       .memoryTypeIndex = 0,
   };

   VkDeviceMemory memory_h;
   struct anv_device_memory* memory;
   result = anv_AllocateMemory(anv_device_to_handle(device), &allocate_info,
                               NULL /* XXX: pAllocator */, &memory_h);
   if (result != VK_SUCCESS)
      goto fail_create_image;

   memory = anv_device_memory_from_handle(memory_h);
   memory->bo.is_winsys_bo = true;

   anv_BindImageMemory(VK_NULL_HANDLE, image_h, memory_h, 0);

   surface = &image->color_surface;
   assert(surface->isl.tiling == ISL_TILING_X);

   *row_pitch = surface->isl.row_pitch;
   *image_p = image_h;
   *memory_p = memory_h;
   *buffer_handle_p = image->bo->gem_handle;
   *size = image->size;
   *offset = image->offset;
   return VK_SUCCESS;

fail_create_image:
   anv_DestroyImage(device_h, image_h, pAllocator);
   return result;
}

void anv_wsi_magma_image_free(VkDevice device, const VkAllocationCallbacks* pAllocator,
                              VkImage image_h, VkDeviceMemory memory_h)
{
   anv_DestroyImage(device, image_h, pAllocator);

   anv_FreeMemory(device, memory_h, pAllocator);
}

uintptr_t anv_wsi_magma_get_platform_semaphore(VkSemaphore vk_semaphore)
{
   ANV_FROM_HANDLE(anv_semaphore, semaphore, vk_semaphore);
   return semaphore->current_platform_semaphore;
}
