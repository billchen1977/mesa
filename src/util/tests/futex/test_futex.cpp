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

#include <atomic>
#include <chrono>
#include <thread>

#include "util/futex.h"
#include "gtest/gtest.h"

TEST(Futex, Retry)
{
   uint32_t value = 0;
   EXPECT_EQ(-EAGAIN, futex_wait(&value, value + 1, NULL));
}

TEST(Futex, Timeout)
{
   uint32_t value = 0;

   auto start = std::chrono::high_resolution_clock::now();

   static constexpr uint32_t kTimeoutMs = 100;
   struct timespec timeout = {
       .tv_sec = 0,
       .tv_nsec = kTimeoutMs * 1000000,
   };
   EXPECT_EQ(-ETIMEDOUT, futex_wait(&value, value, &timeout));

   auto end = std::chrono::high_resolution_clock::now();
   std::chrono::duration<double, std::milli> elapsed = end - start;

   EXPECT_GT(elapsed.count(), kTimeoutMs);
}

TEST(Futex, WaitAndWake)
{
   constexpr int kResultNotReady = 1;
   static std::atomic<int> result = kResultNotReady;
   static uint32_t value;

   std::thread thread([]() {
      int r = futex_wait(&value, value, NULL);
      result.store(r);
   });

   int check = kResultNotReady;

   for (int retry = 0; retry < 10; retry++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      futex_wake(&value, 1);
      check = result.load();
      if (check != kResultNotReady) {
         thread.join();
         break;
      }
   }

   EXPECT_EQ(check, 0);
}
