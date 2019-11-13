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

#include "util/os_dirent.h"
#include <gtest/gtest.h>

constexpr int kFileCount = 1000;

#ifdef __Fuchsia__
#include "os/fuchsia.h"

#include <fuchsia/io/llcpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/zx/channel.h>

static int fuchsia_open_callback(const char* name, zx_handle_t handle)
{
   return fdio_service_connect(name, handle);
}

struct String {
   ~String() { Reset(); }

   void Alloc(int size) { ptr = new char[size]; }

   void Reset()
   {
      delete[] ptr;
      ptr = nullptr;
   }

   char* ptr = nullptr;
};

static bool fuchsia_create_files(String files[kFileCount])
{
   for (uint32_t i = 0; i < kFileCount; i++) {
      zx::channel channel0, channel1;
      zx_status_t status = zx::channel::create(0, &channel0, &channel1);

      status = fdio_open(files[i].ptr,
                         ::llcpp::fuchsia::io::OPEN_FLAG_CREATE |
                             ::llcpp::fuchsia::io::OPEN_RIGHT_WRITABLE,
                         channel0.release());
      if (status != ZX_OK) {
         printf("fdio_open(%s) failed: %d\n", files[i].ptr, status);
         return false;
      }
   }
   return true;
}
#endif

TEST(os_dirent, readdir)
{
   const char* path = "/tmp";
   const int path_len = strlen(path);

   String files[kFileCount];
   for (uint32_t i = 0; i < kFileCount; i++) {
      files[i].Alloc(path_len + 16);
      sprintf(files[i].ptr, "%s/%u", path, i);
   }

#ifdef __Fuchsia__
   EXPECT_TRUE(fuchsia_create_files(files));
   fuchsia_init(fuchsia_open_callback);
#endif

   os_dir_t* dir = os_opendir(path);

   while (struct os_dirent* entry = os_readdir(dir)) {
      EXPECT_NE(0, entry->d_ino);

      String found_path;
      found_path.Alloc(path_len + strlen(entry->d_name) + 16);
      sprintf(found_path.ptr, "%s/%s", path, entry->d_name);

      for (uint32_t i = 0; i < kFileCount; i++) {
         if (files[i].ptr && strcmp(files[i].ptr, found_path.ptr) == 0) {
            files[i].Reset();
            break;
         }
      }
   }

   for (uint32_t i = 0; i < kFileCount; i++) {
      EXPECT_EQ(nullptr, files[i].ptr) << "Didn't find: " << files[i].ptr;
   }

   os_closedir(dir);

#ifdef __Fuchsia__
   // Don't bother removing tmp files
#endif
}
