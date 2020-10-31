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

#include "anv_magma.h"
#include "anv_private.h"
#include "magma_sysmem.h"
#include "vk_util.h"
#include "isl/isl.h"

#if VK_USE_PLATFORM_FUCHSIA

#define MAX_FORMAT_INDICES 128
const uint32_t kMaxFormatIndices = MAX_FORMAT_INDICES;

struct anv_buffer_collection {
   magma_buffer_collection_t buffer_collection;

   magma_sysmem_buffer_constraints_t constraints;

   uint32_t format_index_input_index_map[MAX_FORMAT_INDICES];
};

ANV_DEFINE_HANDLE_CASTS(anv_buffer_collection, VkBufferCollectionFUCHSIA)

VkResult anv_CreateBufferCollectionFUCHSIA(VkDevice vk_device,
                                           const VkBufferCollectionCreateInfoFUCHSIA* pCreateInfo,
                                           const VkAllocationCallbacks* pAllocator,
                                           VkBufferCollectionFUCHSIA* pCollection)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   magma_buffer_collection_t magma_buffer_collection;
   status = magma_buffer_collection_import(sysmem_connection, pCreateInfo->collectionToken,
                                           &magma_buffer_collection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_INVALID_EXTERNAL_HANDLE);

   struct anv_buffer_collection* buffer_collection =
       vk_alloc2(&device->alloc, pAllocator, sizeof(*buffer_collection), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

   buffer_collection->buffer_collection = magma_buffer_collection;
   buffer_collection->constraints = 0;

   *pCollection = anv_buffer_collection_to_handle(buffer_collection);

   return VK_SUCCESS;
}

void anv_DestroyBufferCollectionFUCHSIA(VkDevice vk_device, VkBufferCollectionFUCHSIA vk_collection,
                                        const VkAllocationCallbacks* pAllocator)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return;

   if (buffer_collection->constraints) {
     magma_buffer_constraints_release(sysmem_connection, buffer_collection->constraints);
   }

   magma_buffer_collection_release(sysmem_connection, buffer_collection->buffer_collection);
   vk_free2(&device->alloc, pAllocator, buffer_collection);
}

static VkResult get_image_format_constraints(
    VkDevice vk_device, VkFormat format, const VkImageCreateInfo* pImageInfo,
    magma_image_format_constraints_t* image_constraints_out, isl_tiling_flags_t isl_tiling_flags,
    VkImageFormatConstraintsInfoFUCHSIA* format_constraints)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);

   const struct anv_format_plane plane_format =
       anv_get_format_plane(&device->info, format, VK_IMAGE_ASPECT_COLOR_BIT, pImageInfo->tiling);

   const isl_surf_usage_flags_t isl_surf_usage =
       choose_isl_surf_usage(pImageInfo->flags, // vk_create_flags
                             pImageInfo->usage, // vk_usage
                             0,                 // isl_extra_usage
                             VK_IMAGE_ASPECT_COLOR_BIT);
   enum isl_surf_dim dim;
   switch (pImageInfo->imageType) {
   case VK_IMAGE_TYPE_1D:
      dim = ISL_SURF_DIM_1D;
      break;
   case VK_IMAGE_TYPE_2D:
      dim = ISL_SURF_DIM_2D;
      break;
   default:
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
   }

   struct isl_surf_init_info isl_surf_init_info = {
       .dim = dim,
       .format = plane_format.isl_format,
       .width = pImageInfo->extent.width / plane_format.denominator_scales[0],
       .height = pImageInfo->extent.height / plane_format.denominator_scales[1],
       .depth = pImageInfo->extent.depth,
       .levels = pImageInfo->mipLevels,
       .array_len = pImageInfo->arrayLayers,
       .samples = pImageInfo->samples,
       .min_alignment_B = 0,
       .row_pitch_B = 0,
       .usage = isl_surf_usage,
       .tiling_flags = isl_tiling_flags};

   struct isl_surf isl_surf;
   if (!isl_surf_init_s(&device->isl_dev, &isl_surf, &isl_surf_init_info)) {
      intel_loge("get_image_format_constraints: isl_surf_init_s failed");
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
   }

   assert(pImageInfo->extent.width);
   magma_image_format_constraints_t image_constraints = {.width = pImageInfo->extent.width,
                                                         .height = pImageInfo->extent.height,
                                                         .layers = 1,
                                                         .bytes_per_row_divisor = 1,
                                                         .min_bytes_per_row = isl_surf.row_pitch_B};

   switch (isl_surf.tiling) {
   case ISL_TILING_LINEAR:
      image_constraints.has_format_modifier = false;
      break;
   case ISL_TILING_X:
      image_constraints.has_format_modifier = true;
      image_constraints.format_modifier = MAGMA_FORMAT_MODIFIER_INTEL_X_TILED;
      break;
   case ISL_TILING_Y0:
      image_constraints.has_format_modifier = true;
      image_constraints.format_modifier = MAGMA_FORMAT_MODIFIER_INTEL_Y_TILED;
      break;
   case ISL_TILING_Yf:
      image_constraints.has_format_modifier = true;
      image_constraints.format_modifier = MAGMA_FORMAT_MODIFIER_INTEL_YF_TILED;
      break;
   default:
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
   }

   switch (format) {
   case VK_FORMAT_B8G8R8A8_SINT:
   case VK_FORMAT_B8G8R8A8_UNORM:
   case VK_FORMAT_B8G8R8A8_SRGB:
   case VK_FORMAT_B8G8R8A8_SNORM:
   case VK_FORMAT_B8G8R8A8_SSCALED:
   case VK_FORMAT_B8G8R8A8_USCALED:
      image_constraints.image_format = MAGMA_FORMAT_BGRA32;
      break;
   case VK_FORMAT_R8G8B8A8_SINT:
   case VK_FORMAT_R8G8B8A8_UNORM:
   case VK_FORMAT_R8G8B8A8_SRGB:
   case VK_FORMAT_R8G8B8A8_SNORM:
   case VK_FORMAT_R8G8B8A8_SSCALED:
   case VK_FORMAT_R8G8B8A8_USCALED:
      image_constraints.image_format = MAGMA_FORMAT_R8G8B8A8;
      break;
   case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      image_constraints.image_format = MAGMA_FORMAT_NV12;
      break;
   case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
      image_constraints.image_format = MAGMA_FORMAT_I420;
      break;
   case VK_FORMAT_R8_UNORM:
      image_constraints.image_format = MAGMA_FORMAT_R8;
      if (format_constraints && format_constraints->sysmemFormat) {
         if (format_constraints->sysmemFormat == MAGMA_FORMAT_L8) {
            image_constraints.image_format = MAGMA_FORMAT_L8;
         } else if (format_constraints->sysmemFormat != MAGMA_FORMAT_R8) {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
         }
      }
      break;
   case VK_FORMAT_R8G8_UNORM:
      image_constraints.image_format = MAGMA_FORMAT_R8G8;
      break;
   default:
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
   }

   *image_constraints_out = image_constraints;

   return VK_SUCCESS;
}

VkResult anv_SetBufferCollectionConstraintsFUCHSIA(VkDevice vk_device,
                                                   VkBufferCollectionFUCHSIA vk_collection,
                                                   const VkImageCreateInfo* pImageInfo)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   uint32_t slot_count = 0;
   VkResult result;

   const VkFormat kDefaultFormatList[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                                          VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                                          VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM};
   magma_image_format_constraints_t image_constraints[3 * ARRAY_SIZE(kDefaultFormatList)];

   // Sysmem is currently limited to a maximum of 32 image constraints.
   assert(ARRAY_SIZE(image_constraints) <= 32);

   const VkFormat* format_list_to_try = &pImageInfo->format;
   uint32_t num_formats_to_try = 1;

   if (pImageInfo->format == VK_FORMAT_UNDEFINED) {
      format_list_to_try = kDefaultFormatList;
      num_formats_to_try = ARRAY_SIZE(kDefaultFormatList);
   }

   for (uint32_t i = 0; i < num_formats_to_try; ++i) {
      VkFormat format = format_list_to_try[i];
      assert(slot_count < ARRAY_SIZE(image_constraints));
#ifndef NDEBUG
      const struct anv_physical_device* physical_device = device->physical;
      const struct gen_device_info* devinfo = &physical_device->info;
      const struct anv_format* anv_format = anv_get_format(format);
      VkFormatFeatureFlags linear_flags =
          anv_get_image_format_features(devinfo, format, anv_format, VK_IMAGE_TILING_LINEAR);
      VkFormatFeatureFlags optimal_flags =
          anv_get_image_format_features(devinfo, format, anv_format, VK_IMAGE_TILING_OPTIMAL);
      // We always include linear even if optimal is requested so that optimal is compatible with
      // as many sysmem formats as possible. Check that linear formats support everything that
      // optimal formats do.
      assert(!(optimal_flags & ~linear_flags));
#endif
      result =
          get_image_format_constraints(vk_device, format, pImageInfo,
                                       &image_constraints[slot_count], ISL_TILING_LINEAR_BIT, NULL);
      if (result != VK_SUCCESS) {
         return result;
      }
      slot_count++;
      // Sysmem can't handle tiled YUV, so don't attempt it.
      if (pImageInfo->tiling != VK_IMAGE_TILING_LINEAR &&
          image_constraints[slot_count - 1].image_format != MAGMA_FORMAT_NV12 &&
          image_constraints[slot_count - 1].image_format != MAGMA_FORMAT_I420) {
         // We always support X tiled for scanout but there may be a more optimal tiling format.
         result = get_image_format_constraints(
             vk_device, format, pImageInfo, &image_constraints[slot_count], ISL_TILING_X_BIT, NULL);
         if (result == VK_SUCCESS) {
            assert(image_constraints[slot_count].has_format_modifier);
            slot_count++;
            assert(slot_count < ARRAY_SIZE(image_constraints));
            result = get_image_format_constraints(vk_device, format, pImageInfo,
                                                  &image_constraints[slot_count],
                                                  ISL_TILING_ANY_MASK, NULL);
            if (result == VK_SUCCESS) {
               assert(image_constraints[slot_count].has_format_modifier);
               if (image_constraints[slot_count].format_modifier !=
                   image_constraints[slot_count - 1].format_modifier) {
                  slot_count++;
               }
            }
         }
      }

      if (result != VK_SUCCESS)
         return result;
   }

   if (slot_count == 0)
      return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);

   magma_buffer_format_constraints_t format_constraints = {.count = 1,
                                                           .usage = 0,
                                                           .secure_permitted = false,
                                                           .secure_required = false,
                                                           .ram_domain_supported = true,
                                                           .cpu_domain_supported = true,
                                                           .min_size_bytes = 0};

   magma_sysmem_buffer_constraints_t constraints;
   status = magma_buffer_constraints_create(sysmem_connection, &format_constraints, &constraints);
   if (status != MAGMA_STATUS_OK)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   for (uint32_t slot = 0; slot < slot_count; slot++) {
      assert(slot < sizeof(image_constraints) / sizeof(image_constraints[0]));
      status = magma_buffer_constraints_set_format(sysmem_connection, constraints, slot,
                                                   &image_constraints[slot]);
      if (status != MAGMA_STATUS_OK) {
         break;
      }
   }

   if (status == MAGMA_STATUS_OK) {
      status = magma_buffer_collection_set_constraints(
          sysmem_connection, buffer_collection->buffer_collection, constraints);
   }

   magma_buffer_constraints_release(sysmem_connection, constraints);

   if (status != MAGMA_STATUS_OK)
      return VK_ERROR_FORMAT_NOT_SUPPORTED;

   return VK_SUCCESS;
}

VkResult
anv_SetBufferCollectionBufferConstraintsFUCHSIA(VkDevice vk_device,
                                                VkBufferCollectionFUCHSIA vk_collection,
                                                const VkBufferConstraintsInfoFUCHSIA* pConstraints)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   magma_buffer_format_constraints_t format_constraints = {
       .count = pConstraints->minCount,
       .usage = 0,
       .secure_permitted = false,
       .secure_required = false,
       .ram_domain_supported = true,
       .cpu_domain_supported = true,
       .min_size_bytes = pConstraints->pBufferCreateInfo->size};

   magma_sysmem_buffer_constraints_t constraints;
   status = magma_buffer_constraints_create(sysmem_connection, &format_constraints, &constraints);
   if (status != MAGMA_STATUS_OK)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   status = magma_buffer_collection_set_constraints(
       sysmem_connection, buffer_collection->buffer_collection, constraints);

   magma_buffer_constraints_release(sysmem_connection, constraints);

   if (status != MAGMA_STATUS_OK)
      return VK_ERROR_FORMAT_NOT_SUPPORTED;

   return VK_SUCCESS;
}

VkResult anv_GetBufferCollectionPropertiesFUCHSIA(VkDevice vk_device,
                                                  VkBufferCollectionFUCHSIA vk_collection,
                                                  VkBufferCollectionPropertiesFUCHSIA* pProperties)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);
   magma_buffer_format_description_t description;
   status = magma_sysmem_get_description_from_collection(
       sysmem_connection, buffer_collection->buffer_collection, &description);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   status = magma_get_buffer_count(description, &pProperties->count);
   magma_buffer_format_description_release(description);

   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   struct anv_physical_device* pdevice = device->physical;
   // All memory types supported.
   pProperties->memoryTypeBits = (1ull << pdevice->memory.type_count) - 1;
   return VK_SUCCESS;
}

VkFormat sysmem_to_vk_format(uint32_t sysmem_format)
{
   switch (sysmem_format) {
   case MAGMA_FORMAT_BGRA32:
      return VK_FORMAT_B8G8R8A8_UNORM;
   case MAGMA_FORMAT_R8G8B8A8:
      return VK_FORMAT_R8G8B8A8_UNORM;
   case MAGMA_FORMAT_NV12:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   case MAGMA_FORMAT_I420:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
   case MAGMA_FORMAT_L8:
   case MAGMA_FORMAT_R8:
      return VK_FORMAT_R8_UNORM;
   case MAGMA_FORMAT_R8G8:
      return VK_FORMAT_R8G8_UNORM;
   default:
      return VK_FORMAT_UNDEFINED;
   }
}

VkResult
anv_GetBufferCollectionProperties2FUCHSIA(VkDevice vk_device,
                                          VkBufferCollectionFUCHSIA vk_collection,
                                          VkBufferCollectionProperties2FUCHSIA* pProperties)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   magma_buffer_format_description_t description;
   status = magma_sysmem_get_description_from_collection(
       sysmem_connection, buffer_collection->buffer_collection, &description);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);

   uint32_t count;
   status = magma_get_buffer_count(description, &count);
   if (status != MAGMA_STATUS_OK) {
      magma_buffer_format_description_release(description);
      return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   uint32_t sysmem_format;
   status = magma_get_buffer_format(description, &sysmem_format);
   if (status != MAGMA_STATUS_OK) {
      magma_buffer_format_description_release(description);
      return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   pProperties->sysmemFormat = sysmem_format;

   magma_bool_t has_format_modifier;
   uint64_t format_modifier = 0;
   status = magma_get_buffer_format_modifier(description, &has_format_modifier, &format_modifier);
   if (status != MAGMA_STATUS_OK) {
      magma_buffer_format_description_release(description);
      return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   uint32_t color_space = MAGMA_COLORSPACE_INVALID;
   magma_get_buffer_color_space(description, &color_space);
   // Colorspace may be invalid for non-images, so ignore error.

   pProperties->colorSpace.colorSpace = color_space;
   pProperties->samplerYcbcrConversionComponents.r = VK_COMPONENT_SWIZZLE_IDENTITY;
   pProperties->samplerYcbcrConversionComponents.g = VK_COMPONENT_SWIZZLE_IDENTITY;
   pProperties->samplerYcbcrConversionComponents.b = VK_COMPONENT_SWIZZLE_IDENTITY;
   pProperties->samplerYcbcrConversionComponents.a = VK_COMPONENT_SWIZZLE_IDENTITY;

   pProperties->suggestedYcbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;
   pProperties->suggestedXChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN;
   pProperties->suggestedYChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;

   switch (color_space) {
   case MAGMA_COLORSPACE_REC601_NTSC:
   case MAGMA_COLORSPACE_REC601_PAL:
      pProperties->suggestedYcbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;
      break;

   case MAGMA_COLORSPACE_REC601_NTSC_FULL_RANGE:
   case MAGMA_COLORSPACE_REC601_PAL_FULL_RANGE:
      pProperties->suggestedYcbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;
      pProperties->suggestedYcbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
      break;

   case MAGMA_COLORSPACE_REC709:
      pProperties->suggestedYcbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;
      break;

   case MAGMA_COLORSPACE_REC2020:
      pProperties->suggestedYcbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020;
      break;

   case MAGMA_COLORSPACE_SRGB:
      pProperties->suggestedYcbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
      pProperties->suggestedYcbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
      break;

   default:
      pProperties->suggestedYcbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
      pProperties->suggestedYcbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
      break;
   }

   pProperties->createInfoIndex = 0;

   if (buffer_collection->constraints) {
      magma_bool_t format_valid[kMaxFormatIndices];
      status = magma_get_description_format_index(sysmem_connection, description,
                                                  buffer_collection->constraints, format_valid,
                                                  kMaxFormatIndices);
      if (status != MAGMA_STATUS_OK) {
         magma_buffer_format_description_release(description);
         return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
      }

      // Choose the first valid format for now.
      for (uint32_t i = 0; i < kMaxFormatIndices; i++) {
         if (format_valid[i]) {
            pProperties->createInfoIndex = buffer_collection->format_index_input_index_map[i];
            break;
         }
      }
   }

   if (!has_format_modifier) {
      format_modifier = MAGMA_FORMAT_MODIFIER_LINEAR;
   }

   {
      VkFormat format = sysmem_to_vk_format(sysmem_format);
      const struct anv_physical_device* physical_device = device->physical;
      const struct gen_device_info* devinfo = &physical_device->info;
      const struct anv_format* anv_format = anv_get_format(format);
      if (has_format_modifier) {
         pProperties->formatFeatures =
             anv_get_image_format_features(devinfo, format, anv_format, VK_IMAGE_TILING_OPTIMAL);
      } else {
         pProperties->formatFeatures =
             anv_get_image_format_features(devinfo, format, anv_format, VK_IMAGE_TILING_LINEAR);
      }
   }

   magma_buffer_format_description_release(description);

   pProperties->bufferCount = count;

   if (pProperties->bufferCount < 1) {
      pProperties->memoryTypeBits = 0u;
   } else {
      struct anv_physical_device* pdevice = device->physical;
      // All memory types supported.
      pProperties->memoryTypeBits = (1ull << pdevice->memory.type_count) - 1;
   }

   return VK_SUCCESS;
}

VkResult anv_SetBufferCollectionImageConstraintsFUCHSIA(
    VkDevice vk_device, VkBufferCollectionFUCHSIA vk_collection,
    const VkImageConstraintsInfoFUCHSIA* pImageConstraintsInfo)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, collection, vk_collection);

   // Can't set constraints twice.
   if (collection->constraints)
      return ANV_MAGMA_DRET(VK_ERROR_INITIALIZATION_FAILED);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);

   if (pImageConstraintsInfo->createInfoCount < 1) {
      assert(!(pImageConstraintsInfo->createInfoCount < 1));
      return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);
   }

   const bool have_format_constraints = (pImageConstraintsInfo->pFormatConstraints != NULL);

   // Secure formats not supported.
   for (uint32_t i = 0; i < pImageConstraintsInfo->createInfoCount; ++i) {
      bool secure_required =
          (pImageConstraintsInfo->pCreateInfos[i].flags & VK_IMAGE_CREATE_PROTECTED_BIT);

      const VkImageFormatConstraintsInfoFUCHSIA* format_constraints =
          have_format_constraints ? &pImageConstraintsInfo->pFormatConstraints[i] : NULL;

      bool this_secure_optional =
          have_format_constraints && (pImageConstraintsInfo->pFormatConstraints[i].flags &
                                      VK_IMAGE_FORMAT_CONSTRAINTS_PROTECTED_OPTIONAL_FUCHSIA);

      if (secure_required || this_secure_optional) {
         assert(!(secure_required || this_secure_optional));
         return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);
      }
   }

   magma_sysmem_buffer_constraints_t constraints;

   // Create the buffer constraints.
   {
      magma_buffer_format_constraints_t format_constraints = {
          .count = pImageConstraintsInfo->minBufferCount,
          .usage = 0,
          .secure_permitted = false,
          .secure_required = false,
          .ram_domain_supported = true,
          .cpu_domain_supported = true,
          .min_size_bytes = 0,
      };

      status =
          magma_buffer_constraints_create(sysmem_connection, &format_constraints, &constraints);
      if (status != MAGMA_STATUS_OK)
         return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   // Add additional constraints.
   {
      magma_buffer_format_additional_constraints_t additional = {
          .min_buffer_count_for_camping = pImageConstraintsInfo->minBufferCountForCamping,
          .min_buffer_count_for_shared_slack = pImageConstraintsInfo->minBufferCountForSharedSlack,
          .min_buffer_count_for_dedicated_slack =
              pImageConstraintsInfo->minBufferCountForDedicatedSlack,
          .max_buffer_count = pImageConstraintsInfo->maxBufferCount};

      status = magma_buffer_constraints_add_additional(sysmem_connection, constraints, &additional);
      if (status != MAGMA_STATUS_OK) {
         magma_buffer_constraints_release(sysmem_connection, constraints);
         return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
      }
   }

   uint32_t format_index = 0;

   // Set format slots for each image info.
   for (uint32_t i = 0; i < pImageConstraintsInfo->createInfoCount; ++i) {
      const VkImageCreateInfo* pCreateInfo = &pImageConstraintsInfo->pCreateInfos[i];
      VkFormat format = pCreateInfo->format;

      const struct anv_physical_device* physical_device = device->physical;
      const struct gen_device_info* devinfo = &physical_device->info;
      const struct anv_format* anv_format = anv_get_format(format);
      const VkFormatFeatureFlags linear_flags =
          anv_get_image_format_features(devinfo, format, anv_format, VK_IMAGE_TILING_LINEAR);
      const VkFormatFeatureFlags optimal_flags =
          anv_get_image_format_features(devinfo, format, anv_format, VK_IMAGE_TILING_OPTIMAL);

      const VkImageFormatConstraintsInfoFUCHSIA* format_constraints =
          have_format_constraints ? &pImageConstraintsInfo->pFormatConstraints[i] : NULL;

      const uint32_t colorSpaceCount = format_constraints ? format_constraints->colorSpaceCount : 0;
      uint32_t color_spaces[colorSpaceCount];

      for (uint32_t j = 0; j < colorSpaceCount; ++j) {
         color_spaces[j] = format_constraints->pColorSpaces[j].colorSpace;
      }

      enum { SLOT0_LINEAR, SLOT1_X_TILED, SLOT2_TILED };
      const uint32_t kSlotCount = 3;
      magma_image_format_constraints_t image_constraints[kSlotCount];
      bool image_constraints_valid[kSlotCount];

      status = get_image_format_constraints(vk_device, format, pCreateInfo,
                                            &image_constraints[SLOT0_LINEAR], ISL_TILING_LINEAR_BIT,
                                            format_constraints);
      if (status != VK_SUCCESS)
         continue;

      // Sysmem can't handle certain formats tiled, so don't attempt it.
      bool skip_optimal = (pCreateInfo->tiling == VK_IMAGE_TILING_LINEAR) ||
                          (image_constraints[SLOT0_LINEAR].image_format == MAGMA_FORMAT_NV12) ||
                          (image_constraints[SLOT0_LINEAR].image_format == MAGMA_FORMAT_I420) ||
                          (image_constraints[SLOT0_LINEAR].image_format == MAGMA_FORMAT_L8) ||
                          (image_constraints[SLOT0_LINEAR].image_format == MAGMA_FORMAT_R8) ||
                          (image_constraints[SLOT0_LINEAR].image_format == MAGMA_FORMAT_R8G8);

      image_constraints_valid[SLOT0_LINEAR] =
          !format_constraints || !(~linear_flags & format_constraints->requiredFormatFeatures);

      if (skip_optimal) {
         image_constraints_valid[SLOT1_X_TILED] = false;
      } else {
         image_constraints_valid[SLOT1_X_TILED] =
             !format_constraints || !(~optimal_flags & format_constraints->requiredFormatFeatures);
      }

      image_constraints_valid[SLOT2_TILED] = image_constraints_valid[SLOT1_X_TILED];

      for (uint32_t slot = 0; slot < kSlotCount; slot++) {
         if (!image_constraints_valid[slot])
            continue;

         switch (slot) {
         case SLOT0_LINEAR:
            // Image constraints already initialized.
            break;
         case SLOT1_X_TILED:
            status = get_image_format_constraints(vk_device, format, pCreateInfo,
                                                  &image_constraints[SLOT1_X_TILED],
                                                  ISL_TILING_X_BIT, format_constraints);
            break;
         case SLOT2_TILED:
            status = get_image_format_constraints(vk_device, format, pCreateInfo,
                                                  &image_constraints[SLOT2_TILED],
                                                  ISL_TILING_ANY_MASK, format_constraints);
            break;
         default:
            assert(false);
         }

         if (status != VK_SUCCESS)
            continue;

         // Currently every vulkan format maps to only 1 sysmem format, so ensure the client is
         // using the same format.
         if (format_constraints && format_constraints->sysmemFormat &&
             (format_constraints->sysmemFormat != image_constraints[slot].image_format)) {
            continue;
         }

         collection->format_index_input_index_map[format_index] = i;

         status = magma_buffer_constraints_set_format(sysmem_connection, constraints, format_index,
                                                      &image_constraints[slot]);
         if (status != MAGMA_STATUS_OK) {
            magma_buffer_constraints_release(sysmem_connection, constraints);
            return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);
         }

         if (colorSpaceCount) {
            magma_buffer_constraints_set_colorspaces(sysmem_connection, constraints, format_index,
                                                     colorSpaceCount, color_spaces);
         }

         format_index += 1;
         if (format_index >= kMaxFormatIndices) {
            assert(!(format_index >= kMaxFormatIndices));
            magma_buffer_constraints_release(sysmem_connection, constraints);
            return ANV_MAGMA_DRET(VK_ERROR_OUT_OF_HOST_MEMORY);
         }
      }
   }

   if (format_index == 0) {
      magma_buffer_constraints_release(sysmem_connection, constraints);
      return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);
   }

   status = magma_buffer_collection_set_constraints(sysmem_connection,
                                                    collection->buffer_collection, constraints);
   if (status != MAGMA_STATUS_OK) {
      magma_buffer_constraints_release(sysmem_connection, constraints);
      return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);
   }

   collection->constraints = constraints;

   return VK_SUCCESS;
}

// Takes ownership of the buffer format description.
static VkResult anv_image_params_from_description(
    magma_buffer_format_description_t description, uint32_t width, uint32_t height,
    struct anv_fuchsia_image_plane_params params_out[MAGMA_MAX_IMAGE_PLANES],
    isl_tiling_flags_t* tiling_flags_out, bool* not_cache_coherent_out)
{
   magma_bool_t has_format_modifier;
   uint64_t format_modifier;
   magma_status_t status = MAGMA_STATUS_OK;
   if (params_out) {
      magma_image_plane_t planes[MAGMA_MAX_IMAGE_PLANES];

      status = magma_get_buffer_format_plane_info_with_size(description, width, height, planes);
      if (status == MAGMA_STATUS_OK) {
         for (uint32_t i = 0; i < MAGMA_MAX_IMAGE_PLANES; i++) {
            params_out[i].bytes_per_row = planes[i].bytes_per_row;
            params_out[i].byte_offset = planes[i].byte_offset;
         }
      }
   }
   uint32_t coherency_domain;
   if (status == MAGMA_STATUS_OK) {
      status = magma_get_buffer_coherency_domain(description, &coherency_domain);
   }
   if (status == MAGMA_STATUS_OK) {
      status =
          magma_get_buffer_format_modifier(description, &has_format_modifier, &format_modifier);
   }

   magma_buffer_format_description_release(description);

   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_FORMAT_NOT_SUPPORTED);

   if (not_cache_coherent_out) {
      *not_cache_coherent_out = coherency_domain == MAGMA_COHERENCY_DOMAIN_RAM;
   }

   if (tiling_flags_out) {
      *tiling_flags_out = ISL_TILING_LINEAR_BIT;

      if (has_format_modifier) {
         switch (format_modifier) {
         case MAGMA_FORMAT_MODIFIER_INTEL_X_TILED:
            *tiling_flags_out = ISL_TILING_X_BIT;
            break;
         case MAGMA_FORMAT_MODIFIER_INTEL_Y_TILED:
            *tiling_flags_out = ISL_TILING_Y0_BIT;
            break;
         case MAGMA_FORMAT_MODIFIER_INTEL_YF_TILED:
            *tiling_flags_out = ISL_TILING_Yf_BIT;
            break;
         case MAGMA_FORMAT_MODIFIER_LINEAR:
            break;
         default:
            assert(false);
         }
      }
   }

   return VK_SUCCESS;
}

VkResult anv_image_params_from_buffer_collection(
    VkDevice vk_device, VkBufferCollectionFUCHSIA vk_collection, const VkExtent3D* extent,
    struct anv_fuchsia_image_plane_params params_out[MAGMA_MAX_IMAGE_PLANES],
    isl_tiling_flags_t* tiling_flags_out, bool* not_cache_coherent_out)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);
   magma_buffer_format_description_t description;
   status = magma_sysmem_get_description_from_collection(
       sysmem_connection, buffer_collection->buffer_collection, &description);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);
   uint32_t width = extent ? extent->width : 0u;
   uint32_t height = extent ? extent->height : 0;

   return anv_image_params_from_description(description, width, height, params_out,
                                            tiling_flags_out, not_cache_coherent_out);
}

VkResult anv_get_buffer_collection_handle(struct anv_device* device,
                                          VkBufferCollectionFUCHSIA vk_collection, uint32_t index,
                                          uint32_t* handle_out, uint32_t* offset_out)
{
   ANV_FROM_HANDLE(anv_buffer_collection, buffer_collection, vk_collection);

   magma_sysmem_connection_t sysmem_connection;
   magma_status_t status = AnvMagmaGetSysmemConnection(device->connection, &sysmem_connection);
   if (status != MAGMA_STATUS_OK)
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);
   if (magma_sysmem_get_buffer_handle_from_collection(sysmem_connection,
                                                      buffer_collection->buffer_collection, index,
                                                      handle_out, offset_out) != MAGMA_STATUS_OK) {
      return ANV_MAGMA_DRET(VK_ERROR_DEVICE_LOST);
   }
   return VK_SUCCESS;
}

#endif // VK_USE_PLATFORM_FUCHSIA
