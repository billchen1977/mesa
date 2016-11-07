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

#include <unordered_map>
#include <utility>

struct Bundle {
   anv_device* device;
   uint32_t handle;
};

std::unordered_map<void*, Bundle> g_map_of_maps;

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
   DASSERT(device->connection);
   magma_system_close(device->connection);
   DLOG("closed the magma system connection");
}

// Return handle, or 0 on failure. Gem handles are never 0.
uint32_t anv_gem_create(anv_device* device, size_t size)
{
   DASSERT(device->connection);
   uint32_t handle;
   uint64_t magma_size = size;
   if (magma_system_alloc(device->connection, magma_size, &magma_size, &handle) != 0) {
      DLOG("magma_system_alloc failed size 0x%llx", magma_size);
      return 0;
   }
   DASSERT(handle != 0);
   DLOG("magma_system_alloc size 0x%llx returning handle 0x%x", magma_size, handle);
   return handle;
}

void anv_gem_close(anv_device* device, uint32_t handle)
{
   magma_system_free(device->connection, handle);
}

void* anv_gem_mmap(anv_device* device, uint32_t handle, uint64_t offset, uint64_t size,
                   uint32_t flags)
{
   DASSERT(device->connection);
   DASSERT(offset == 0);
   DASSERT(flags == 0);
   void* addr;
   if (magma_system_map(device->connection, handle, &addr) != 0)
      return DRETP(nullptr, "magma_system_map failed");
   DLOG("magma_system_map handle 0x%x size 0x%llx returning %p", handle, size, addr);
   g_map_of_maps[addr] = Bundle{device, handle};
   return addr;
}

void anv_gem_munmap(void* addr, uint64_t size)
{
   auto iter = g_map_of_maps.find(addr);
   if (iter == g_map_of_maps.end()) {
      DASSERT(false)
      return;
   }

   if (magma_system_unmap(iter->second.device->connection, iter->second.handle, addr) != 0)
      DLOG("magma_system_unmap failed");

   DLOG("magma_system_unmap handle 0x%x", iter->second.handle);
   g_map_of_maps.erase(iter);
}

uint32_t anv_gem_userptr(anv_device* device, void* mem, size_t size)
{
   DLOG("anv_gem_userptr - STUB");
   DASSERT(false);
   return 0;
}

int anv_gem_set_caching(anv_device* device, uint32_t gem_handle, uint32_t caching)
{
   DLOG("anv_get_set_caching - STUB");
   return 0;
}

int anv_gem_set_domain(anv_device* device, uint32_t gem_handle, uint32_t read_domains,
                       uint32_t write_domain)
{
   DLOG("anv_gem_set_domain - STUB");
   return 0;
}

/**
 * On error, \a timeout_ns holds the remaining time.
 */
int anv_gem_wait(anv_device* device, uint32_t handle, int64_t* timeout_ns)
{
   DASSERT(device->connection);
   magma_system_wait_rendering(device->connection, handle);
   return 0;
}

int anv_gem_execbuffer(anv_device* device, drm_i915_gem_execbuffer2* execbuf)
{
   DLOG("anv_gem_execbuffer");
   DASSERT(device->connection);

   if (execbuf->buffer_count == 0)
      return 0;

   std::unique_ptr<DrmCommandBuffer> command_buffer = DrmCommandBuffer::Create(execbuf);

   if (!command_buffer)
      return DRET_MSG(-1, "DrmCommandBuffer creation failed");

   magma_system_submit_command_buffer(device->connection, command_buffer->system_command_buffer(),
                                      device->context_id);

   return 0;
}

int anv_gem_set_tiling(anv_device* device, uint32_t gem_handle, uint32_t stride, uint32_t tiling)
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
   DASSERT(device->connection);
   uint32_t context_id;

   magma_system_create_context(device->connection, &context_id);
   DLOG("magma_system_create_context returned context_id %u", context_id);

   return static_cast<int>(context_id);
}

int anv_gem_destroy_context(anv_device* device, int context_id)
{
   DASSERT(device->connection);
   magma_system_destroy_context(device->connection, context_id);
   return 0;
}

int anv_gem_get_aperture(int fd, uint64_t* size)
{
   DLOG("anv_gem_get_aperture - STUB");
   return 0;
}

int anv_gem_handle_to_fd(anv_device* device, uint32_t gem_handle)
{
   DLOG("anv_gem_handle_to_fd - STUB");
   return 0;
}

uint32_t anv_gem_fd_to_handle(anv_device* device, int fd)
{
   DLOG("anv_gem_fd_to_handle - STUB");
   return 0;
}
