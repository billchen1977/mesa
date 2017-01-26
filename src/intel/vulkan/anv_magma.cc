// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "drm_command_buffer.h"
#include "magma_system.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "magma_util/sleep.h"
// clang-format off
#include "anv_private.h"
// clang-format on

static magma_system_connection* magma_connection(anv_device* device)
{
   DASSERT(device);
   DASSERT(device->connection);
   return device->connection;
}

int anv_gem_connect(anv_device* device)
{
   device->connection = magma_system_open(device->fd, MAGMA_SYSTEM_CAPABILITY_RENDERING |
                                                          MAGMA_SYSTEM_CAPABILITY_DISPLAY);
   if (!device->connection)
      return DRET_MSG(-1, "magma_system_open failed");

   DLOG("opened a magma system connection");
   return 0;
}

void anv_gem_disconnect(anv_device* device)
{
   magma_system_close(magma_connection(device));
   DLOG("closed the magma system connection");
}

// Return handle, or 0 on failure. Gem handles are never 0.
anv_buffer_handle_t anv_gem_create(anv_device* device, size_t size)
{
   magma_buffer_t buffer;
   uint64_t magma_size = size;
   if (magma_system_alloc(magma_connection(device), magma_size, &magma_size, &buffer) != 0) {
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
   magma_system_free(magma_connection(device), handle);
}

void* anv_gem_mmap(anv_device* device, anv_buffer_handle_t handle, uint64_t offset, uint64_t size,
                   uint32_t flags)
{
   DASSERT(flags == 0);
   void* addr;
   if (magma_system_map(magma_connection(device), handle, &addr) != 0)
      return DRETP(nullptr, "magma_system_map failed");
   DLOG("magma_system_map handle 0x%lx size 0x%zx returning %p", handle, size, addr);
   return reinterpret_cast<uint8_t*>(addr) + offset;
}

void anv_gem_munmap(struct anv_device* device, anv_buffer_handle_t gem_handle, void* addr, uint64_t size)
{
   if (!addr)
      return;

   if (magma_system_unmap(magma_connection(device), gem_handle) != 0) {
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
   magma_system_wait_rendering(magma_connection(device), handle);
   return 0;
}

int anv_gem_execbuffer(anv_device* device, drm_i915_gem_execbuffer2* execbuf)
{
   DLOG("anv_gem_execbuffer");

   if (execbuf->buffer_count == 0)
      return 0;

   uint64_t required_size = DrmCommandBuffer::RequiredSize(execbuf);

   uint64_t allocated_size;
   uint64_t cmd_buf_id;
   int32_t error;

   error = magma_system_alloc(magma_connection(device), required_size, &allocated_size, &cmd_buf_id);
   if (error)
      return DRET_MSG(error, "magma_system_alloc failed size 0x%llx", required_size);

   DASSERT(allocated_size >= required_size);

   void* cmd_buf_data;
   error = magma_system_map(magma_connection(device), cmd_buf_id, &cmd_buf_data);
   if (error) {
      magma_system_free(magma_connection(device), cmd_buf_id);
      return DRET_MSG(error, "magma_system_map failed");
   }

   if (!DrmCommandBuffer::Translate(execbuf, cmd_buf_data)) {
      error = magma_system_unmap(magma_connection(device), cmd_buf_id);
      DASSERT(!error);
      magma_system_free(magma_connection(device), cmd_buf_id);
      return DRET_MSG(error, "DrmCommandBuffer::Translate failed");
   }

   magma_system_submit_command_buffer(magma_connection(device), cmd_buf_id, device->context_id);

   error = magma_system_unmap(magma_connection(device), cmd_buf_id);
   DASSERT(!error);

   magma_system_free(magma_connection(device), cmd_buf_id);

   return 0;
}

int anv_gem_set_tiling(anv_device* device, anv_buffer_handle_t gem_handle, uint32_t stride,
                       uint32_t tiling)
{
   DLOG("anv_gem_set_tiling - STUB");
   return 0;
}

int anv_gem_get_param(int fd, uint32_t param)
{
   int result = 0;

   switch (param) {
   case I915_PARAM_CHIPSET_ID:
      result = magma_system_get_device_id(fd);
      break;
   case I915_PARAM_HAS_WAIT_TIMEOUT:
   case I915_PARAM_HAS_EXECBUF2:
      result = 1;
      break;
   }

   DLOG("anv_gem_get_param(0x%x, %u) returning %d", fd, param, result);
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
   magma_system_create_context(magma_connection(device), &context_id);
   DLOG("magma_system_create_context returned context_id %u", context_id);

   return static_cast<int>(context_id);
}

int anv_gem_destroy_context(anv_device* device, int context_id)
{
   magma_system_destroy_context(magma_connection(device), context_id);
   return 0;
}

int anv_gem_get_aperture(int fd, uint64_t* size)
{
   DLOG("anv_gem_get_aperture - STUB");
   return 0;
}

int anv_gem_handle_to_fd(anv_device* device, anv_buffer_handle_t gem_handle)
{
   DLOG("anv_gem_handle_to_fd - STUB");
   return 0;
}

anv_buffer_handle_t anv_gem_fd_to_handle(anv_device* device, int fd)
{
   DLOG("anv_gem_fd_to_handle - STUB");
   return 0;
}

VkResult anv_ExportDeviceMemoryMAGMA(VkDevice _device, VkDeviceMemory _memory, uint32_t* pHandle)
{
   DLOG("anv_ExportDeviceMemoryMAGMA");

   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_device_memory, mem, _memory);

   auto result = magma_system_export(magma_connection(device), mem->bo.gem_handle, pHandle);
   DASSERT(result == MAGMA_STATUS_OK);

   return VK_SUCCESS;
}

VkResult anv_ImportDeviceMemoryMAGMA(VkDevice _device, uint32_t handle,
                                     const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMem)
{
   DLOG("anv_ImportDeviceMemoryMAGMA");
   ANV_FROM_HANDLE(anv_device, device, _device);

   struct anv_device_memory* mem = static_cast<struct anv_device_memory*>(
       vk_alloc2(&device->alloc, pAllocator, sizeof(*mem), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
   if (mem == nullptr)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   magma_buffer_t magma_buffer;
   auto result = magma_system_import(magma_connection(device), handle, &magma_buffer);
   DASSERT(result == MAGMA_STATUS_OK);

   anv_bo_init(&mem->bo, magma_buffer, magma_system_get_buffer_size(magma_buffer));

   *pMem = anv_device_memory_to_handle(mem);

   return VK_SUCCESS;
}
