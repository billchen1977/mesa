// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h> // for MAP_FAILED
#include "anv_magma.h"
#include "anv_private.h"
#include "gen_device_info.h" // for gen_getparam
#include "magma_util/dlog.h"
#include "msd_intel_gen_query.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(__Fuchsia__)
#include "os/fuchsia.h"
#endif

static magma_connection_t magma_connection(struct anv_device* device)
{
   assert(device);
   assert(device->connection);
   return device->connection->connection;
}

static magma_buffer_t magma_buffer(anv_buffer_handle_t buffer_handle)
{
   assert(buffer_handle);
   return ((struct anv_magma_buffer*)buffer_handle)->buffer;
}

int anv_gem_connect(struct anv_device* device)
{
   magma_connection_t connection;
   magma_status_t status = magma_create_connection2((magma_device_t)device->handle, &connection);
   if (status != MAGMA_STATUS_OK || !connection) {
      DLOG("magma_create_connection failed: %d", status);
      return -1;
   }

   device->connection = AnvMagmaCreateConnection(connection);

   DLOG("created magma connection");
   return 0;
}

void anv_gem_disconnect(struct anv_device* device)
{
   AnvMagmaReleaseConnection(device->connection);
   DLOG("released magma connection");
}

// Return handle, or 0 on failure. Gem handles are never 0.
anv_buffer_handle_t anv_gem_create(struct anv_device* device, size_t size)
{
   magma_buffer_t buffer;
   uint64_t magma_size = size;
   magma_status_t status =
       magma_create_buffer(magma_connection(device), magma_size, &magma_size, &buffer);
   if (status != MAGMA_STATUS_OK) {
      DLOG("magma_create_buffer failed (%d) size 0x%zx", status, magma_size);
      return 0;
   }

   assert(buffer);
   DLOG("magma_create_buffer size 0x%zx returning buffer %lu", magma_size, magma_get_buffer_id(buffer));

   return (anv_buffer_handle_t)AnvMagmaCreateBuffer(device->connection, buffer);
}

void anv_gem_close(struct anv_device* device, anv_buffer_handle_t handle)
{
   DLOG("anv_gem_close handle 0x%lx", handle);
   AnvMagmaReleaseBuffer(device->connection, (struct anv_magma_buffer*)handle);
}

void* anv_gem_mmap(struct anv_device* device, anv_buffer_handle_t handle, uint64_t offset,
                   uint64_t size, uint32_t flags)
{
   assert(flags == 0);
   void* addr;
   magma_status_t status = magma_map(magma_connection(device), magma_buffer(handle), &addr);
   if (status != MAGMA_STATUS_OK) {
      DLOG("magma_map failed: status %d", status);
      return MAP_FAILED;
   }
   DLOG("magma_map buffer %lu size 0x%zx returning %p", magma_get_buffer_id(magma_buffer(handle)), size, addr);
   return (uint8_t*)addr + offset;
}

void anv_gem_munmap(struct anv_device* device, anv_buffer_handle_t handle, void* addr,
                    uint64_t size)
{
   if (!addr)
      return;

   if (magma_unmap(magma_connection(device), magma_buffer(handle)) != 0) {
      DLOG("magma_unmap failed");
      return;
   }

   DLOG("magma_unmap buffer %lu", magma_get_buffer_id(magma_buffer(handle)));
}

uint32_t anv_gem_userptr(struct anv_device* device, void* mem, size_t size)
{
   DLOG("anv_gem_userptr - STUB");
   assert(false);
   return 0;
}

int anv_gem_set_caching(struct anv_device* device, anv_buffer_handle_t gem_handle, uint32_t caching)
{
   DLOG("anv_get_set_caching - STUB");
   return 0;
}

int anv_gem_set_domain(struct anv_device* device, anv_buffer_handle_t gem_handle,
                       uint32_t read_domains, uint32_t write_domain)
{
   DLOG("anv_gem_set_domain - STUB");
   return 0;
}

/**
 * On error, \a timeout_ns holds the remaining time.
 */
int anv_gem_wait(struct anv_device* device, anv_buffer_handle_t handle, int64_t* timeout_ns)
{
   uint64_t buffer_id = magma_get_buffer_id(magma_buffer(handle));
   DLOG("anv_gem_wait buffer_id %lu timeout_ns %lu", buffer_id, *timeout_ns);
   magma_status_t status = AnvMagmaConnectionWait(device->connection, buffer_id, timeout_ns);
   switch (status) {
   case MAGMA_STATUS_OK:
      break;
   case MAGMA_STATUS_TIMED_OUT:
      errno = ETIME;
      return -1;
   default:
      return -1;
   }
   return 0;
}

/**
 * Returns 0, 1, or negative to indicate error
 */
int anv_gem_busy(struct anv_device* device, anv_buffer_handle_t handle)
{
   DLOG("anv_gem_busy");
   return AnvMagmaConnectionIsBusy(device->connection, magma_get_buffer_id(magma_buffer(handle)));
}

bool anv_gem_supports_48b_addresses(anv_device_handle_t handle)
{
   uint64_t gtt_size;
   magma_status_t status =
       magma_query2((magma_device_t)handle, kMsdIntelGenQueryGttSize, &gtt_size);
   if (status != MAGMA_STATUS_OK) {
      DLOG("magma_query failed: %d", status);
      return false;
   }
   return gtt_size >= 1ul << 48;
}

int anv_gem_get_context_param(anv_device_handle_t handle, int context, uint32_t param,
                              uint64_t* value)
{
   magma_status_t status;
   switch (param) {
   case I915_CONTEXT_PARAM_GTT_SIZE:
      status = magma_query2((magma_device_t)handle, kMsdIntelGenQueryGttSize, value);
      if (status != MAGMA_STATUS_OK) {
         DLOG("magma_query failed: %d", status);
         return -1;
      }
      return 0;
   default:
      DLOG("anv_gem_get_context_param: unhandled param 0x%x", param);
      return -1;
   }
}

int anv_gem_execbuffer(struct anv_device* device, struct drm_i915_gem_execbuffer2* execbuf)
{
   DLOG("anv_gem_execbuffer");
   return AnvMagmaConnectionExec(device->connection, device->context_id, execbuf);
}

int anv_gem_set_tiling(struct anv_device* device, anv_buffer_handle_t gem_handle, uint32_t stride,
                       uint32_t tiling)
{
   DLOG("anv_gem_set_tiling - STUB");
   return 0;
}

#if VK_USE_PLATFORM_FUCHSIA
typedef VkResult(VKAPI_PTR* PFN_vkGetServiceAddr)(const char* pName, uint32_t handle);

PUBLIC VKAPI_ATTR void VKAPI_CALL
vk_icdInitializeConnectToServiceCallback(PFN_vkGetServiceAddr get_services_addr)
{
   fuchsia_init(get_services_addr);
}

#endif // VK_USE_PLATFORM_FUCHSIA

VkResult anv_magma_open_device_handle(const char* path, anv_device_handle_t* device_out)
{
   magma_device_t device;
#if defined(__Fuchsia__)
   zx_handle_t client_handle;
   if (!fuchsia_open(path, &client_handle)) {
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }
   if (magma_device_import(client_handle, &device) != MAGMA_STATUS_OK) {
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }
#else
   int fd;
   fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   if (magma_device_import(fd, &device) != MAGMA_STATUS_OK) {
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }
#endif
   *device_out = (anv_device_handle_t)device;
   return VK_SUCCESS;
}

void anv_magma_release_device_handle(anv_device_handle_t device)
{
   magma_device_release((magma_device_t)device);
}

int anv_gem_get_param(anv_device_handle_t fd, uint32_t param)
{
   int value;
   if (!gen_getparam((uintptr_t)fd, param, &value))
      return 0;
   return value;
}

bool anv_gem_get_bit6_swizzle(anv_device_handle_t fd, uint32_t tiling)
{
   DLOG("anv_gem_get_bit6_swizzle - STUB");
   return 0;
}

int anv_gem_create_context(struct anv_device* device)
{
   uint32_t context_id;
   magma_create_context(magma_connection(device), &context_id);
   DLOG("magma_create_context returned context_id %u", context_id);
   return context_id;
}

int anv_gem_destroy_context(struct anv_device* device, int context_id)
{
   magma_release_context(magma_connection(device), context_id);
   return 0;
}

int anv_gem_get_aperture(anv_device_handle_t fd, uint64_t* size)
{
   DLOG("anv_gem_get_aperture - STUB");
   return 0;
}

int anv_gem_handle_to_fd(struct anv_device* device, anv_buffer_handle_t gem_handle)
{
   uint32_t handle = 0;
   magma_status_t result = magma_export(magma_connection(device), magma_buffer(gem_handle), &handle);
   assert(result == MAGMA_STATUS_OK);
   return (int)handle;
}

anv_buffer_handle_t anv_gem_fd_to_handle(struct anv_device* device, int fd)
{
   uint32_t handle = (uint32_t)fd;
   magma_buffer_t buffer;
   magma_status_t result = magma_import(magma_connection(device), handle, &buffer);
   assert(result == MAGMA_STATUS_OK);
   return (anv_buffer_handle_t)AnvMagmaCreateBuffer(device->connection, buffer);
}

int anv_gem_gpu_get_reset_stats(struct anv_device* device, uint32_t* active, uint32_t* pending)
{
   DLOG("anv_gem_gpu_get_reset_stats - STUB");
   *active = 0;
   *pending = 0;
   return 0;
}

int anv_gem_import_fuchsia_buffer(struct anv_device* device, uint32_t handle,
                                  anv_buffer_handle_t* buffer_out, uint64_t* size_out)
{
   magma_buffer_t buffer;
   magma_status_t status = magma_import(magma_connection(device), handle, &buffer);
   if (status != MAGMA_STATUS_OK) {
      DLOG("magma_import failed: %d", status);
      return -EINVAL;
   }

   *size_out = magma_get_buffer_size(buffer);
   *buffer_out = (anv_buffer_handle_t)AnvMagmaCreateBuffer(device->connection, buffer);
   return 0;
}

#if VK_USE_PLATFORM_FUCHSIA
VkResult
anv_GetMemoryZirconHandleFUCHSIA(VkDevice vk_device,
                                 const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
                                 uint32_t* pHandle)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_device_memory, memory, pGetZirconHandleInfo->memory);

   assert(pGetZirconHandleInfo->sType ==
          VK_STRUCTURE_TYPE_TEMP_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA);
   assert(pGetZirconHandleInfo->handleType ==
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA);

   magma_status_t result =
       magma_export(magma_connection(device), magma_buffer(memory->bo->gem_handle), pHandle);
   assert(result == MAGMA_STATUS_OK);

   return VK_SUCCESS;
}

VkResult anv_GetMemoryZirconHandlePropertiesFUCHSIA(
    VkDevice vk_device, VkExternalMemoryHandleTypeFlagBitsKHR handleType, uint32_t handle,
    VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);

   assert(handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA);
   assert(pMemoryZirconHandleProperties->sType ==
          VK_STRUCTURE_TYPE_TEMP_MEMORY_ZIRCON_HANDLE_PROPERTIES_FUCHSIA);

   struct anv_physical_device* pdevice = &device->instance->physicalDevice;
   // Duplicate handle because import takes ownership of the handle.
   uint32_t handle_duplicate;
   magma_status_t status = magma_duplicate_handle(handle, &handle_duplicate);
   if (status != MAGMA_STATUS_OK) {
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   }

   magma_buffer_t buffer;
   status = magma_import(magma_connection(device), handle_duplicate, &buffer);
   if (status != MAGMA_STATUS_OK) {
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   }

   magma_bool_t is_mappable;
   status = magma_get_buffer_is_mappable(buffer, 0u, &is_mappable);
   magma_release_buffer(magma_connection(device), buffer);
   if (status != MAGMA_STATUS_OK) {
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   }
   if (!is_mappable) {
      pMemoryZirconHandleProperties->memoryTypeBits = 0;
   } else {
      // All memory types supported
      pMemoryZirconHandleProperties->memoryTypeBits = (1ull << pdevice->memory.type_count) - 1;
   }

   return VK_SUCCESS;
}

/* Similar to anv_ImportSemaphoreFdKHR */
VkResult
anv_ImportSemaphoreZirconHandleFUCHSIA(VkDevice vk_device,
                                       const VkImportSemaphoreZirconHandleInfoFUCHSIA* info)
{
   assert(info->sType == VK_STRUCTURE_TYPE_TEMP_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA);
   assert(info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA);

   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_semaphore, semaphore, info->semaphore);

   magma_semaphore_t magma_semaphore;
   magma_status_t status =
       magma_import_semaphore(magma_connection(device), info->handle, &magma_semaphore);
   if (status != MAGMA_STATUS_OK)
      return vk_error(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR);

   struct anv_semaphore_impl new_impl = {.type = ANV_SEMAPHORE_TYPE_DRM_SYNCOBJ};
   new_impl.syncobj = magma_semaphore;

   if (info->flags & VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR) {
      anv_semaphore_impl_cleanup(device, &semaphore->temporary);
      semaphore->temporary = new_impl;
   } else {
      anv_semaphore_impl_cleanup(device, &semaphore->permanent);
      semaphore->permanent = new_impl;
   }

   return VK_SUCCESS;
}

/* Similar to anv_GetSemaphoreFdKHR */
VkResult anv_GetSemaphoreZirconHandleFUCHSIA(VkDevice vk_device,
                                             const VkSemaphoreGetZirconHandleInfoFUCHSIA* info,
                                             uint32_t* pZirconHandle)
{
   ANV_FROM_HANDLE(anv_device, device, vk_device);
   ANV_FROM_HANDLE(anv_semaphore, semaphore, info->semaphore);

   assert(info->sType == VK_STRUCTURE_TYPE_TEMP_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA);

   if (info->handleType != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA)
      return VK_SUCCESS;

   struct anv_semaphore_impl* impl = semaphore->temporary.type != ANV_SEMAPHORE_TYPE_NONE
                                         ? &semaphore->temporary
                                         : &semaphore->permanent;

   uint32_t handle;
   magma_status_t status = magma_export_semaphore(magma_connection(device), impl->syncobj, &handle);
   if (status != MAGMA_STATUS_OK)
      return vk_error(VK_ERROR_TOO_MANY_OBJECTS);

   /* From the Vulkan 1.0.53 spec:
    *
    *    "Export operations have the same transference as the specified handle
    *    type’s import operations. [...] If the semaphore was using a
    *    temporarily imported payload, the semaphore’s prior permanent payload
    *    will be restored.
    */
   anv_semaphore_reset_temporary(device, semaphore);

   *pZirconHandle = handle;
   return VK_SUCCESS;
}

#endif // VK_USE_PLATFORM_FUCHSIA

bool anv_gem_supports_syncobj_wait(anv_device_handle_t fd) { return true; }

anv_syncobj_handle_t anv_gem_syncobj_create(struct anv_device* device, uint32_t flags)
{
   magma_semaphore_t semaphore;
   magma_status_t status = magma_create_semaphore(magma_connection(device), &semaphore);
   if (status != MAGMA_STATUS_OK) {
      DLOG("magma_create_semaphore failed: %d", status);
      return 0;
   }
   if (flags & DRM_SYNCOBJ_CREATE_SIGNALED)
      magma_signal_semaphore(semaphore);
   return semaphore;
}

void anv_gem_syncobj_destroy(struct anv_device* device, anv_syncobj_handle_t semaphore)
{
   magma_release_semaphore(magma_connection(device), semaphore);
}

void anv_gem_syncobj_reset(struct anv_device* device, anv_syncobj_handle_t fence)
{
   magma_reset_semaphore(fence);
}

static uint64_t gettime_ns(void)
{
   struct timespec current;
   clock_gettime(CLOCK_MONOTONIC, &current);
#define NSEC_PER_SEC 1000000000
   return (uint64_t)current.tv_sec * NSEC_PER_SEC + current.tv_nsec;
#undef NSEC_PER_SEC
}

static int64_t anv_get_relative_timeout(uint64_t abs_timeout)
{
   uint64_t now = gettime_ns();

   if (abs_timeout < now)
      return 0;
   return abs_timeout - now;
}

static inline uint64_t ns_to_ms(uint64_t ns) { return ns / 1000000ull; }

int anv_gem_syncobj_wait(struct anv_device* device, anv_syncobj_handle_t* fences,
                         uint32_t fence_count, int64_t abs_timeout_ns, bool wait_all)
{
   int64_t timeout_ns = anv_get_relative_timeout(abs_timeout_ns);
   magma_status_t status =
       magma_wait_semaphores(fences, fence_count, ns_to_ms(timeout_ns), wait_all);
   switch (status) {
   case MAGMA_STATUS_OK:
      break;
   case MAGMA_STATUS_TIMED_OUT:
      errno = ETIME;
      // fall through
   default:
      return -1;
   }
   return 0;
}

int anv_gem_reg_read(struct anv_device *device,
                     uint32_t offset, uint64_t *result)
{
   // TODO(MA-643)
   assert(false);
   return 0;
}

anv_syncobj_handle_t anv_gem_syncobj_fd_to_handle(struct anv_device* device, int fd)
{
   assert(false);
   return 0;
}

int anv_gem_syncobj_handle_to_fd(struct anv_device* device, anv_syncobj_handle_t handle)
{
   assert(false);
   return -1;
}

int anv_gem_syncobj_export_sync_file(struct anv_device* device, anv_syncobj_handle_t handle)
{
   assert(false);
   return -1;
}

int anv_gem_syncobj_import_sync_file(struct anv_device* device, anv_syncobj_handle_t handle, int fd)
{
   assert(false);
   return -1;
}

int anv_gem_sync_file_merge(struct anv_device* device, int fd1, int fd2)
{
   assert(false);
   return -1;
}

int anv_gem_set_context_param(anv_device_handle_t handle, int context, uint32_t param,
                              uint64_t value)
{
   assert(false);
   return -1;
}

bool anv_gem_has_context_priority(anv_device_handle_t fd) { return false; }
