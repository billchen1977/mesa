// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANV_MAGMA_H
#define ANV_MAGMA_H

#include "magma.h"
#include "magma_util/dlog.h"
#include "magma_util/macros.h"
// clang-format off
#include "anv_private.h"
// clang-format on
#include <map>
#include <mutex>

class Connection : public anv_connection {
public:
   Connection(magma_connection_t* magma_connection) : magma_connection_(magma_connection) {}

   ~Connection() { magma_release_connection(magma_connection_); }

   magma_connection_t* magma_connection() { return magma_connection_; }

   bool find_semaphore(uint32_t id, magma_semaphore_t* semaphore_out)
   {
      std::unique_lock<std::mutex> lock(semaphore_map_lock_);
      auto iter = semaphore_map_.find(id);
      if (iter == semaphore_map_.end())
         return DRETF(false, "id not in semaphore map: %u", id);
      *semaphore_out = iter->second;
      return true;
   }

   uint32_t add_semaphore(magma_semaphore_t semaphore)
   {
      std::unique_lock<std::mutex> lock(semaphore_map_lock_);
      uint32_t id;
      do {
         id = next_semaphore_id_++;
      } while (semaphore_map_.find(id) != semaphore_map_.end());
      semaphore_map_[id] = semaphore;
      return id;
   }

   bool remove_semaphore(uint32_t id, magma_semaphore_t* semaphore_out)
   {
      std::unique_lock<std::mutex> lock(semaphore_map_lock_);
      auto iter = semaphore_map_.find(id);
      if (iter == semaphore_map_.end())
         return DRETF(false, "id not in semaphore map: %u", id);
      *semaphore_out = iter->second;
      semaphore_map_.erase(iter);
      return true;
   }

   bool find_semaphores(anv_semaphore* semaphore[], magma_semaphore_t magma_semaphore_out[],
                        uint32_t count)
   {
      std::unique_lock<std::mutex> lock(semaphore_map_lock_);

      for (uint32_t i = 0; i < count; i++) {
         anv_semaphore_impl* impl = semaphore[i]->temporary.type != ANV_SEMAPHORE_TYPE_NONE
                                        ? &semaphore[i]->temporary
                                        : &semaphore[i]->permanent;

         auto iter = semaphore_map_.find(impl->syncobj);
         if (iter == semaphore_map_.end())
            return DRETF(false, "id not in semaphore map: %u", impl->syncobj);

         magma_semaphore_out[i] = iter->second;
      }
      return true;
   }

private:
   magma_connection_t* magma_connection_;
   std::mutex semaphore_map_lock_;
   std::map<uint32_t, magma_semaphore_t> semaphore_map_;
   uint32_t next_semaphore_id_ = 1000;
};

static magma_connection_t* magma_connection(anv_device* device)
{
   DASSERT(device);
   DASSERT(device->connection);
   return static_cast<Connection*>(device->connection)->magma_connection();
}

#endif // ANV_MAGMA_H
