// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "os_dirent.h"

#include "list.h"
#include "os/fuchsia.h"

#include <fuchsia/io/llcpp/fidl.h>
#include <lib/zx/channel.h>
#include <magma_util/dlog.h>

namespace fio = llcpp::fuchsia::io;

// Offsets must match struct os_dir, static_asserts below.
struct os_dirent_impl {
   ino_t d_ino;
   char d_name[NAME_MAX];
};

static_assert(offsetof(struct os_dirent_impl, d_ino) == offsetof(struct os_dirent, d_ino),
              "d_ino offset mismatch");
static_assert(offsetof(struct os_dirent_impl, d_name) == offsetof(struct os_dirent, d_name),
              "d_ino offset mismatch");

// From io.fidl:
struct __attribute__((__packed__)) dirent {
   uint64_t ino;
   uint8_t size;
   uint8_t type;
   char name[0];
};

static zx_status_t readdir(const zx::channel& control_channel, void* buffer, size_t capacity,
                           size_t* out_actual)
{
   // Explicitly allocating message buffers to avoid heap allocation.
   fidl::Buffer<fio::Directory::ReadDirentsRequest> request_buffer;
   fidl::Buffer<fio::Directory::ReadDirentsResponse> response_buffer;
   auto result =
       fio::Directory::Call::ReadDirents(zx::unowned_channel(control_channel),
                                         request_buffer.view(), capacity, response_buffer.view());
   if (result.status() != ZX_OK)
      return result.status();

   fio::Directory::ReadDirentsResponse* response = result.Unwrap();
   if (response->s != ZX_OK)
      return response->s;

   const auto& dirents = response->dirents;
   if (dirents.count() > capacity)
      return ZX_ERR_IO;

   memcpy(buffer, dirents.data(), dirents.count());
   *out_actual = dirents.count();
   return response->s;
}

struct os_dir {
   os_dir() { buffer = new char[buffer_size()]; }
   ~os_dir() { delete [] buffer; }

   static size_t buffer_size() { return 4096; }

   zx::channel control_channel;
   struct os_dirent_impl entry;
   char* buffer = nullptr;
   size_t count = 0;
   size_t index = 0;
};

os_dir_t* os_opendir(const char* path)
{
   zx::channel control_channel;
   if (!fuchsia_open(path, control_channel.reset_and_get_address())) {
      DLOG("fuchsia_open(%s) failed\n", path);
      return nullptr;
   }

   auto dir = new os_dir();
   dir->control_channel = std::move(control_channel);

   return dir;
}

int os_closedir(os_dir_t* dir)
{
   // Assume param is valid.
   delete dir;
   return 0;
}

struct os_dirent* os_readdir(os_dir_t* dir)
{
   if (dir->index >= dir->count) {
      size_t count;
      zx_status_t status =
          readdir(dir->control_channel, dir->buffer, os_dir::buffer_size(), &count);
      if (status != ZX_OK) {
         DLOG("os_readdir: readdir failed: %d\n", status);
         return nullptr;
      }

      dir->count = count;
      dir->index = 0;
   }

   if (dir->index + sizeof(dirent) > dir->count) {
      DLOG("os_readdir: no more entries");
      return nullptr;
   }

   dirent* entry = reinterpret_cast<dirent*>(&dir->buffer[dir->index]);
   size_t entry_size = sizeof(dirent) + entry->size;

   if (dir->index + entry_size > dir->count) {
      DLOG("os_readdir: last entry incomplete (this shouldn't happen)");
      return nullptr;
   }
   dir->index += entry_size;

   dir->entry.d_ino = entry->ino;
   strncpy(dir->entry.d_name, entry->name, entry->size);
   dir->entry.d_name[entry->size] = 0;

   return reinterpret_cast<os_dirent*>(&dir->entry);
}
