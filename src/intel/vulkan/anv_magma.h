// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANV_MAGMA_H
#define ANV_MAGMA_H

#include "magma.h"
#include "magma_util/dlog.h"
#include "magma_util/inflight_list.h"
#include "magma_util/macros.h"
// clang-format off
#include "anv_private.h"
// clang-format on

class Connection : public anv_connection {
public:
   Connection(magma_connection_t* magma_connection)
       : magma_connection_(magma_connection), inflight_list_(magma_connection)
   {
   }

   ~Connection() { magma_release_connection(magma_connection_); }

   magma_connection_t* magma_connection() { return magma_connection_; }

   magma::InflightList* inflight_list() { return &inflight_list_; }

private:
   magma_connection_t* magma_connection_;
   magma::InflightList inflight_list_;
};

static magma_connection_t* magma_connection(anv_device* device)
{
   DASSERT(device);
   DASSERT(device->connection);
   return static_cast<Connection*>(device->connection)->magma_connection();
}

#endif // ANV_MAGMA_H
