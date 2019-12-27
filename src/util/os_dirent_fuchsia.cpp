/*
 * Copyright © 2019 Google, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "os_dirent.h"

#include "list.h"
#include "os/fuchsia.h"

#include <lib/zxio/inception.h>
#include <lib/zxio/zxio.h>

// Offsets must match struct os_dirent, static_asserts below.
struct os_dirent_impl {
   ino_t d_ino;
   char d_name[NAME_MAX];
};

static_assert(offsetof(struct os_dirent_impl, d_ino) == offsetof(struct os_dirent, d_ino),
              "d_ino offset mismatch");
static_assert(offsetof(struct os_dirent_impl, d_name) == offsetof(struct os_dirent, d_name),
              "d_ino offset mismatch");

#ifndef ZXIO_DIRENT_ITERATOR_DEFAULT_BUFFER_SIZE

class os_dir {
public:
   os_dir() {}

   ~os_dir()
   {
      if (dir_iterator_init) {
         zxio_dirent_iterator_destroy(&iterator);
      }
      if (dir_init) {
         zxio_close(&io_storage.io);
      }
   }

   // Always consumes |dir_channel|
   bool Init(zx_handle_t dir_channel)
   {
      zx_status_t status = zxio_dir_init(&io_storage, dir_channel);
      if (status != ZX_OK) {
         FUCHSIA_DLOG("zxio_dir_init failed: %d", status);
         return false;
      }
      dir_init = true;

      status = zxio_dirent_iterator_init(&iterator, &io_storage.io);
      if (status != ZX_OK) {
         FUCHSIA_DLOG("zxio_dirent_iterator_init failed: %d", status);
         return false;
      }
      dir_iterator_init = true;

      return true;
   }

   struct os_dirent_impl* Next()
   {
      zxio_dirent_t* dirent;
      zx_status_t status = zxio_dirent_iterator_next(&iterator, &dirent);
      if (status != ZX_OK) {
         FUCHSIA_DLOG("zxio_dirent_iterator_next failed: %d", status);
         return nullptr;
      }

      entry.d_ino = dirent->has.id ? dirent->id : OS_INO_UNKNOWN;
      strncpy(entry.d_name, dirent->name, dirent->name_length);
      entry.d_name[dirent->name_length] = 0;

      return &entry;
   }

private:
   bool dir_init = false;
   bool dir_iterator_init = false;
   zxio_storage_t io_storage;
   zxio_dirent_iterator_t iterator;
   struct os_dirent_impl entry;
};

#else

class os_dir {
public:
   os_dir() { buffer = new char[buffer_size()]; }

   ~os_dir()
   {
      if (dir_init) {
         zxio_close(&io_storage.io);
      }
      delete[] buffer;
   }

   static size_t buffer_size() { return ZXIO_DIRENT_ITERATOR_DEFAULT_BUFFER_SIZE; }

   // Always consumes |dir_channel|
   bool Init(zx_handle_t dir_channel)
   {
      zx_status_t status = zxio_dir_init(&io_storage, dir_channel);
      if (status != ZX_OK) {
         FUCHSIA_DLOG("zxio_dir_init failed: %d", status);
         return false;
      }
      dir_init = true;

      status = zxio_dirent_iterator_init(&iterator, &io_storage.io, buffer, buffer_size());
      if (status != ZX_OK) {
         FUCHSIA_DLOG("zxio_dirent_iterator_init failed: %d", status);
         return false;
      }

      return true;
   }

   struct os_dirent_impl* Next()
   {
      zxio_dirent_t* dirent;
      zx_status_t status = zxio_dirent_iterator_next(&iterator, &dirent);
      if (status != ZX_OK) {
         FUCHSIA_DLOG("zxio_dirent_iterator_next failed: %d", status);
         return nullptr;
      }

      entry.d_ino = dirent->inode;
      strncpy(entry.d_name, dirent->name, dirent->size);
      entry.d_name[dirent->size] = 0;

      return &entry;
   }

private:
   bool dir_init = false;
   zxio_storage_t io_storage;
   zxio_dirent_iterator_t iterator;
   struct os_dirent_impl entry;
   char* buffer = nullptr;
};

#endif

os_dir_t* os_opendir(const char* path)
{
   zx_handle_t dir_channel;
   if (!fuchsia_open(path, &dir_channel)) {
      FUCHSIA_DLOG("fuchsia_open(%s) failed\n", path);
      return nullptr;
   }

   auto dir = new os_dir();

   if (!dir->Init(dir_channel)) {
      delete dir;
      return nullptr;
   }

   return dir;
}

int os_closedir(os_dir_t* dir)
{
   // Assume param is valid.
   delete dir;
   return 0;
}

struct os_dirent* os_readdir(os_dir_t* dir) { return reinterpret_cast<os_dirent*>(dir->Next()); }
