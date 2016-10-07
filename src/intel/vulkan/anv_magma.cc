// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "anv_private.h"

// Return handle, or 0 on failure. Gem handles are never 0.
uint32_t anv_gem_create(anv_device *device, size_t size) {
  printf("anv_gem_create - STUB\n");
  return 0;
}

void anv_gem_close(anv_device *device, uint32_t handle) {
  printf("anv_gem_close - STUB\n");
}

void *anv_gem_mmap(anv_device *device, uint32_t handle, uint64_t offset,
                   uint64_t size, uint32_t flags) {
  printf("anv_gem_mmap - STUB\n");
  return nullptr;
}

void anv_gem_munmap(void *addr, uint64_t size) {
  printf("anv_gem_munmap - STUB needs handle\n");
}

uint32_t anv_gem_userptr(anv_device *device, void *mem, size_t size) {
  printf("anv_gem_userptr - STUB\n");
  return 0;
}

int anv_gem_set_caching(anv_device *device, uint32_t gem_handle,
                        uint32_t caching) {
  printf("anv_get_set_caching - STUB\n");
  return 0;
}

int anv_gem_set_domain(anv_device *device, uint32_t gem_handle,
                       uint32_t read_domains, uint32_t write_domain) {
  printf("anv_gem_set_domain - STUB\n");
  return 0;
}

/**
 * On error, \a timeout_ns holds the remaining time.
 */
int anv_gem_wait(anv_device *device, uint32_t handle, int64_t *timeout_ns) {
  printf("anv_gem_wait - STUB\n");
  return 0;
}

int anv_gem_execbuffer(anv_device *device, drm_i915_gem_execbuffer2 *execbuf) {
  printf("anv_gem_execbuffer - STUB\n");
  return 0;
}

int anv_gem_set_tiling(anv_device *device, uint32_t gem_handle, uint32_t stride,
                       uint32_t tiling) {
  printf("anv_gem_set_tiling - STUB\n");
  return 0;
}

int anv_gem_get_param(int fd, uint32_t param) {
  printf("anv_gem_get_param - STUB\n");
  return 0;
}

bool anv_gem_get_bit6_swizzle(int fd, uint32_t tiling) {
  printf("anv_gem_get_bit6_swizzle - STUB\n");
  return 0;
}

int anv_gem_create_context(anv_device *device) {
  printf("anv_gem_create_context - STUB\n");
  return 0;
}

int anv_gem_destroy_context(anv_device *device, int context) {
  printf("anv_gem_destroy_context - STUB\n");
  return 0;
}

int anv_gem_get_aperture(int fd, uint64_t *size) {
  printf("anv_gem_get_aperture - STUB\n");
  return 0;
}

int anv_gem_handle_to_fd(anv_device *device, uint32_t gem_handle) {
  printf("anv_gem_handle_to_fd - STUB\n");
  return 0;
}

uint32_t anv_gem_fd_to_handle(anv_device *device, int fd) {
  printf("anv_gem_fd_to_handle - STUB\n");
  return 0;
}
