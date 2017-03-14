// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "util/macros.h"
#include "wsi_common_magma.h"

#include "magma_system.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "apps/tracing/lib/trace/event.h"
#include "platform_trace.h"

#define typed_memcpy(dest, src, count)                                                             \
   ({                                                                                              \
      static_assert(sizeof(*src) == sizeof(*dest), "");                                            \
      memcpy((dest), (src), (count) * sizeof(*(src)));                                             \
   })

static const VkSurfaceFormatKHR formats[] = {
    {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
    },
};

static const VkPresentModeKHR present_modes[] = {
    VK_PRESENT_MODE_FIFO_KHR,
};

class WsiMagma : public wsi_interface {
public:
   WsiMagma(const wsi_magma_callbacks* callbacks) : callbacks_(callbacks) {}

   magma_system_connection* connection(VkDevice device)
   {
      return reinterpret_cast<magma_system_connection*>(callbacks_->get_magma_connection(device));
   }

   const wsi_magma_callbacks* callbacks() { return callbacks_; }

private:
   const wsi_magma_callbacks* callbacks_;
};

//////////////////////////////////////////////////////////////////////////////

class MagmaImage {
public:
   static std::unique_ptr<MagmaImage> Create(VkDevice device, const wsi_magma_callbacks* callbacks,
                                             magma_system_connection* connection,
                                             const VkSwapchainCreateInfoKHR* create_info,
                                             const VkAllocationCallbacks* pAllocator);

   ~MagmaImage() { callbacks_->free_wsi_image(device_, allocator_, image_, device_memory_); }

   magma_buffer_t buffer_handle() { return buffer_handle_; }

   VkImage image() { return image_; }

   magma_semaphore_t semaphore() { return semaphore_; }

private:
   MagmaImage(VkDevice device, const wsi_magma_callbacks* callbacks,
              const VkAllocationCallbacks* allocator, magma_buffer_t buffer_handle, VkImage image,
              VkDeviceMemory device_memory, magma_semaphore_t semaphore)
       : device_(device), allocator_(allocator), callbacks_(callbacks),
         buffer_handle_(buffer_handle), image_(image), device_memory_(device_memory),
         semaphore_(semaphore)
   {
   }

   VkDevice device_;
   const VkAllocationCallbacks* allocator_;
   const wsi_magma_callbacks* callbacks_;
   magma_buffer_t buffer_handle_;
   VkImage image_;
   VkDeviceMemory device_memory_;
   magma_semaphore_t semaphore_;
};

std::unique_ptr<MagmaImage> MagmaImage::Create(VkDevice device,
                                               const wsi_magma_callbacks* callbacks,
                                               magma_system_connection* connection,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* allocator)
{
   VkResult result;
   uint32_t row_pitch;
   uint32_t offset;
   uint32_t bpp = 32;
   magma_buffer_t buffer_handle;
   uint32_t size;
   VkImage image;
   VkDeviceMemory device_memory;

   result = callbacks->create_wsi_image(device, pCreateInfo, allocator, &image, &device_memory,
                                        &size, &offset, &row_pitch, &buffer_handle);
   if (result != VK_SUCCESS)
      return DRETP(nullptr, "create_wsi_image failed");

   magma_semaphore_t semaphore;
   magma_status_t status = magma_system_create_semaphore(connection, &semaphore);
   if (status != MAGMA_STATUS_OK)
      return DRETP(nullptr, "failed to create semaphore");

   magma_system_signal_semaphore(semaphore);

   auto magma_image = std::unique_ptr<MagmaImage>(new MagmaImage(
       device, callbacks, allocator, buffer_handle, image, device_memory, semaphore));

   return magma_image;
}

//////////////////////////////////////////////////////////////////////////////

class MagmaSwapchain : public wsi_swapchain {
public:
   MagmaSwapchain(VkDevice device, magma_system_connection* connection) : connection_(connection)
   {
      // Default-initialize the anv_swapchain base
      wsi_swapchain* base = static_cast<wsi_swapchain*>(this);
      *base = {};

      // Make sure that did mostly what we expected
      assert(this->alloc.pUserData == 0);
      assert(this->fences[0] == VK_NULL_HANDLE);

      this->device = device;
      this->destroy = Destroy;
      this->get_images = GetImages;
      this->acquire_next_image = AcquireNextImage;
      this->queue_present = QueuePresent;

      magma::PlatformTrace::GetInstance();
   }

   magma_system_connection* connection() { return connection_; }

   uint32_t image_count() { return images_.size(); }

   MagmaImage* get_image(uint32_t index)
   {
      assert(index < images_.size());
      return images_[index].get();
   }

   void AddImage(std::unique_ptr<MagmaImage> image) { images_.push_back(std::move(image)); }

   static VkResult Destroy(wsi_swapchain* wsi_chain, const VkAllocationCallbacks* pAllocator)
   {
      DLOG("Destroy");
      MagmaSwapchain* chain = cast(wsi_chain);
      delete chain;
      return VK_SUCCESS;
   }

   static VkResult GetImages(wsi_swapchain* wsi_chain, uint32_t* pCount, VkImage* pSwapchainImages)
   {
      DLOG("GetImages");
      MagmaSwapchain* chain = cast(wsi_chain);

      if (!pSwapchainImages) {
         *pCount = chain->image_count();
         return VK_SUCCESS;
      }

      assert(chain->image_count() <= *pCount);

      for (uint32_t i = 0; i < chain->image_count(); i++)
         pSwapchainImages[i] = chain->get_image(i)->image();

      *pCount = chain->image_count();

      return VK_SUCCESS;
   }

   // Note - semaphore is supposed to be signalled when the image is no longer accessed by the
   // presentation engine.
   // Intel's mesa vulkan driver appears to ignore all semaphores (MA-110).
   static VkResult AcquireNextImage(wsi_swapchain* wsi_chain, uint64_t timeout,
                                    VkSemaphore semaphore, uint32_t* pImageIndex)
   {
      MagmaSwapchain* chain = cast(wsi_chain);

      uint32_t index = chain->next_index_;
      MagmaImage* image = chain->get_image(index);
      auto id = magma_system_get_semaphore_id(image->semaphore());
      DLOG("AcquireNextImage semaphore id 0x%" PRIx64, id);


      uint64_t nonce = TRACE_NONCE();
      TRACE_ASYNC_BEGIN("magma", "AcquireNextImage Wait", nonce);

      magma_status_t status = magma_system_wait_semaphore(image->semaphore(), timeout);
      if (status == MAGMA_STATUS_TIMED_OUT) {
         DLOG("timeout waiting for image semaphore");
         return VK_TIMEOUT;
      }
      TRACE_ASYNC_END("magma", "AcquireNextImage Wait", nonce);

      assert(status == MAGMA_STATUS_OK);

      if (++chain->next_index_ >= chain->image_count())
         chain->next_index_ = 0;

      *pImageIndex = index;
      DLOG("AcquireNextImage returning index %u id 0x%" PRIx64, *pImageIndex,
           magma_system_get_buffer_id(image->buffer_handle()));

      return VK_SUCCESS;
   }

   static VkResult QueuePresent(wsi_swapchain* swapchain, uint32_t image_index,
                                uint32_t wait_semaphore_count, const VkSemaphore* wait_semaphores)
   {
      MagmaSwapchain* magma_swapchain = cast(swapchain);
      MagmaImage* image = magma_swapchain->get_image(image_index);

      DLOG("QueuePresent image_index %u id 0x%" PRIx64, image_index,
           magma_system_get_buffer_id(image->buffer_handle()));

      magma_semaphore_t signal_semaphores[1]{image->semaphore()};
      magma_system_display_page_flip(
          magma_swapchain->connection(), image->buffer_handle(), wait_semaphore_count,
          reinterpret_cast<const magma_semaphore_t*>(wait_semaphores), 1, signal_semaphores);

      return VK_SUCCESS;
   }

private:
   static MagmaSwapchain* cast(wsi_swapchain* swapchain)
   {
      auto magma_swapchain = static_cast<MagmaSwapchain*>(swapchain);
      assert(magma_swapchain->magic_ == kMagic);
      return magma_swapchain;
   }

   static constexpr uint32_t kMagic = 0x6D617377; // 'masw'

   const uint32_t magic_ = kMagic;
   magma_system_connection* connection_;
   std::vector<std::unique_ptr<MagmaImage>> images_;
   uint32_t next_index_ = 0;
};

static VkResult magma_surface_get_support(VkIcdSurfaceBase* icd_surface,
                                          struct wsi_device* wsi_device,
                                          const VkAllocationCallbacks* alloc,
                                          uint32_t queueFamilyIndex, VkBool32* pSupported)
{
   DLOG("magma_surface_get_support queue %u", queueFamilyIndex);
   *pSupported = true;
   return VK_SUCCESS;
}

static VkResult magma_surface_get_capabilities(VkIcdSurfaceBase* icd_surface,
                                               VkSurfaceCapabilitiesKHR* caps)
{
   DLOG("magma_surface_get_capabilities");

   VkExtent2D any_extent = {0xFFFFFFFF, 0xFFFFFFFF};

   caps->minImageExtent = {1, 1};
   caps->maxImageExtent = any_extent;
   caps->currentExtent = any_extent;

   caps->supportedCompositeAlpha =
       VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR | VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

   caps->minImageCount = 3;
   caps->maxImageCount = 3;
   caps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   caps->maxImageArrayLayers = 1;
   caps->supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   return VK_SUCCESS;
}

static VkResult magma_surface_get_formats(VkIcdSurfaceBase* surface, struct wsi_device* device,
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

static VkResult magma_surface_create_swapchain(VkIcdSurfaceBase* icd_surface, VkDevice device,
                                               wsi_device* wsi_device,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               const struct wsi_image_fns* image_fns,
                                               struct wsi_swapchain** swapchain_out)
{
   DLOG("magma_surface_create_swapchain");

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);

   auto wsi_magma = static_cast<WsiMagma*>(wsi_device->wsi[VK_ICD_WSI_PLATFORM_MAGMA]);
   assert(wsi_magma);

   // TODO(MA-115): use pAllocator here and for images (and elsewhere in magma?)
   auto chain = std::make_unique<MagmaSwapchain>(device, wsi_magma->connection(device));

   uint32_t num_images = pCreateInfo->minImageCount;

   for (uint32_t i = 0; i < num_images; i++) {
      std::unique_ptr<MagmaImage> image = MagmaImage::Create(
          device, wsi_magma->callbacks(), wsi_magma->connection(device), pCreateInfo, pAllocator);
      if (!image)
         return VK_ERROR_OUT_OF_DEVICE_MEMORY;

      chain->AddImage(std::move(image));
   }

   *swapchain_out = chain.release();

   return VK_SUCCESS;
}

VkResult wsi_magma_init_wsi(wsi_device* device, const VkAllocationCallbacks* alloc,
                            const wsi_magma_callbacks* callbacks)
{
   device->wsi[VK_ICD_WSI_PLATFORM_MAGMA] = nullptr;

   auto wsi = new WsiMagma(callbacks);
   if (!wsi)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   wsi->get_support = magma_surface_get_support;
   wsi->get_capabilities = magma_surface_get_capabilities;
   wsi->get_formats = magma_surface_get_formats;
   wsi->get_present_modes = magma_surface_get_present_modes;
   wsi->create_swapchain = magma_surface_create_swapchain;

   device->wsi[VK_ICD_WSI_PLATFORM_MAGMA] = wsi;

   return VK_SUCCESS;
}

void wsi_magma_finish_wsi(wsi_device* device, const VkAllocationCallbacks* alloc)
{
   auto wsi_magma = static_cast<WsiMagma*>(device->wsi[VK_ICD_WSI_PLATFORM_MAGMA]);
   if (wsi_magma)
      delete wsi_magma;
}

VkBool32 wsi_get_physical_device_magma_presentation_support(struct wsi_device* wsi_device,
                                                            VkAllocationCallbacks* alloc,
                                                            uint32_t queueFamilyIndex)
{
   return VK_TRUE;
}

VkResult wsi_create_magma_surface(const VkAllocationCallbacks* pAllocator,
                                  const VkMagmaSurfaceCreateInfoKHR* pCreateInfo,
                                  VkSurfaceKHR* pSurface)
{
   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_MAGMA_SURFACE_CREATE_INFO_KHR);

   auto surface = reinterpret_cast<VkIcdSurfaceMagma*>(
       vk_alloc(pAllocator, sizeof(VkIcdSurfaceMagma), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
   if (!surface)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   surface->base.platform = VK_ICD_WSI_PLATFORM_MAGMA;

   *pSurface = _VkIcdSurfaceBase_to_handle(&surface->base);

   return VK_SUCCESS;
}
