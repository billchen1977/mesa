// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>
#include <vector>

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
                       magma_connection_t* display_connection, const wsi_magma_callbacks* callbacks)
       : callbacks_(callbacks), render_connection_(render_connection),
         display_connection_(display_connection)
   {
   }

   magma_connection_t* render_connection() { return render_connection_; }

   magma_connection_t* display_connection() { return display_connection_; }

private:
   const wsi_magma_callbacks* callbacks_;  // not owned
   magma_connection_t* render_connection_; // not owned
   magma_connection_t* display_connection_;
};

//////////////////////////////////////////////////////////////////////////////

class MagmaImage {
public:
   static std::unique_ptr<MagmaImage> Create(VkDevice device, const wsi_magma_callbacks* callbacks,
                                             std::shared_ptr<WsiMagmaConnections> connections,
                                             const VkSwapchainCreateInfoKHR* create_info,
                                             const VkAllocationCallbacks* pAllocator);

   ~MagmaImage();

   magma_buffer_t display_buffer() { return display_buffer_; }

   magma_semaphore_t display_semaphore() { return display_semaphore_; }

   magma_semaphore_t buffer_presented_semaphore() { return buffer_presented_semaphore_; }

   VkImage image() { return image_; }

   VkDevice device() { return device_; }

   const wsi_magma_callbacks* callbacks() { return callbacks_; }

private:
   MagmaImage(VkDevice device, const wsi_magma_callbacks* callbacks,
              std::shared_ptr<WsiMagmaConnections> connections, magma_buffer_t render_buffer,
              magma_buffer_t display_buffer, magma_semaphore_t display_semaphore, VkImage image,
              VkDeviceMemory device_memory, const VkAllocationCallbacks* allocator,
              magma_semaphore_t buffer_presented_semaphore)
       : device_(device), image_(image), device_memory_(device_memory), callbacks_(callbacks),
         connections_(std::move(connections)), render_buffer_(render_buffer),
         display_buffer_(display_buffer), display_semaphore_(display_semaphore),
         buffer_presented_semaphore_(buffer_presented_semaphore), allocator_(allocator)
   {
   }

   VkDevice device_;
   VkImage image_;
   VkDeviceMemory device_memory_;
   const wsi_magma_callbacks* callbacks_;
   std::shared_ptr<WsiMagmaConnections> connections_;
   magma_buffer_t render_buffer_, display_buffer_; // render_buffer is not owned
   magma_semaphore_t display_semaphore_;
   magma_semaphore_t buffer_presented_semaphore_;
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
   magma_buffer_t render_buffer, display_buffer;
   magma_semaphore_t display_semaphore;
   magma_semaphore_t buffer_presented_semaphore;
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

   status = magma_import(connections->display_connection(), buffer_handle, &display_buffer);
   if (status != MAGMA_STATUS_OK)
      return DRETP(nullptr, "failed to import buffer");

   status = magma_create_semaphore(connections->display_connection(), &display_semaphore);
   if (status != MAGMA_STATUS_OK)
      return DRETP(nullptr, "failed to create semaphore");

   magma_signal_semaphore(display_semaphore);

   status = magma_create_semaphore(connections->display_connection(), &buffer_presented_semaphore);
   if (status != MAGMA_STATUS_OK)
      return DRETP(nullptr, "failed to create semaphore");

   return std::unique_ptr<MagmaImage>(new MagmaImage(
       device, callbacks, std::move(connections), render_buffer, display_buffer, display_semaphore,
       image, device_memory, allocator, buffer_presented_semaphore));
}

MagmaImage::~MagmaImage()
{
   callbacks_->free_wsi_image(device_, allocator_, image_, device_memory_);

   magma_release_buffer(connections_->display_connection(), display_buffer_);
   magma_release_semaphore(connections_->display_connection(), display_semaphore_);
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

   magma_connection_t* display_connection() { return connections_->display_connection(); }

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

   static VkResult AcquireNextImage(wsi_swapchain* wsi_chain, uint64_t timeout,
                                    VkSemaphore semaphore, uint32_t* pImageIndex)
   {
      MagmaSwapchain* chain = cast(wsi_chain);

      uint32_t index = chain->next_index_;
      MagmaImage* image = chain->get_image(index);

      //TODO(MA-289) remove this wait/signal once Mozart 2.0 lands
      magma_status_t status = magma_wait_semaphore(image->display_semaphore(), timeout);
      if (status == MAGMA_STATUS_TIMED_OUT) {
         DLOG("timeout waiting for image semaphore");
         return VK_TIMEOUT;
      }
      magma_signal_semaphore(image->display_semaphore());


      DLOG("AcquireNextImage semaphore id 0x%" PRIx64,
           magma_get_semaphore_id(image->display_semaphore()));

      if (semaphore) {
         uint32_t semaphore_handle;
         magma_status_t status = magma_export_semaphore(
             chain->display_connection(), image->display_semaphore(), &semaphore_handle);
         if (status == MAGMA_STATUS_OK) {
            VkImportSemaphoreFuchsiaHandleInfoKHR info = {
                .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FUCHSIA_HANDLE_INFO_KHR,
                .semaphore = semaphore,
                .flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR,
                .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FUCHSIA_FENCE_BIT_KHR,
                .handle = semaphore_handle};
            VkResult result =
                image->callbacks()->vk_import_semaphore_fuchsia_handle_khr(image->device(), &info);
            if (result != VK_SUCCESS)
               DLOG("vkImportSemaphoreFuchsiaHandleKHR failed: %d", result);
         } else {
            DLOG("magma_export_semaphore failed: %d", status);
         }
      }

      if (++chain->next_index_ >= chain->image_count())
         chain->next_index_ = 0;

      *pImageIndex = index;
      DLOG("AcquireNextImage returning index %u id 0x%" PRIx64, *pImageIndex,
           magma_get_buffer_id(image->display_buffer()));

      return VK_SUCCESS;
   }

   static VkResult QueuePresent(wsi_swapchain* swapchain, uint32_t image_index,
                                const VkPresentRegionKHR *damage,
                                uint32_t wait_semaphore_count, const VkSemaphore* wait_semaphores)
   {
      MagmaSwapchain* magma_swapchain = cast(swapchain);
      MagmaImage* image = magma_swapchain->get_image(image_index);

      DLOG("QueuePresent image_index %u id 0x%" PRIx64, image_index,
           magma_get_buffer_id(image->display_buffer()));

      magma_semaphore_t display_semaphores[wait_semaphore_count];
      magma_status_t status;

      for (uint32_t i = 0; i < wait_semaphore_count; i++) {
         uint32_t semaphore_handle;
         status = magma_export_semaphore(
             magma_swapchain->render_connection(),
             image->callbacks()->get_platform_semaphore(wait_semaphores[i]), &semaphore_handle);
         if (status != MAGMA_STATUS_OK) {
            DLOG("Failed to export wait semaphore");
            continue;
         }
         magma_semaphore_t semaphore;
         status = magma_import_semaphore(magma_swapchain->display_connection(), semaphore_handle,
                                         &display_semaphores[i]);
         if (status != MAGMA_STATUS_OK)
            DLOG("failed to import wait semaphore");
      }

      magma_semaphore_t signal_semaphores[1]{image->display_semaphore()};

      magma_display_page_flip(magma_swapchain->display_connection(), image->display_buffer(),
                              wait_semaphore_count, display_semaphores, 1, signal_semaphores,
                              image->buffer_presented_semaphore());

      for (uint32_t i = 0; i < wait_semaphore_count; i++) {
         magma_release_semaphore(magma_swapchain->display_connection(), display_semaphores[i]);
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
   DLOG("magma_surface_get_support queue %u connection %p", queueFamilyIndex, surface->connection);

   *pSupported = surface->connection != nullptr;
   return VK_SUCCESS;
}

static VkResult magma_surface_get_capabilities(VkIcdSurfaceBase* icd_surface,
                                               VkSurfaceCapabilitiesKHR* caps)
{
   auto surface = reinterpret_cast<VkIcdSurfaceMagma*>(icd_surface);
   DLOG("magma_surface_get_capabilities connection %p", surface->connection);

   VkExtent2D extent = {0xFFFFFFFF, 0xFFFFFFFF};

   magma_display_size display_size;
   magma_status_t status = magma_display_get_size(surface->fd, &display_size);
   if (status == MAGMA_STATUS_OK)
      extent = {display_size.width, display_size.height};

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
   auto display_connection = reinterpret_cast<magma_connection_t*>(magma_surface->connection);

   // TODO(MA-115): use pAllocator here and for images (and elsewhere in magma?)
   auto connections = std::make_shared<WsiMagmaConnections>(render_connection, display_connection,
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
