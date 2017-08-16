// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "magma.h"
#include "magma_util/macros.h"
#include "vulkan_shim.h"

class TestWsiMagma {
public:
   ~TestWsiMagma();

   bool Init();
   bool Run(uint32_t frame_count);

private:
   VkInstance instance_;
   VkDevice device_;
   VkSurfaceKHR surface_;
   VkExtent2D extent_;
   VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
   VkQueue queue_;

   PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR_;
   PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR_;
   PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR_;
   PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR_;
   PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR_;
   PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR_;
   PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR_;
   PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR_;
   PFN_vkQueuePresentKHR fpQueuePresentKHR_;
};

bool operator==(VkExtent2D& a, VkExtent2D b) { return a.width == b.width && a.height == b.height; }

bool TestWsiMagma::Init()
{
   // Enumerate and create a device - use the anv_magma_stubs though
   VkInstanceCreateInfo create_info{
       VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType             sType;
       nullptr,                                // const void*                 pNext;
       0,                                      // VkInstanceCreateFlags       flags;
       nullptr,                                // const VkApplicationInfo*    pApplicationInfo;
       0,                                      // uint32_t                    enabledLayerCount;
       nullptr,                                // const char* const*          ppEnabledLayerNames;
       0,                                      // uint32_t                    enabledExtensionCount;
       nullptr, // const char* const*          ppEnabledExtensionNames;
   };

   VkResult result;

   result = vkCreateInstance(&create_info, nullptr /*allocation_callbacks*/, &instance_);
   if (result != VK_SUCCESS)
      return DRETF(false, "vkCreateInstance failed %d", result);

   printf("vkCreateInstance succeeded\n");

   fpGetPhysicalDeviceSurfaceSupportKHR_ =
       reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
           vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfaceSupportKHR"));
   assert(fpGetPhysicalDeviceSurfaceSupportKHR_);
   fpGetPhysicalDeviceSurfaceCapabilitiesKHR_ =
       reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
           vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
   assert(fpGetPhysicalDeviceSurfaceCapabilitiesKHR_);
   fpGetPhysicalDeviceSurfaceFormatsKHR_ =
       reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
           vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
   assert(fpGetPhysicalDeviceSurfaceFormatsKHR_);
   fpGetPhysicalDeviceSurfacePresentModesKHR_ =
       reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
           vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
   assert(fpGetPhysicalDeviceSurfacePresentModesKHR_);
   fpCreateSwapchainKHR_ = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
       vkGetInstanceProcAddr(instance_, "vkCreateSwapchainKHR"));
   assert(fpCreateSwapchainKHR_);
   fpDestroySwapchainKHR_ = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
       vkGetInstanceProcAddr(instance_, "vkDestroySwapchainKHR"));
   assert(fpDestroySwapchainKHR_);
   fpGetSwapchainImagesKHR_ = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
       vkGetInstanceProcAddr(instance_, "vkGetSwapchainImagesKHR"));
   assert(fpGetSwapchainImagesKHR_);
   fpAcquireNextImageKHR_ = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
       vkGetInstanceProcAddr(instance_, "vkAcquireNextImageKHR"));
   assert(fpAcquireNextImageKHR_);
   fpQueuePresentKHR_ = reinterpret_cast<PFN_vkQueuePresentKHR>(
       vkGetInstanceProcAddr(instance_, "vkQueuePresentKHR"));
   assert(fpQueuePresentKHR_);

   uint32_t physical_device_count;
   result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
   if (result != VK_SUCCESS)
      return DRETF(false, "vkEnumeratePhysicalDevices failed %d", result);

   if (physical_device_count < 1)
      return DRETF(false, "unexpected physical_device_count %d", physical_device_count);

   printf("vkEnumeratePhysicalDevices returned count %d\n", physical_device_count);

   std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
   if ((result = vkEnumeratePhysicalDevices(instance_, &physical_device_count,
                                            physical_devices.data())) != VK_SUCCESS)
      return DRETF(false, "vkEnumeratePhysicalDevices failed %d", result);

   uint32_t queue_family_count;
   vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[0], &queue_family_count, nullptr);

   if (queue_family_count < 1)
      return DRETF(false, "invalid queue_family_count %d", queue_family_count);

   std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
   vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[0], &queue_family_count,
                                            queue_family_properties.data());

   VkMagmaSurfaceCreateInfoKHR createInfo = {
       .sType = VK_STRUCTURE_TYPE_MAGMA_SURFACE_CREATE_INFO_KHR, .pNext = nullptr,
   };

   result = vkCreateMagmaSurfaceKHR(instance_, &createInfo, nullptr, &surface_);
   if (result != VK_SUCCESS)
      return DRETF(false, "vkCreateMagmaSurfaceKHR failed: %d", result);

   uint32_t queue_family_index = UINT32_MAX;

   for (uint32_t i = 0; i < queue_family_count; i++) {
      VkBool32 supports_present;
      fpGetPhysicalDeviceSurfaceSupportKHR_(physical_devices[0], i, surface_, &supports_present);
      if (supports_present)
         queue_family_index = i;
   }

   if (queue_family_index == UINT32_MAX)
      return DRETF(false, "couldn't find a queue supporting present");

   uint32_t formatCount;
   result =
       fpGetPhysicalDeviceSurfaceFormatsKHR_(physical_devices[0], surface_, &formatCount, nullptr);
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetPhysicalDeviceSurfaceFormatsKHR failed %d", result);

   std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
   result = fpGetPhysicalDeviceSurfaceFormatsKHR_(physical_devices[0], surface_, &formatCount,
                                                  surfFormats.data());
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetPhysicalDeviceSurfaceFormatsKHR failed %d", result);

   if (surfFormats.size() != 1)
      return DRETF(false, "unexpected number of surface formats %zd", surfFormats.size());

   if (surfFormats[0].format != VK_FORMAT_B8G8R8A8_UNORM)
      return DRETF(false, "unexpected surface format 0x%x", surfFormats[0].format);

   uint32_t presentModeCount;
   result = fpGetPhysicalDeviceSurfacePresentModesKHR_(physical_devices[0], surface_,
                                                       &presentModeCount, nullptr);
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetPhysicalDeviceSurfacePresentModesKHR failed %d", result);

   std::vector<VkPresentModeKHR> presentModes(presentModeCount);
   result = fpGetPhysicalDeviceSurfacePresentModesKHR_(physical_devices[0], surface_,
                                                       &presentModeCount, presentModes.data());
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetPhysicalDeviceSurfacePresentModesKHR failed %d", result);

   if (presentModes.size() != 1)
      return DRETF(false, "unexpected number of present modes %zd", presentModes.size());

   if (presentModes[0] != VK_PRESENT_MODE_FIFO_KHR)
      return DRETF(false, "unexpected present mode 0x%x", presentModes[0]);

   VkSurfaceCapabilitiesKHR surf_caps;
   result = fpGetPhysicalDeviceSurfaceCapabilitiesKHR_(physical_devices[0], surface_, &surf_caps);
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetPhysicalDeviceSurfaceCapabilitiesKHR_ failed %d", result);

   if (surf_caps.currentExtent == VkExtent2D{0, 0})
      return DRETF(false, "unexpected extent");

   if (!(surf_caps.currentExtent == VkExtent2D{UINT32_MAX, UINT32_MAX}))
      return DRETF(false, "unexpected extent");

   if (!(surf_caps.minImageExtent == VkExtent2D{1,1}))
      return DRETF(false, "unexpected minImageExtent");

   if (!(surf_caps.maxImageExtent == VkExtent2D{UINT32_MAX, UINT32_MAX}))
      return DRETF(false, "unexpected maxImageExtent");

   extent_ = VkExtent2D{1024,768};

   if (surf_caps.supportedCompositeAlpha !=
       (VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR | VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
      return DRETF(false, "unexpected supportedCompositeAlpha 0x%x",
                   surf_caps.supportedCompositeAlpha);

   if (surf_caps.minImageCount != 2)
      return DRETF(false, "unexpected minImageCount %d", surf_caps.minImageCount);

   if (surf_caps.maxImageCount != 3)
      return DRETF(false, "unexpected minImageCount %d", surf_caps.maxImageCount);

   if (surf_caps.supportedTransforms != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      return DRETF(false, "unexpected supportedTransforms 0x%x", surf_caps.supportedTransforms);

   if (surf_caps.maxImageArrayLayers != 1)
      return DRETF(false, "unexpected maxImageArrayLayers 0x%x", surf_caps.maxImageArrayLayers);

   if (surf_caps.supportedUsageFlags !=
       (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
      return DRETF(false, "unexpected supportedUsageFlags 0x%x", surf_caps.supportedUsageFlags);

   // Create the device.

   float queue_priorities[1] = {0.0};

   VkDeviceQueueCreateInfo queue_create_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = 0,
                                                .queueFamilyIndex = queue_family_index,
                                                .queueCount = 1,
                                                .pQueuePriorities = queue_priorities};
   VkDeviceCreateInfo device_create_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .queueCreateInfoCount = 1,
                                            .pQueueCreateInfos = &queue_create_info,
                                            .enabledLayerCount = 0,
                                            .ppEnabledLayerNames = nullptr,
                                            .enabledExtensionCount = 0,
                                            .ppEnabledExtensionNames = nullptr,
                                            .pEnabledFeatures = nullptr};

   result = vkCreateDevice(physical_devices[0], &device_create_info,
                           nullptr /* allocationcallbacks */, &device_);
   if (result != VK_SUCCESS)
      return DRETF(false, "vkCreateDevice failed: %d", result);

   vkGetDeviceQueue(device_, queue_family_index, 0, &queue_);

   VkSwapchainCreateInfoKHR swapchain_create_info = {
       .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
       .pNext = nullptr,
       .surface = surface_,
       .minImageCount = 2,
       .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
       .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR, // the only supported value?
       .imageExtent = extent_,
       .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
       .preTransform = surf_caps.currentTransform,
       .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
       .imageArrayLayers = 1,
       .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
       .queueFamilyIndexCount = 0,
       .pQueueFamilyIndices = NULL,
       .presentMode = presentModes[0],
       .oldSwapchain = VK_NULL_HANDLE,
       .clipped = true,
   };

   result = fpCreateSwapchainKHR_(device_, &swapchain_create_info, nullptr, &swapchain_);
   if (result != VK_SUCCESS)
      return DRETF(false, "fpCreateSwapchainKHR failed: %d", result);

   return true;
}

bool TestWsiMagma::Run(uint32_t frame_count)
{
   uint32_t image_count;

   VkResult result = fpGetSwapchainImagesKHR_(device_, swapchain_, &image_count, nullptr);
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetSwapchainImagesKHR failed: %d", result);

   std::vector<VkImage> images(image_count);

   result = fpGetSwapchainImagesKHR_(device_, swapchain_, &image_count, images.data());
   if (result != VK_SUCCESS)
      return DRETF(false, "fpGetSwapchainImagesKHR failed: %d", result);

   uint32_t image_index;

   for (uint32_t frame = 0; frame < frame_count; frame++) {
      result = fpAcquireNextImageKHR_(device_, swapchain_, 0, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                      &image_index);
      if (result != VK_SUCCESS)
         return DRETF(false, "fpAcquireNextImageKHR_ failed: %d", result);

      VkPresentInfoKHR present = {
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .pNext = nullptr,
          .waitSemaphoreCount = 0,
          .pWaitSemaphores = nullptr,
          .swapchainCount = 1,
          .pSwapchains = &swapchain_,
          .pImageIndices = &image_index,
      };

      result = fpQueuePresentKHR_(queue_, &present);
      if (result != VK_SUCCESS)
         return DRETF(false, "fpQueuePresentKHR_ failed (image index %u): %d", image_index, result);
   }

   return true;
}

TestWsiMagma::~TestWsiMagma()
{
   if (swapchain_ != VK_NULL_HANDLE)
      fpDestroySwapchainKHR_(device_, swapchain_, nullptr);
}

int main(int argc, char** argv)
{
   VulkanShimInit();

   TestWsiMagma test;
   if (!test.Init())
      return -1;
   if (!test.Run(100))
      return 0;
}
