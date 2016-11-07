// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "anv_wsi.h"
#include "vk_format_info.h"

#include "magma_system.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"

static const VkSurfaceFormatKHR formats[] = {
    {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
    },
};

static const VkPresentModeKHR present_modes[] = {
    VK_PRESENT_MODE_FIFO_KHR,
};

class MagmaImage {
public:
   static std::unique_ptr<MagmaImage> Create(anv_device* device,
                                             const VkSwapchainCreateInfoKHR* create_info);

   ~MagmaImage();

   bool busy() { return busy_; }

   void set_busy(bool busy) { busy_ = busy; }

   anv_image* image() { return image_; }

   uint32_t id()
   {
      assert(image_->bo);
      return image_->bo->gem_handle;
   }

private:
   MagmaImage(anv_device* device, anv_image* image);

   bool Alloc();

   anv_device* device_ = nullptr;
   anv_image* image_ = nullptr;
   anv_device_memory* device_memory_ = nullptr;
   bool busy_ = false;
};

std::unique_ptr<MagmaImage> MagmaImage::Create(anv_device* device,
                                               const VkSwapchainCreateInfoKHR* create_info)
{
   VkImageCreateInfo vkimage_create_info = {
       .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
       .imageType = VK_IMAGE_TYPE_2D,
       .format = create_info->imageFormat,
       .extent = {.width = create_info->imageExtent.width,
                  .height = create_info->imageExtent.height,
                  .depth = 1},
       .mipLevels = 1,
       .arrayLayers = 1,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .tiling = VK_IMAGE_TILING_LINEAR,
       .usage = (create_info->imageUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
       .flags = 0,
   };

   anv_image_create_info image_create_info = {
       .vk_info = &vkimage_create_info, .isl_tiling_flags = ISL_TILING_LINEAR, .stride = 0};

   VkImage vkimage;
   VkResult result =
       anv_image_create(anv_device_to_handle(device), &image_create_info, nullptr, &vkimage);
   if (result != VK_SUCCESS)
      return DRETP(nullptr, "anv_image_create failed: %d", result);

   auto magma_image =
       std::unique_ptr<MagmaImage>(new MagmaImage(device, anv_image_from_handle(vkimage)));

   if (!magma_image->Alloc())
      return DRETP(nullptr, "Aloc failed");

   return magma_image;
}

MagmaImage::MagmaImage(anv_device* device, anv_image* image) : device_(device), image_(image)
{
   assert(vk_format_is_color(image->vk_format));
}

MagmaImage::~MagmaImage()
{
   anv_DestroyImage(anv_device_to_handle(device_), anv_image_to_handle(image_), nullptr);

   if (device_memory_)
      anv_FreeMemory(anv_device_to_handle(device_), anv_device_memory_to_handle(device_memory_),
                     nullptr);
}

bool MagmaImage::Alloc()
{
   assert(image_);

   VkMemoryAllocateInfo memory_allocate_info = {
       .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
       .allocationSize = image_->size,
       .memoryTypeIndex = 0,
   };

   VkDeviceMemory vkdevicememory;
   VkResult result;

   result = anv_AllocateMemory(anv_device_to_handle(device_), &memory_allocate_info, nullptr,
                               &vkdevicememory);
   if (result != VK_SUCCESS)
      return DRETF(false, "anv_AllocateMemory failed: %d", result);

   device_memory_ = anv_device_memory_from_handle(vkdevicememory);

   result = anv_BindImageMemory(anv_device_to_handle(device_), anv_image_to_handle(image_),
                                vkdevicememory, 0);
   if (result != VK_SUCCESS)
      return DRETF(false, "anv_BindImageMemory failed: %d", result);

   anv_surface* surface = &image_->color_surface;
   assert(surface->isl.tiling == ISL_TILING_LINEAR);

   return true;
}

class MagmaSwapchain : public anv_swapchain {
public:
   MagmaSwapchain(anv_device* device, VkExtent2D extent) : extent_(extent)
   {
      // Default-initialize the anv_swapchain base
      anv_swapchain* base = static_cast<anv_swapchain*>(this);
      *base = {};

      // Make sure that did mostly what we expected
      assert(this->alloc.pUserData == 0);
      assert(this->fences[0] == VK_NULL_HANDLE);

      this->device = device;
      this->destroy = Destroy;
      this->get_images = GetImages;
      this->acquire_next_image = AcquireNextImage;
      this->queue_present = QueuePresent;
   }

   magma_system_connection* connection() { return device->connection; }

   uint32_t image_count() { return images_.size(); }

   MagmaImage* get_image(uint32_t index)
   {
      assert(index < images_.size());
      return images_[index].get();
   }

   void AddImage(std::unique_ptr<MagmaImage> image) { images_.push_back(std::move(image)); }

   static VkResult Destroy(anv_swapchain* anv_chain, const VkAllocationCallbacks* pAllocator)
   {
      DLOG("Destroy");
      MagmaSwapchain* chain = cast(anv_chain);
      delete chain;
      return VK_SUCCESS;
   }

   static VkResult GetImages(anv_swapchain* anv_chain, uint32_t* pCount, VkImage* pSwapchainImages)
   {
      DLOG("GetImages");
      MagmaSwapchain* chain = cast(anv_chain);

      if (!pSwapchainImages) {
         *pCount = chain->image_count();
         return VK_SUCCESS;
      }

      assert(chain->image_count() <= *pCount);

      for (uint32_t i = 0; i < chain->image_count(); i++)
         pSwapchainImages[i] = anv_image_to_handle(chain->get_image(i)->image());

      *pCount = chain->image_count();

      return VK_SUCCESS;
   }

   // Note - semaphore is supposed to be signalled when the image is no longer accessed by the
   // presentation engine.
   // Intel's mesa vulkan driver appears to ignore all semaphores (MA-110).
   static VkResult AcquireNextImage(anv_swapchain* anv_chain, uint64_t timeout,
                                    VkSemaphore semaphore, uint32_t* pImageIndex)
   {
      DLOG("AcquireNextImage");

      MagmaSwapchain* chain = cast(anv_chain);

      for (uint32_t i = 0; i < chain->image_count(); i++) {
         if (!chain->get_image(i)->busy()) {
            DLOG("returning image index %u", i);
            *pImageIndex = i;
            return VK_SUCCESS;
         }
      }

      // TODO(MA-114): wait until a buffer is available
      assert(timeout == 0);

      return VK_NOT_READY;
   }

   static void PageflipCallback(int32_t error, void* data)
   {
      DLOG("PageflipCallback (error %d)", error);

      MagmaImage* image = reinterpret_cast<MagmaImage*>(data);
      image->set_busy(false);
   }

   static VkResult QueuePresent(anv_swapchain* anv_chain, anv_queue* queue, uint32_t image_index)
   {
      DLOG("QueuePresent image_index %u", image_index);

      MagmaSwapchain* chain = cast(anv_chain);
      MagmaImage* image = chain->get_image(image_index);

      assert(!image->busy());

      image->set_busy(true);

      magma_system_display_page_flip(chain->connection(), image->id(), PageflipCallback, image);

      return VK_SUCCESS;
   }

private:
   static MagmaSwapchain* cast(anv_swapchain* anv_chain)
   {
      auto chain = static_cast<MagmaSwapchain*>(anv_chain);
      assert(chain->magic_ == kMagic);
      return chain;
   }

   static constexpr uint32_t kMagic = 0x6D617377; // 'masw'

   const uint32_t magic_ = kMagic;
   VkExtent2D extent_;
   std::vector<std::unique_ptr<MagmaImage>> images_;
};

static VkResult magma_surface_get_support(VkIcdSurfaceBase* icd_surface,
                                          struct anv_physical_device* device,
                                          uint32_t queueFamilyIndex, VkBool32* pSupported)
{
   DLOG("magma_surface_get_support queue %u", queueFamilyIndex);
   *pSupported = true;
   return VK_SUCCESS;
}

static VkResult magma_surface_get_capabilities(VkIcdSurfaceBase* icd_surface,
                                               struct anv_physical_device* device,
                                               VkSurfaceCapabilitiesKHR* caps)
{
   DLOG("magma_surface_get_capabilities");

   VkExtent2D extent = {2160, 1440};
   caps->currentExtent = extent;
   caps->minImageExtent = extent;
   caps->maxImageExtent = extent;

   caps->supportedCompositeAlpha =
       VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR | VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

   caps->minImageCount = 2;
   caps->maxImageCount = 3;
   caps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->maxImageArrayLayers = 1;
   caps->supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   return VK_SUCCESS;
}

static VkResult magma_surface_get_formats(VkIcdSurfaceBase* surface,
                                          struct anv_physical_device* device,
                                          uint32_t* pSurfaceFormatCount,
                                          VkSurfaceFormatKHR* pSurfaceFormats)
{
   DLOG("magma_surface_get_formats");

   if (pSurfaceFormats == NULL) {
      *pSurfaceFormatCount = ARRAY_SIZE(formats);
      return VK_SUCCESS;
   }

   assert(*pSurfaceFormatCount >= ARRAY_SIZE(formats));
   typed_memcpy(pSurfaceFormats, formats, *pSurfaceFormatCount);
   *pSurfaceFormatCount = ARRAY_SIZE(formats);

   return VK_SUCCESS;
}

static VkResult magma_surface_get_present_modes(VkIcdSurfaceBase* surface,
                                                anv_physical_device* device,
                                                uint32_t* pPresentModeCount,
                                                VkPresentModeKHR* pPresentModes)
{
   DLOG("magma_surface_get_present_modes");

   if (pPresentModes == NULL) {
      *pPresentModeCount = ARRAY_SIZE(present_modes);
      return VK_SUCCESS;
   }

   assert(*pPresentModeCount >= ARRAY_SIZE(present_modes));
   typed_memcpy(pPresentModes, present_modes, *pPresentModeCount);
   *pPresentModeCount = ARRAY_SIZE(present_modes);

   return VK_SUCCESS;
}

static VkResult magma_surface_create_swapchain(VkIcdSurfaceBase* icd_surface, anv_device* device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               anv_swapchain** swapchain_out)
{
   DLOG("magma_surface_create_swapchain");

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);

   // TODO(MA-115): use pAllocator here and for images (and elsewhere in magma?)
   auto chain = std::make_unique<MagmaSwapchain>(device, pCreateInfo->imageExtent);

   uint32_t num_images = pCreateInfo->minImageCount;

   for (uint32_t i = 0; i < num_images; i++) {
      std::unique_ptr<MagmaImage> image = MagmaImage::Create(device, pCreateInfo);
      if (!image)
         return VK_ERROR_OUT_OF_DEVICE_MEMORY;

      chain->AddImage(std::move(image));
   }

   *swapchain_out = chain.release();

   return VK_SUCCESS;
}

VkResult anv_magma_init_wsi(anv_physical_device* device)
{
   auto wsi = reinterpret_cast<anv_wsi_interface*>(anv_alloc(&device->instance->alloc,
                                                             sizeof(anv_wsi_interface), 8,
                                                             VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE));
   if (!wsi)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   wsi->get_support = magma_surface_get_support;
   wsi->get_capabilities = magma_surface_get_capabilities;
   wsi->get_formats = magma_surface_get_formats;
   wsi->get_present_modes = magma_surface_get_present_modes;
   wsi->create_swapchain = magma_surface_create_swapchain;

   device->wsi[VK_ICD_WSI_PLATFORM_MAGMA] = wsi;

   return VK_SUCCESS;
}

void anv_magma_finish_wsi(anv_physical_device* device)
{
   anv_wsi_interface* wsi = device->wsi[VK_ICD_WSI_PLATFORM_MAGMA];
   if (wsi)
      anv_free(&device->instance->alloc, wsi);
}

VkResult anv_CreateMagmaSurfaceKHR(VkInstance vkinstance,
                                   const VkMagmaSurfaceCreateInfoKHR* pCreateInfo,
                                   const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
   ANV_FROM_HANDLE(anv_instance, instance, vkinstance);

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_MAGMA_SURFACE_CREATE_INFO_KHR);

   auto surface = reinterpret_cast<VkIcdSurfaceMagma*>(
       anv_alloc2(&instance->alloc, pAllocator, sizeof(VkIcdSurfaceMagma), 8,
                  VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
   if (!surface)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   surface->base.platform = VK_ICD_WSI_PLATFORM_MAGMA;

   *pSurface = _VkIcdSurfaceBase_to_handle(&surface->base);

   return VK_SUCCESS;
}

VkBool32 anv_GetPhysicalDeviceMagmaPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                          uint32_t queueFamilyIndex,
                                                          xcb_connection_t* connection,
                                                          xcb_visualid_t visual_id)
{
   return VK_TRUE;
}
