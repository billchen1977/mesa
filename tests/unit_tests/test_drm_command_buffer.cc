#include "drm_command_buffer.h"
#include "magma.h"
#include "magma_util/macros.h"
#include "gtest/gtest.h"
#include <vector>

class Buffer {
public:
   Buffer(magma_connection_t* connection, magma_buffer_t handle, uint64_t size)
       : connection_(connection), handle_(handle), size_(size)
   {
   }
   ~Buffer() { magma_release_buffer(connection_, handle_); }

   uint64_t size() { return size_; }
   uint64_t id() { return magma_get_buffer_id(handle_); }
   magma_buffer_t handle() { return handle_; }

private:
   magma_connection_t* connection_;
   magma_buffer_t handle_;
   uint64_t size_;
};

class TestDrmCommandBuffer {
public:
   TestDrmCommandBuffer() { connection_ = magma_create_connection(0, MAGMA_CAPABILITY_RENDERING); }

   ~TestDrmCommandBuffer() { magma_release_connection(connection_); }

   void NoBuffers()
   {
      drm_i915_gem_execbuffer2 execbuffer2 = {
          .buffers_ptr = reinterpret_cast<uint64_t>(nullptr),
          .buffer_count = 0,
          .batch_start_offset = 0,
          .batch_len = 0, // not used
          .flags = I915_EXEC_HANDLE_LUT,
      };

      std::vector<uint64_t> wait_semaphores;
      std::vector<uint64_t> signal_semaphores;

      uint64_t size = DrmCommandBuffer::RequiredSize(&execbuffer2, 0);
      EXPECT_EQ(sizeof(magma_system_command_buffer), size);

      std::vector<uint8_t> buffer(size);

      EXPECT_TRUE(DrmCommandBuffer::Translate(&execbuffer2, wait_semaphores, signal_semaphores,
                                              buffer.data()));

      auto command_buffer = reinterpret_cast<magma_system_command_buffer*>(buffer.data());
      EXPECT_EQ(-1, (int)command_buffer->batch_buffer_resource_index);
      EXPECT_EQ(0u, command_buffer->batch_start_offset);
      EXPECT_EQ(0u, command_buffer->num_resources);
      EXPECT_EQ(0u, command_buffer->wait_semaphore_count);
      EXPECT_EQ(0u, command_buffer->signal_semaphore_count);
   }

   std::unique_ptr<Buffer> CreateBuffer(uint64_t size)
   {
      magma_buffer_t handle;
      if (magma_create_buffer(connection_, size, &size, &handle) != 0)
         return DRETP(nullptr, "magma_system_alloc failed");
      return std::make_unique<Buffer>(connection_, handle, size);
   }

   void WithBuffers(bool add_relocs, uint32_t wait_semaphore_count, uint32_t signal_semaphore_count)
   {
      std::vector<std::unique_ptr<Buffer>> buffers;

      buffers.push_back(CreateBuffer(PAGE_SIZE));
      buffers.push_back(CreateBuffer(PAGE_SIZE));

      std::vector<uint64_t> wait_semaphore_ids;
      for (uint32_t i = 0; i < wait_semaphore_count; i++) {
         wait_semaphore_ids.push_back(10 + i);
      }
      std::vector<uint64_t> signal_semaphore_ids;
      for (uint32_t i = 0; i < signal_semaphore_count; i++) {
         signal_semaphore_ids.push_back(100 + i);
      }

      std::vector<drm_i915_gem_relocation_entry> exec_relocs_0;
      std::vector<drm_i915_gem_relocation_entry> exec_relocs_1;
      std::vector<drm_i915_gem_exec_object2> exec_res;

      if (add_relocs) {
         exec_relocs_0.push_back({.target_handle = 1, // index
                                  .delta = 0xcafebeef,
                                  .offset = buffers[0]->size() / 2,
                                  .presumed_offset = 0, // not used
                                  .read_domains = 0,
                                  .write_domain = 0});
         exec_relocs_0.push_back({.target_handle = 1, // index
                                  .delta = 0xbeefcafe,
                                  .offset = buffers[0]->size() / 3,
                                  .presumed_offset = 0, // not used
                                  .read_domains = 0,
                                  .write_domain = 0});
         exec_relocs_1.push_back({.target_handle = 0, // index
                                  .delta = 0xcafebeef,
                                  .offset = buffers[1]->size() / 2,
                                  .presumed_offset = 0, // not used
                                  .read_domains = 0,
                                  .write_domain = 0});
      }

      exec_res.push_back({
          .handle = buffers[0]->handle(),
          .relocation_count = static_cast<uint32_t>(exec_relocs_0.size()),
          .relocs_ptr = reinterpret_cast<uint64_t>(exec_relocs_0.data()),
          .alignment = 0,
          .offset = 0,
          .flags = 0,
          .rsvd1 = 10,                      // offset
          .rsvd2 = buffers[0]->size() - 10, // length
      });

      exec_res.push_back({
          .handle = buffers[1]->handle(),
          .relocation_count = static_cast<uint32_t>(exec_relocs_1.size()),
          .relocs_ptr = reinterpret_cast<uint64_t>(exec_relocs_1.data()),
          .alignment = 0,
          .offset = 0,
          .flags = 0,
          .rsvd1 = 20,                      // offset
          .rsvd2 = buffers[1]->size() - 20, // length
      });

      drm_i915_gem_execbuffer2 exec_buffer = {
          .buffers_ptr = reinterpret_cast<uint64_t>(exec_res.data()),
          .buffer_count = static_cast<uint32_t>(exec_res.size()),
          .batch_start_offset = static_cast<uint32_t>(buffers[1]->size()) / 2,
          .batch_len = 0, // not used
          .flags = I915_EXEC_HANDLE_LUT,
      };

      uint64_t size = DrmCommandBuffer::RequiredSize(&exec_buffer, wait_semaphore_ids.size() + signal_semaphore_ids.size());
      uint64_t expected_size =
          sizeof(magma_system_command_buffer) +
          (wait_semaphore_ids.size() + signal_semaphore_ids.size()) * sizeof(uint64_t) +
          sizeof(magma_system_exec_resource) * exec_res.size() +
          sizeof(magma_system_relocation_entry) * (exec_relocs_0.size() + exec_relocs_1.size());
      EXPECT_EQ(expected_size, size);

      std::vector<uint8_t> buffer(size);
      EXPECT_TRUE(DrmCommandBuffer::Translate(&exec_buffer, wait_semaphore_ids,
                                              signal_semaphore_ids, buffer.data()));

      auto command_buffer = reinterpret_cast<magma_system_command_buffer*>(buffer.data());
      EXPECT_EQ(exec_buffer.buffer_count - 1, command_buffer->batch_buffer_resource_index);
      EXPECT_EQ(exec_buffer.batch_start_offset, command_buffer->batch_start_offset);
      EXPECT_EQ(exec_buffer.buffer_count, command_buffer->num_resources);
      EXPECT_EQ(wait_semaphore_ids.size(), command_buffer->wait_semaphore_count);
      EXPECT_EQ(signal_semaphore_ids.size(), command_buffer->signal_semaphore_count);

      auto semaphores = reinterpret_cast<uint64_t*>(command_buffer + 1);
      for (uint32_t i = 0; i < wait_semaphore_count; i++) {
         EXPECT_EQ(wait_semaphore_ids[i], semaphores[i]);
      }
      semaphores += wait_semaphore_count;
      for (uint32_t i = 0; i < signal_semaphore_count; i++) {
         EXPECT_EQ(signal_semaphore_ids[i], semaphores[i]);
      }

      auto exec_resource =
          reinterpret_cast<magma_system_exec_resource*>(semaphores + signal_semaphore_count);
      for (uint32_t i = 0; i < exec_res.size(); i++) {
         EXPECT_EQ(exec_resource->buffer_id, buffers[i]->id());
         EXPECT_EQ(exec_resource->offset, exec_res[i].rsvd1);
         EXPECT_EQ(exec_resource->length, exec_res[i].rsvd2);
         EXPECT_EQ(exec_resource->num_relocations, exec_res[i].relocation_count);
         exec_resource++;
      }

      if (add_relocs) {
         auto reloc = reinterpret_cast<magma_system_relocation_entry*>(exec_resource);
         for (uint32_t i = 0; i < exec_relocs_0.size(); i++) {
            EXPECT_EQ(reloc->offset, exec_relocs_0[i].offset);
            EXPECT_EQ(reloc->target_resource_index, exec_relocs_0[i].target_handle);
            EXPECT_EQ(reloc->target_offset, exec_relocs_0[i].delta);
            reloc++;
         }
         for (uint32_t i = 0; i < exec_relocs_1.size(); i++) {
            EXPECT_EQ(reloc->offset, exec_relocs_1[i].offset);
            EXPECT_EQ(reloc->target_resource_index, exec_relocs_1[i].target_handle);
            EXPECT_EQ(reloc->target_offset, exec_relocs_1[i].delta);
            reloc++;
         }
      }
   }

private:
   magma_connection_t* connection_;
};

TEST(DrmCommandBuffer, NoBuffers)
{
   TestDrmCommandBuffer test;
   test.NoBuffers();
}

TEST(DrmCommandBuffer, SomeBuffers)
{
   TestDrmCommandBuffer test;
   test.WithBuffers(false, 1, 2);
}

TEST(DrmCommandBuffer, BuffersWithRelocs)
{
   TestDrmCommandBuffer test;
   test.WithBuffers(true, 3, 2);
}
