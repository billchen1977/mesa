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
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <unistd.h>

int _fuchsia_not_implemented_assert_return_int()
{
   assert(false);
   errno = -ENOTSUP;
   return -1;
}

int _fuchsia_not_implemented_return_int()
{
   errno = -ENOTSUP;
   return -1;
}

uintptr_t _fuchsia_not_implemented_assert_return_null()
{
   assert(false);
   errno = -ENOTSUP;
   return (uintptr_t)NULL;
}

uintptr_t _fuchsia_not_implemented_return_null()
{
   errno = -ENOTSUP;
   return (uintptr_t)NULL;
}

int chdir(const char* path) { return _fuchsia_not_implemented_assert_return_int(); }

int chmod(const char* path, mode_t mode) { return _fuchsia_not_implemented_assert_return_int(); }

int close(int fildes) { return _fuchsia_not_implemented_assert_return_int(); }

int closedir(DIR* dirp) { return _fuchsia_not_implemented_assert_return_int(); }

int dup(int fildes) { return _fuchsia_not_implemented_assert_return_int(); }

int dup2(int fildes, int fildes2) { return _fuchsia_not_implemented_assert_return_int(); }

int fclose(FILE* stream) { return _fuchsia_not_implemented_assert_return_int(); }

int fcntl(int fildes, int cmd, ...) { return _fuchsia_not_implemented_assert_return_int(); }

FILE* fdopen(int fildes, const char* mode)
{
   return (FILE*)_fuchsia_not_implemented_assert_return_null();
}

int feof(FILE* stream) { return _fuchsia_not_implemented_assert_return_int(); }

char* fgets(char* restrict s, int n, FILE* restrict stream)
{
   return (char*)_fuchsia_not_implemented_assert_return_null();
}

int fileno(FILE* stream) { return _fuchsia_not_implemented_assert_return_int(); }

FILE* fopen(const char* restrict filename, const char* restrict mode)
{
   return (FILE*)_fuchsia_not_implemented_assert_return_null();
}

size_t fread(void* restrict ptr, size_t size, size_t nitems, FILE* restrict stream)
{
   return _fuchsia_not_implemented_assert_return_null();
}

int fseek(FILE* stream, long offset, int whence)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int fseeko(FILE* stream, off_t offset, int whence)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int fstat(int fildes, struct stat* buf) { return _fuchsia_not_implemented_assert_return_int(); }

int fstatvfs(int fildes, struct statvfs* buf)
{
   return _fuchsia_not_implemented_assert_return_int();
}

long ftell(FILE* stream) { return _fuchsia_not_implemented_assert_return_int(); }

off_t ftello(FILE* stream) { return _fuchsia_not_implemented_assert_return_int(); }

int ftruncate(int fildes, off_t length) { return _fuchsia_not_implemented_assert_return_int(); }

int futimens(int fd, const struct timespec times[2])
{
   return _fuchsia_not_implemented_assert_return_int();
}

char* getcwd(char* buf, size_t size)
{
   return (char*)_fuchsia_not_implemented_assert_return_null();
}

int ioctl(int fildes, int request, ...)
{
   // Called during device query
   return _fuchsia_not_implemented_return_int();
}

int isatty(int fildes) { return _fuchsia_not_implemented_assert_return_null(); }

int link(const char* path1, const char* path2)
{
   return _fuchsia_not_implemented_assert_return_int();
}

off_t lseek(int fildes, off_t offset, int whence)
{
   return (off_t)_fuchsia_not_implemented_assert_return_int();
}

int lstat(const char* restrict path, struct stat* restrict buf)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int mkdir(const char* path, mode_t mode) { return _fuchsia_not_implemented_assert_return_int(); }

FILE* open_memstream(char** bufp, size_t* sizep)
{
   return (FILE*)_fuchsia_not_implemented_assert_return_null();
}

int open(const char* path, int oflag, ...) { return _fuchsia_not_implemented_assert_return_int(); }

DIR* opendir(const char* dirname) { return (DIR*)_fuchsia_not_implemented_assert_return_null(); }

int pipe(int fildes[2]) { return _fuchsia_not_implemented_assert_return_int(); }

int poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int posix_fallocate(int fd, off_t offset, off_t len)
{
   return _fuchsia_not_implemented_assert_return_int();
}

ssize_t pread(int fildes, void* buf, size_t nbyte, off_t offset)
{
   return (ssize_t)_fuchsia_not_implemented_assert_return_null();
}

ssize_t read(int fildes, void* buf, size_t nbyte)
{
   return (ssize_t)_fuchsia_not_implemented_assert_return_null();
}

struct dirent* readdir(DIR* dirp)
{
   return (struct dirent*)_fuchsia_not_implemented_assert_return_null();
}

ssize_t readlink(const char* restrict path, char* restrict buf, size_t bufsize)
{
   return (ssize_t)_fuchsia_not_implemented_assert_return_null();
}

int remove(const char* path) { return _fuchsia_not_implemented_assert_return_int(); }

int rename(const char* old, const char* new)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int stat(const char* restrict path, struct stat* restrict buf)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int statvfs(const char* restrict path, struct statvfs* restrict buf)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int symlink(const char* path1, const char* path2)
{
   return _fuchsia_not_implemented_assert_return_int();
}

int uname(struct utsname* name) { return _fuchsia_not_implemented_assert_return_int(); }

int unlink(const char* path) { return _fuchsia_not_implemented_assert_return_int(); }

ssize_t write(int fildes, const void* buf, size_t nbyte)
{
   return (ssize_t)_fuchsia_not_implemented_assert_return_null();
}

ssize_t writev(int fildes, const struct iovec* iov, int iovcnt)
{
   return (ssize_t)_fuchsia_not_implemented_assert_return_null();
}
