// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _OS_DIRENT_H_
#define _OS_DIRENT_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct os_dirent {
   ino_t d_ino;
   char d_name[];
};

typedef struct os_dir os_dir_t;

os_dir_t* os_opendir(const char* path);

struct os_dirent* os_readdir(os_dir_t* dir);

int os_closedir(os_dir_t* dir);

#ifdef __cplusplus
}
#endif

#endif /* _OS_DIRENT_H_ */
