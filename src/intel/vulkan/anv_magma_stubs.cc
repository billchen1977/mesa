
#include "anv_private.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
#include "magma_util/platform/platform_buffer.h"
#include <unordered_map>

static std::unordered_map<uint32_t, std::unique_ptr<magma::PlatformBuffer>> buffer_map;

int anv_gem_connect(struct anv_device *device)
{
   return 0;
}

uint32_t anv_gem_create(struct anv_device *device, size_t size)
{
   auto buffer = magma::PlatformBuffer::Create(size);
   uint32_t handle;
   if (!buffer->duplicate_handle(&handle))
     return 0;
   buffer_map[handle] = std::move(buffer);
   return handle;
}

void anv_gem_close(struct anv_device *device, uint32_t gem_handle)
{
   auto iter = buffer_map.find(gem_handle);
   if (iter != buffer_map.end())
     buffer_map.erase(iter);
}

void* anv_gem_mmap(struct anv_device *device, uint32_t gem_handle,
             uint64_t offset, uint64_t size, uint32_t flags)
{
   auto iter = buffer_map.find(gem_handle);
   if (iter == buffer_map.end())
     return DRETP(nullptr, "handle not found");
   void* addr;
   if (!iter->second->MapCpu(&addr))
     return DRETP(nullptr, "CpuMap failed");
   return addr;
}

/* This is just a wrapper around munmap, but it also notifies valgrind that
 * this map is no longer valid.  Pair this with anv_gem_mmap().
 */
void anv_gem_munmap(void* p, uint64_t size) {}

uint32_t anv_gem_userptr(struct anv_device *device, void *mem, size_t size)
{
   return -1;
}

int anv_gem_wait(struct anv_device *device, uint32_t gem_handle, int64_t *timeout_ns)
{
   return 0;
}

int anv_gem_execbuffer(struct anv_device *device,
                   struct drm_i915_gem_execbuffer2 *execbuf)
{
   return 0;
}

int anv_gem_set_tiling(struct anv_device *device,
                   uint32_t gem_handle, uint32_t stride, uint32_t tiling)
{
   return 0;
}

int anv_gem_set_caching(struct anv_device *device, uint32_t gem_handle,
                    uint32_t caching)
{
   return 0;
}

int anv_gem_set_domain(struct anv_device *device, uint32_t gem_handle,
                   uint32_t read_domains, uint32_t write_domain)
{
   return 0;
}

int
anv_gem_get_param(int fd, uint32_t param)
{
   unreachable("Unused");
}

bool
anv_gem_get_bit6_swizzle(int fd, uint32_t tiling)
{
   unreachable("Unused");
}

int
anv_gem_create_context(struct anv_device *device)
{
   unreachable("Unused");
}

int
anv_gem_destroy_context(struct anv_device *device, int context)
{
   unreachable("Unused");
}

int
anv_gem_get_aperture(int fd, uint64_t *size)
{
   unreachable("Unused");
}

int
anv_gem_handle_to_fd(struct anv_device *device, uint32_t gem_handle)
{
   unreachable("Unused");
}

uint32_t
anv_gem_fd_to_handle(struct anv_device *device, int fd)
{
   unreachable("Unused");
}
