// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <lib/framebuffer/framebuffer.h>
#include <memory>
#include <vector>
#include <zircon/syscalls.h>

#include "util/macros.h"
#include "wsi_common_magma.h"

#include "magma.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"

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

   const wsi_magma_callbacks* callbacks() { return callbacks_; }

private:
   const wsi_magma_callbacks* callbacks_;
};

class WsiMagmaConnections {
public:
   WsiMagmaConnections(magma_connection_t* render_connection,
                       const wsi_magma_callbacks* callbacks)
       : callbacks_(callbacks), render_connection_(render_connection)
   {
   }

   magma_connection_t* render_connection() { return render_connection_; }

private:
   const wsi_magma_callbacks* callbacks_;  // not owned
   magma_connection_t* render_connection_; // not owned
};

//////////////////////////////////////////////////////////////////////////////

class MagmaImage {
public:
   static std::unique_ptr<MagmaImage> Create(VkDevice device, const wsi_magma_callbacks* callbacks,
                                             std::shared_ptr<WsiMagmaConnections> connections,
                                             const VkSwapchainCreateInfoKHR* create_info,
                                             const VkAllocationCallbacks* pAllocator);

   ~MagmaImage();

   uint64_t display_buffer() { return display_buffer_; }

   zx_handle_t buffer_presented_semaphore() { return buffer_presented_semaphore_; }

   uint64_t buffer_presented_semaphore_id() { return buffer_presented_semaphore_id_; }

   VkImage image() { return image_; }

   VkDevice device() { return device_; }

   const wsi_magma_callbacks* callbacks() { return callbacks_; }

private:
   MagmaImage(VkDevice device, const wsi_magma_callbacks* callbacks,
              std::shared_ptr<WsiMagmaConnections> connections, magma_buffer_t render_buffer,
              uint64_t display_buffer, zx_handle_t buffer_presented_semaphore,
              uint64_t buffer_presented_semaphore_id, VkImage image, VkDeviceMemory device_memory,
              const VkAllocationCallbacks* allocator)
       : device_(device), image_(image), device_memory_(device_memory), callbacks_(callbacks),
         connections_(std::move(connections)), render_buffer_(render_buffer),
         display_buffer_(display_buffer), buffer_presented_semaphore_(buffer_presented_semaphore),
         buffer_presented_semaphore_id_(buffer_presented_semaphore_id), allocator_(allocator)
   {
   }

   VkDevice device_;
   VkImage image_;
   VkDeviceMemory device_memory_;
   const wsi_magma_callbacks* callbacks_;
   std::shared_ptr<WsiMagmaConnections> connections_;
   magma_buffer_t render_buffer_; // render_buffer is not owned
   uint64_t display_buffer_;
   zx_handle_t buffer_presented_semaphore_;
   uint64_t buffer_presented_semaphore_id_;
   const VkAllocationCallbacks* allocator_;
};

std::unique_ptr<MagmaImage> MagmaImage::Create(VkDevice device,
                                               const wsi_magma_callbacks* callbacks,
                                               std::shared_ptr<WsiMagmaConnections> connections,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* allocator)
{
   VkResult result;
   uint32_t row_pitch;
   uint32_t offset;
   uint32_t bpp = 32;
   magma_buffer_t render_buffer;
   uint64_t display_buffer;
   zx_handle_t buffer_presented_semaphore;
   uint32_t buffer_handle, semaphore_handle;
   uint32_t size;
   VkImage image;
   VkDeviceMemory device_memory;

   result = callbacks->create_wsi_image(device, pCreateInfo, allocator, &image, &device_memory,
                                        &size, &offset, &row_pitch, &render_buffer);
   if (result != VK_SUCCESS)
      return DRETP(nullptr, "create_wsi_image failed");

   magma_status_t status =
       magma_export(connections->render_connection(), render_buffer, &buffer_handle);
   if (status != MAGMA_STATUS_OK)
      return DRETP(nullptr, "failed to export buffer");

   // Must be consistent with intel-gpu-core.h and the tiling format
   // used by VK_IMAGE_USAGE_SCANOUT_BIT_GOOGLE.
   const uint32_t kImageTypeXTiled = 1;
   status = fb_import_image(buffer_handle, kImageTypeXTiled, &display_buffer);
   if (status != MAGMA_STATUS_OK)
      return DRETP(nullptr, "failed to import buffer");

   if (zx_event_create(0, &buffer_presented_semaphore) != ZX_OK
         || zx_object_signal(buffer_presented_semaphore, 0, ZX_EVENT_SIGNALED) != ZX_OK)
      return DRETP(nullptr, "failed to create or signal semaphore");

   zx_info_handle_basic_t info;
   if (zx_object_get_info(buffer_presented_semaphore, ZX_INFO_HANDLE_BASIC,
                          &info, sizeof(info), nullptr, nullptr) != ZX_OK)
      return DRETP(nullptr, "failed to get semaphore id");

   zx_handle_t dup;
   if (zx_handle_duplicate(buffer_presented_semaphore, ZX_RIGHT_SAME_RIGHTS, &dup) != ZX_OK
         || fb_import_event(dup, info.koid) != ZX_OK)
      return DRETP(nullptr, "failed to duplicate or import display semaphore");

   return std::unique_ptr<MagmaImage>(new MagmaImage(
       device, callbacks, std::move(connections), render_buffer, display_buffer,
       buffer_presented_semaphore, info.koid, image, device_memory, allocator));
}

MagmaImage::~MagmaImage()
{
   callbacks_->free_wsi_image(device_, allocator_, image_, device_memory_);

   fb_release_image(display_buffer_);
   fb_release_event(buffer_presented_semaphore_id_);

   zx_handle_close(buffer_presented_semaphore_);
}

//////////////////////////////////////////////////////////////////////////////

class MagmaSwapchain : public wsi_swapchain {
public:
   MagmaSwapchain(VkDevice device, std::shared_ptr<WsiMagmaConnections> connections)
       : connections_(connections)
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
   }

   magma_connection_t* render_connection() { return connections_->render_connection(); }

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

   static VkResult AcquireNextImage(wsi_swapchain* wsi_chain, uint64_t timeout_ns,
                                    VkSemaphore semaphore, uint32_t* pImageIndex)
   {
      MagmaSwapchain* chain = cast(wsi_chain);

      uint32_t index = chain->next_index_;
      MagmaImage* image = chain->get_image(index);

      DLOG("AcquireNextImage semaphore id 0x%" PRIx64,
           image->buffer_presented_semaphore_id());

      // The zircon display APIs don't support providing an image back to the driver
      // before the image is retired. Returning the presented semaphore from the vulkan API
      // only prevents clients from rendering into the buffer, not from presenting the
      // buffer (with a wait semaphore). So we can't return the buffer until this is signaled.
      zx_signals_t observed;
      zx_status_t status;
      if ((status = zx_object_wait_one(image->buffer_presented_semaphore(), ZX_EVENT_SIGNALED,
                                       zx_deadline_after(timeout_ns), &observed)) != ZX_OK) {
         return VK_TIMEOUT;
      }
      // Unsignal the event.
      zx_object_signal(image->buffer_presented_semaphore(), ZX_EVENT_SIGNALED, 0);
      if (semaphore) {
         image->callbacks()->signal_semaphore(semaphore);
      }

      if (++chain->next_index_ >= chain->image_count())
         chain->next_index_ = 0;

      *pImageIndex = index;
      DLOG("AcquireNextImage returning index %u id 0x%" PRIx64, *pImageIndex,
           image->display_buffer());

      return VK_SUCCESS;
   }

   static VkResult QueuePresent(wsi_swapchain* swapchain, uint32_t image_index,
                                const VkPresentRegionKHR *damage,
                                uint32_t wait_semaphore_count, const VkSemaphore* wait_semaphores)
   {
      MagmaSwapchain* magma_swapchain = cast(swapchain);
      MagmaImage* image = magma_swapchain->get_image(image_index);

      DLOG("QueuePresent image_index %u id 0x%" PRIx64, image_index,
           image->display_buffer());

      magma_status_t status;

      // TODO(MA-375): Support more than 1 wait semaphore
      assert(wait_semaphore_count <= 1);

      uint64_t wait_sem_id = FB_INVALID_ID;
      for (uint32_t i = 0; i < wait_semaphore_count; i++) {
         uint32_t semaphore_handle;
         uintptr_t platform_sem = image->callbacks()
             ->get_platform_semaphore(swapchain->device, wait_semaphores[i]);
         wait_sem_id = magma_get_semaphore_id(platform_sem);

         status = magma_export_semaphore(
             magma_swapchain->render_connection(), platform_sem, &semaphore_handle);
         if (status != MAGMA_STATUS_OK) {
            DLOG("Failed to export wait semaphore");
            continue;
         }
         magma_semaphore_t semaphore;
         status = fb_import_event(semaphore_handle, wait_sem_id);
         if (status != ZX_OK)
            DLOG("failed to import wait semaphore");
      }

      fb_present_image(image->display_buffer(),
                       wait_sem_id, FB_INVALID_ID, image->buffer_presented_semaphore_id());

      for (uint32_t i = 0; i < wait_semaphore_count; i++) {
         fb_release_event(wait_sem_id);
      }

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
   std::shared_ptr<WsiMagmaConnections> connections_;
   std::vector<std::unique_ptr<MagmaImage>> images_;
   uint32_t next_index_ = 0;
};

static VkResult magma_surface_get_support(VkIcdSurfaceBase* icd_surface,
                                          struct wsi_device* wsi_device,
                                          const VkAllocationCallbacks* alloc,
                                          uint32_t queueFamilyIndex, 
                                          int local_fd,
                                          bool can_handle_different_gpu,
                                          VkBool32* pSupported)
{
   auto surface = reinterpret_cast<VkIcdSurfaceMagma*>(icd_surface);
   DLOG("magma_surface_get_support queue %u connection %d", queueFamilyIndex, surface->has_fb);

   *pSupported = surface->has_fb;
   return VK_SUCCESS;
}

static VkResult magma_surface_get_capabilities(VkIcdSurfaceBase* icd_surface,
                                               VkSurfaceCapabilitiesKHR* caps)
{
   auto surface = reinterpret_cast<VkIcdSurfaceMagma*>(icd_surface);
   DLOG("magma_surface_get_capabilities connection %d", surface->has_fb);

   VkExtent2D extent = {0xFFFFFFFF, 0xFFFFFFFF};

   uint32_t width, height, stride;
   zx_pixel_format_t format;
   fb_get_config(&width, &height, &stride, &format);
   extent = {width, height};

   caps->minImageExtent = {1, 1};
   caps->maxImageExtent = {extent};
   caps->currentExtent = extent;

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
                                               int local_fd,
                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               const struct wsi_image_fns* image_fns,
                                               struct wsi_swapchain** swapchain_out)
{
   DLOG("magma_surface_create_swapchain");

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);

   auto wsi_magma = static_cast<WsiMagma*>(wsi_device->wsi[VK_ICD_WSI_PLATFORM_MAGMA]);
   assert(wsi_magma);

   auto render_connection =
       reinterpret_cast<magma_connection_t*>(wsi_magma->callbacks()->get_render_connection(device));
   auto magma_surface = reinterpret_cast<VkIcdSurfaceMagma*>(icd_surface);

   // TODO(MA-115): use pAllocator here and for images (and elsewhere in magma?)
   auto connections = std::make_shared<WsiMagmaConnections>(render_connection,
                                                            wsi_magma->callbacks());

   auto chain = std::make_unique<MagmaSwapchain>(device, connections);

   uint32_t num_images = pCreateInfo->minImageCount;

   for (uint32_t i = 0; i < num_images; i++) {
      std::unique_ptr<MagmaImage> image =
          MagmaImage::Create(device, wsi_magma->callbacks(), connections, pCreateInfo, pAllocator);
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
