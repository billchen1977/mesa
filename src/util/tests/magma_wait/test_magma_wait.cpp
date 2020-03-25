/*
 * Copyright © 2020 Google, LLC
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

#include <assert.h>
#include <filesystem>
#include <time.h>

#include <lib/fdio/directory.h>
#include <lib/zx/channel.h>

#include "util/magma_wait.h"
#include "gtest/gtest.h"

inline constexpr int64_t ms_to_ns(int64_t ms) { return ms * 1000000ull; }

static uint64_t gettime_ns(void)
{
   struct timespec current;
   clock_gettime(CLOCK_MONOTONIC, &current);
   return (uint64_t)current.tv_sec * ms_to_ns(1000) + current.tv_nsec;
}

class TestMagmaWait : public ::testing::Test {
public:
   void SetUp() override
   {
      for (auto& entry : std::filesystem::directory_iterator("/dev/class/gpu")) {
         zx::channel local, remote;
         ASSERT_EQ(ZX_OK, zx::channel::create(0 /*flags*/, &local, &remote));

         ASSERT_EQ(ZX_OK, fdio_service_connect(entry.path().c_str(), remote.release()));

         ASSERT_EQ(MAGMA_STATUS_OK, magma_device_import(local.release(), &device_));

         ASSERT_EQ(MAGMA_STATUS_OK, magma_create_connection2(device_, &connection_));
      }
      ASSERT_NE(0, device_);

      ASSERT_EQ(ZX_OK, zx::channel::create(0 /*flags*/, &channel_[0], &channel_[1]));
   }

   void TearDown() override
   {
      magma_release_connection(connection_);
      magma_device_release(device_);
   }

   void WriteNotification()
   {
      uint64_t dummy = 0;
      EXPECT_EQ(ZX_OK, channel_[1].write(0 /*flags*/, &dummy, sizeof(dummy), nullptr /*handles*/,
                                         0 /*num_handles*/));
   }

   static void NotificationCallback(void* context)
   {
      uint64_t dummy;
      EXPECT_EQ(ZX_OK,
                reinterpret_cast<TestMagmaWait*>(context)->channel_[0].read(
                    0 /*flags*/, &dummy, nullptr /*handles*/, sizeof(dummy), 0 /*num_handles*/,
                    nullptr /*actual_bytes*/, nullptr /*actual_handles*/));
      reinterpret_cast<TestMagmaWait*>(context)->callback_count_ += 1;
   }

   void Test(bool wait_all)
   {
      constexpr uint32_t kSemaphoreCount = 5;
      constexpr uint64_t kTimeoutNs = ms_to_ns(100);

      std::vector<magma_semaphore_t> semaphores(kSemaphoreCount);

      for (uint32_t i = 0; i < kSemaphoreCount; i++) {
         ASSERT_EQ(MAGMA_STATUS_OK, magma_create_semaphore(connection_, &semaphores[i]));
      }

      auto start = gettime_ns();

      EXPECT_EQ(-1, magma_wait(channel_[0].get(), semaphores.data(), semaphores.size(),
                               start + kTimeoutNs, wait_all, NotificationCallback, this));
      EXPECT_EQ(ETIME, errno);
      EXPECT_LE(kTimeoutNs - ms_to_ns(1), gettime_ns() - start); // TODO(fxb/49103) remove rounding
      EXPECT_EQ(0u, callback_count_);

      WriteNotification();

      start = gettime_ns();

      EXPECT_EQ(-1, magma_wait(channel_[0].get(), semaphores.data(), semaphores.size(),
                               start + kTimeoutNs, wait_all, NotificationCallback, this));
      EXPECT_EQ(ETIME, errno);
      EXPECT_LE(kTimeoutNs - ms_to_ns(1), gettime_ns() - start); // TODO(fxb/49103) remove rounding
      EXPECT_EQ(1u, callback_count_);

      WriteNotification();
      magma_signal_semaphore(semaphores[0]);

      start = gettime_ns();

      EXPECT_EQ(wait_all ? -1 : 0,
                magma_wait(channel_[0].get(), semaphores.data(), semaphores.size(),
                           start + kTimeoutNs, wait_all, NotificationCallback, this));
      EXPECT_EQ(2u, callback_count_);

      magma_reset_semaphore(semaphores[0]);

      start = gettime_ns();

      EXPECT_EQ(-1, magma_wait(channel_[0].get(), semaphores.data(), semaphores.size(),
                               start + kTimeoutNs, wait_all, NotificationCallback, this));
      EXPECT_EQ(ETIME, errno);
      EXPECT_LE(kTimeoutNs - ms_to_ns(1), gettime_ns() - start); // TODO(fxb/49103) remove rounding
      EXPECT_EQ(2u, callback_count_);

      WriteNotification();

      for (uint32_t i = 0; i < kSemaphoreCount; i++) {
         magma_signal_semaphore(semaphores[i]);
      }

      EXPECT_EQ(0, magma_wait(channel_[0].get(), semaphores.data(), semaphores.size(), 0, wait_all,
                              NotificationCallback, this));
      EXPECT_EQ(3u, callback_count_);

      for (uint32_t i = 0; i < kSemaphoreCount; i++) {
         magma_release_semaphore(connection_, semaphores[i]);
      }
   }

protected:
   magma_device_t device_ = 0;
   magma_connection_t connection_ = 0;
   uint64_t callback_count_ = 0;
   zx::channel channel_[2];
};

TEST_F(TestMagmaWait, WaitOne) { Test(false); }

TEST_F(TestMagmaWait, WaitAll) { Test(true); }
