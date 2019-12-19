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

#include <assert.h>
#include <lib/zx/channel.h>

#include "util/inflight_list.h"
#include "gtest/gtest.h"

struct TestConnection : public magma_connection {
   TestConnection() { zx::channel::create(0, &channel[0], &channel[1]); }

   zx::channel channel[2];
};

static magma_status_t wait_notification_channel(magma_connection_t connection, int64_t timeout_ns)
{
   zx_signals_t pending;
   zx_status_t status =
       static_cast<TestConnection*>(connection)
           ->channel[0]
           .wait_one(ZX_CHANNEL_READABLE, zx::deadline_after(zx::nsec(timeout_ns)), &pending);
   if (status != ZX_OK)
      return MAGMA_STATUS_INTERNAL_ERROR;
   assert(pending & ZX_CHANNEL_READABLE);
   return MAGMA_STATUS_OK;
}

static magma_status_t read_notification_channel(magma_connection_t connection, void* buffer,
                                                uint64_t buffer_size, uint64_t* buffer_size_out)
{
   uint32_t buffer_actual_size;
   zx_status_t status = static_cast<TestConnection*>(connection)
                            ->channel[0]
                            .read(0, buffer, nullptr, buffer_size, 0, &buffer_actual_size, nullptr);
   if (status == ZX_OK) {
      *buffer_size_out = buffer_actual_size;
      return MAGMA_STATUS_OK;
   }
   return MAGMA_STATUS_INTERNAL_ERROR;
}

const uint64_t kBufferIdBase = 0xaabbccdd00000000;

class TestInflightList : public ::testing::Test {
public:
   void SetUp() override { list_ = InflightList_Create(); }

   void TearDown() override { InflightList_Destroy(list_); }

protected:
   InflightList* list_;
};

TEST_F(TestInflightList, RemoveWhenEmpty)
{
   EXPECT_FALSE(InflightList_remove(list_, kBufferIdBase));
   EXPECT_EQ(0u, InflightList_size(list_));
}

TEST_F(TestInflightList, RemoveFromTail)
{
   InflightList_add(list_, kBufferIdBase + 1);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 1));
   InflightList_add(list_, kBufferIdBase + 2);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 2));
   EXPECT_EQ(2u, InflightList_size(list_));

   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase + 1));
   EXPECT_FALSE(InflightList_is_inflight(list_, kBufferIdBase + 1));
   EXPECT_EQ(1u, InflightList_size(list_));
   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase + 2));
   EXPECT_FALSE(InflightList_is_inflight(list_, kBufferIdBase + 2));
   EXPECT_EQ(0u, InflightList_size(list_));
   EXPECT_EQ(0u, u_vector_length(&list_->buffers_));
}

TEST_F(TestInflightList, RemoveFromHead)
{
   InflightList_add(list_, kBufferIdBase + 1);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 1));
   InflightList_add(list_, kBufferIdBase + 2);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 2));
   EXPECT_EQ(2u, InflightList_size(list_));

   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase + 2));
   EXPECT_FALSE(InflightList_is_inflight(list_, kBufferIdBase + 2));
   EXPECT_EQ(1u, InflightList_size(list_));
   EXPECT_EQ(2u, u_vector_length(&list_->buffers_)); // Entry was only marked for deletion
   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase + 1));
   EXPECT_FALSE(InflightList_is_inflight(list_, kBufferIdBase + 1));
   EXPECT_EQ(0u, InflightList_size(list_));
   EXPECT_EQ(0u, u_vector_length(&list_->buffers_));
}

TEST_F(TestInflightList, RemoveMiddle)
{
   InflightList_add(list_, kBufferIdBase + 1);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 1));
   InflightList_add(list_, kBufferIdBase + 2);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 2));
   InflightList_add(list_, kBufferIdBase + 3);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase + 3));
   EXPECT_EQ(3u, InflightList_size(list_));

   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase + 2));
   EXPECT_FALSE(InflightList_is_inflight(list_, kBufferIdBase + 2));
   EXPECT_EQ(2u, InflightList_size(list_));
   EXPECT_EQ(3u, u_vector_length(&list_->buffers_)); // Entry was only marked for deletion
}

TEST_F(TestInflightList, RemoveDouble)
{
   InflightList_add(list_, kBufferIdBase);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase));
   InflightList_add(list_, kBufferIdBase);
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase));
   EXPECT_EQ(2u, InflightList_size(list_));

   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase));
   EXPECT_TRUE(InflightList_is_inflight(list_, kBufferIdBase));
   EXPECT_EQ(1u, InflightList_size(list_));

   EXPECT_TRUE(InflightList_remove(list_, kBufferIdBase));
   EXPECT_FALSE(InflightList_is_inflight(list_, kBufferIdBase));
   EXPECT_EQ(0u, InflightList_size(list_));
}

TEST_F(TestInflightList, WaitAndService)
{
   InflightList_inject_for_test(list_, wait_notification_channel, read_notification_channel);

   uint64_t buffer_id = 0xabab1234;
   EXPECT_FALSE(InflightList_is_inflight(list_, buffer_id));
   InflightList_add(list_, buffer_id);
   EXPECT_TRUE(InflightList_is_inflight(list_, buffer_id));

   TestConnection connection;
   EXPECT_NE(MAGMA_STATUS_OK, InflightList_WaitForCompletion(list_, &connection, 100));
   connection.channel[1].write(0, &buffer_id, sizeof(buffer_id), nullptr, 0);
   EXPECT_EQ(MAGMA_STATUS_OK, InflightList_WaitForCompletion(list_, &connection, 100));

   InflightList_ServiceCompletions(list_, &connection);
   EXPECT_FALSE(InflightList_is_inflight(list_, buffer_id));
}
