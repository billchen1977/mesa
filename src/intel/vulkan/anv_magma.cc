// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "drm_command_buffer.h"
#include "magma.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "magma_util/sleep.h"
// clang-format off
#include "anv_private.h"
// clang-format on

static magma_connection_t* magma_connection(anv_device* device)
{
   DASSERT(device);
   DASSERT(device->connection);
   return device->connection;
}

int anv_gem_connect(anv_device* device)
{
   device->connection = magma_create_connection(device->fd, MAGMA_CAPABILITY_RENDERING);
   if (!device->connection)
      return DRET_MSG(-1, "magma_system_open failed");

   DLOG("opened a magma system connection");
   return 0;
}

void anv_gem_disconnect(anv_device* device)
{
   magma_release_connection(magma_connection(device));
   DLOG("closed the magma system connection");
}

// Return handle, or 0 on failure. Gem handles are never 0.
anv_buffer_handle_t anv_gem_create(anv_device* device, size_t size)
{
   magma_buffer_t buffer;
   uint64_t magma_size = size;
   if (magma_create_buffer(magma_connection(device), magma_size, &magma_size, &buffer) != 0) {
      DLOG("magma_system_alloc failed size 0x%zx", magma_size);
      return 0;
   }
   DLOG("magma_system_alloc size 0x%zx returning buffer 0x%lx", magma_size, buffer);

   DASSERT(buffer != 0);
   return buffer;
}

void anv_gem_close(anv_device* device, anv_buffer_handle_t handle)
{
   DLOG("anv_gem_close handle 0x%lx", handle);
   magma_release_buffer(magma_connection(device), handle);
}

void* anv_gem_mmap(anv_device* device, anv_buffer_handle_t handle, uint64_t offset, uint64_t size,
                   uint32_t flags)
{
   DASSERT(flags == 0);
   void* addr;
   magma_status_t status = magma_map(magma_connection(device), handle, &addr);
   if (status != MAGMA_STATUS_OK) {
      DLOG("magma_system_map failed: status %d", status);
      return MAP_FAILED;
   }
   DLOG("magma_system_map handle 0x%lx size 0x%zx returning %p", handle, size, addr);
   return reinterpret_cast<uint8_t*>(addr) + offset;
}

void anv_gem_munmap(anv_device* device, anv_buffer_handle_t gem_handle, void* addr, uint64_t size)
{
   if (!addr)
      return;

   if (magma_unmap(magma_connection(device), gem_handle) != 0) {
      DLOG("magma_system_unmap failed");
      return;
   }

   DLOG("magma_system_unmap handle 0x%lx", gem_handle);
}

uint32_t anv_gem_userptr(anv_device* device, void* mem, size_t size)
{
   DLOG("anv_gem_userptr - STUB");
   DASSERT(false);
   return 0;
}

int anv_gem_set_caching(anv_device* device, anv_buffer_handle_t gem_handle, uint32_t caching)
{
   DLOG("anv_get_set_caching - STUB");
   return 0;
}

int anv_gem_set_domain(anv_device* device, anv_buffer_handle_t gem_handle, uint32_t read_domains,
                       uint32_t write_domain)
{
   DLOG("anv_gem_set_domain - STUB");
   return 0;
}

/**
 * On error, \a timeout_ns holds the remaining time.
 */
int anv_gem_wait(anv_device* device, anv_buffer_handle_t handle, int64_t* timeout_ns)
{
   magma_wait_rendering(magma_connection(device), handle);
   return 0;
}

/**
 * Returns 0, 1, or negative to indicate error
 */
int anv_gem_busy(anv_device *device, anv_buffer_handle_t handle)
{
   // Magma doesn't have a means to poll buffer busy.
   // Upper layers should be changed to check semaphore signal status.
   magma_wait_rendering(magma_connection(device), handle);
   return 0;
}

bool anv_gem_supports_48b_addresses(int fd)
{
   // TODO(MA-310)
   return false;
}

int anv_gem_get_context_param(int fd, int context, uint32_t param, uint64_t *value)
{
   if (param == I915_CONTEXT_PARAM_GTT_SIZE) {
      // TODO(MA-311) - query for this
      *value = 8ull * 1024ull * 1024ull;
      return 0;
   }
   DLOG("anv_gem_get_context_param: unhandled param 0x%x", param);
   return -1;
}

int anv_gem_execbuffer(anv_device* device, drm_i915_gem_execbuffer2* execbuf,
                       uint32_t wait_semaphore_count, anv_semaphore* wait_semaphores[],
                       uint32_t signal_semaphore_count, anv_semaphore* signal_semaphores[])
{
   DLOG("anv_gem_execbuffer");

   if (execbuf->buffer_count == 0)
      return 0;

   uint64_t required_size =
       DrmCommandBuffer::RequiredSize(execbuf, wait_semaphore_count, signal_semaphore_count);

   uint64_t cmd_buf_id;
   magma_status_t status =
       magma_create_command_buffer(magma_connection(device), required_size, &cmd_buf_id);
   if (status != MAGMA_STATUS_OK)
      return DRET_MSG(-1, "magma_alloc_command_buffer failed size 0x%" PRIx64 " : %d", required_size, status);

   void* cmd_buf_data;
   status = magma_map(magma_connection(device), cmd_buf_id, &cmd_buf_data);
   if (status != MAGMA_STATUS_OK) {
      magma_release_command_buffer(magma_connection(device), cmd_buf_id);
      return DRET_MSG(-1, "magma_system_map failed: %d", status);
   }

   std::vector<uint64_t> wait_semaphore_ids(wait_semaphore_count);
   for (uint32_t i = 0; i < wait_semaphore_count; i++) {
      wait_semaphore_ids[i] = magma_get_semaphore_id(
          reinterpret_cast<magma_semaphore_t>(wait_semaphores[i]->current_platform_semaphore));
   }

   std::vector<uint64_t> signal_semaphore_ids(signal_semaphore_count);
   for (uint32_t i = 0; i < signal_semaphore_count; i++) {
      signal_semaphore_ids[i] = magma_get_semaphore_id(
          reinterpret_cast<magma_semaphore_t>(signal_semaphores[i]->current_platform_semaphore));
   }

   if (!DrmCommandBuffer::Translate(execbuf, std::move(wait_semaphore_ids),
                                    std::move(signal_semaphore_ids), cmd_buf_data)) {
      status = magma_unmap(magma_connection(device), cmd_buf_id);
      DASSERT(status == MAGMA_STATUS_OK);
      magma_release_command_buffer(magma_connection(device), cmd_buf_id);
      return DRET_MSG(-1, "DrmCommandBuffer::Translate failed");
   }

   status = magma_unmap(magma_connection(device), cmd_buf_id);
   DASSERT(status == MAGMA_STATUS_OK);

   magma_submit_command_buffer(magma_connection(device), cmd_buf_id, device->context_id);

   return 0;
}

int anv_gem_set_tiling(anv_device* device, anv_buffer_handle_t gem_handle, uint32_t stride,
                       uint32_t tiling)
{
   DLOG("anv_gem_set_tiling - STUB");
   return 0;
}

constexpr uint32_t kQuerySubsliceAndEuTotalId = MAGMA_QUERY_VENDOR_PARAM_0;

int anv_gem_get_param(int fd, uint32_t param)
{
   magma_status_t status = MAGMA_STATUS_OK;
   uint64_t value;

   switch (param) {
   case I915_PARAM_CHIPSET_ID:
      status = magma_query(fd, MAGMA_QUERY_DEVICE_ID, &value);
      break;
   case I915_PARAM_SUBSLICE_TOTAL:
      status = magma_query(fd, kQuerySubsliceAndEuTotalId, &value);
      value >>= 32;
      break;
   case I915_PARAM_EU_TOTAL:
      status = magma_query(fd, kQuerySubsliceAndEuTotalId, &value);
      value = static_cast<uint32_t>(value);
      break;
   case I915_PARAM_HAS_WAIT_TIMEOUT:
   case I915_PARAM_HAS_EXECBUF2:
      value = 1;
      break;
   default:
      status = MAGMA_STATUS_INVALID_ARGS;
   }

   if (status != MAGMA_STATUS_OK)
      value = 0;

   uint32_t result = static_cast<uint32_t>(value);
   DASSERT(result == value);
   DLOG("anv_gem_get_param(%u, %u) returning %d", fd, param, result);
   return result;
}

bool anv_gem_get_bit6_swizzle(int fd, uint32_t tiling)
{
   DLOG("anv_gem_get_bit6_swizzle - STUB");
   return 0;
}

int anv_gem_create_context(anv_device* device)
{
   uint32_t context_id;
   magma_create_context(magma_connection(device), &context_id);
   DLOG("magma_system_create_context returned context_id %u", context_id);

   return static_cast<int>(context_id);
}

int anv_gem_destroy_context(anv_device* device, int context_id)
{
   magma_release_context(magma_connection(device), context_id);
   return 0;
}

int anv_gem_get_aperture(int fd, uint64_t* size)
{
   DLOG("anv_gem_get_aperture - STUB");
   return 0;
}

int anv_gem_handle_to_fd(anv_device* device, anv_buffer_handle_t gem_handle)
{
   int fd;
   auto result = magma_export_fd(magma_connection(device), gem_handle, &fd);
   DASSERT(result == MAGMA_STATUS_OK);
   return fd;
}

anv_buffer_handle_t anv_gem_fd_to_handle(anv_device* device, int fd, uint64_t* size_out)
{
   magma_buffer_t buffer;
   magma_status_t status = magma_import_fd(magma_connection(device), fd, &buffer);
   if (status != MAGMA_STATUS_OK)
      return 0;

   *size_out = magma_get_buffer_size(buffer);
   return buffer;
}

int anv_gem_gpu_get_reset_stats(struct anv_device *device,
                            uint32_t *active, uint32_t *pending)
{
   DLOG("anv_gem_gpu_get_reset_stats - STUB");
   *active = 0;
   *pending = 0;
   return 0;
}

VkResult anv_GetMemoryFuchsiaHandleKHR(VkDevice vk_device,
                                       const VkMemoryGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo,
                                       uint32_t* pHandle)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_device_memory, memory, pGetFuchsiaHandleInfo->memory);

   assert(pGetFuchsiaHandleInfo->sType == VK_STRUCTURE_TYPE_MEMORY_GET_FUCHSIA_HANDLE_INFO_KHR);
   assert(pGetFuchsiaHandleInfo->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_FUCHSIA_VMO_BIT_KHR);

   auto result = magma_export(magma_connection(device), memory->bo->gem_handle, pHandle);
   DASSERT(result == MAGMA_STATUS_OK);

   return VK_SUCCESS;
}

VkResult anv_GetMemoryFuchsiaHandlePropertiesKHR(
    VkDevice vk_device, VkExternalMemoryHandleTypeFlagBitsKHR handleType, uint32_t handle,
    VkMemoryFuchsiaHandlePropertiesKHR* pMemoryFuchsiaHandleProperties)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);

   assert(handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_FUCHSIA_VMO_BIT_KHR);
   assert(pMemoryFuchsiaHandleProperties->sType ==
          VK_STRUCTURE_TYPE_MEMORY_FUCHSIA_HANDLE_PROPERTIES_KHR);

   struct anv_physical_device* pdevice = &device->instance->physicalDevice;
   // All memory types supported
   pMemoryFuchsiaHandleProperties->memoryTypeBits = (1ull << pdevice->memory.type_count) - 1;

   return VK_SUCCESS;
}

VkResult anv_ImportSemaphoreFuchsiaHandleKHR(VkDevice vk_device,
                                             const VkImportSemaphoreFuchsiaHandleInfoKHR* info)
{
   assert(info->sType == VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FUCHSIA_HANDLE_INFO_KHR);
   assert(info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FUCHSIA_FENCE_BIT_KHR);

   bool permanent = (info->flags & VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR) == 0;
   ANV_FROM_HANDLE(anv_device, device, vk_device);

   anv_platform_semaphore_t imported_semaphore;
   if (anv_platform_import_semaphore(device, info->handle, &imported_semaphore) != 0)
      return vk_error(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR);

   struct anv_semaphore* semaphore = (struct anv_semaphore*)info->semaphore;
   assert(semaphore);

   if (semaphore->current_platform_semaphore != semaphore->original_platform_semaphore)
      anv_platform_destroy_semaphore(device, semaphore->current_platform_semaphore);

   semaphore->current_platform_semaphore = imported_semaphore;

   if (permanent && semaphore->original_platform_semaphore) {
      anv_platform_destroy_semaphore(device, semaphore->original_platform_semaphore);
      semaphore->original_platform_semaphore = 0;
   }

   return VK_SUCCESS;
}

VkResult anv_GetSemaphoreFuchsiaHandleKHR(VkDevice vk_device,
                                          const VkSemaphoreGetFuchsiaHandleInfoKHR* info,
                                          uint32_t* pFuchsiaHandle)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   assert(info->sType == VK_STRUCTURE_TYPE_SEMAPHORE_GET_FUCHSIA_HANDLE_INFO_KHR);

   if (info->handleType != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_FUCHSIA_FENCE_BIT_KHR)
      return VK_SUCCESS;

   anv_platform_semaphore_t semaphore =
       ((struct anv_semaphore*)info->semaphore)->current_platform_semaphore;

   uint32_t handle;
   if (anv_platform_export_semaphore(device, semaphore, &handle) != 0)
      return vk_error(VK_ERROR_TOO_MANY_OBJECTS);

   *pFuchsiaHandle = handle;
   return VK_SUCCESS;
}
