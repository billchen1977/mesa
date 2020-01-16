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
#include <lib/syslog/global.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "mesa"

void __assert_fail(const char* expr, const char* file, int line, const char* func)
{
   FX_LOGF(ERROR, TAG, "Assertion failed: %s (%s: %s: %d)", expr, file, func, line);
   abort();
}

int puts(const char* s) { return fputs(s, stdout); }

int printf(const char* format, ...)
{
   va_list args;
   va_start(args, format);
   vfprintf(stdout, format, args);
   va_end(args);
   return 0;
}

int putc(int c, FILE* stream) { return fprintf(stream, "%c", c); }

int vprintf(const char* format, va_list ap) { return vfprintf(stdout, format, ap); }

int fprintf(FILE* stream, const char* format, ...)
{
   assert(stream == stdout || stream == stderr);
   if (stream == stdout || stream == stderr) {
      va_list args;
      va_start(args, format);
      vfprintf(stream, format, args);
      va_end(args);
   }
   return 0;
}

static inline fx_log_severity_t severity(FILE* stream)
{
   return stream == stdout ? FX_LOG_INFO : FX_LOG_ERROR;
}

int fputs(const char* s, FILE* stream) { return fprintf(stream, "%s", s); }

int vfprintf(FILE* stream, const char* format, va_list ap)
{
   assert(stream == stdout || stream == stderr);
   if (stream == stdout || stream == stderr) {
      static char buffer[512];
      static size_t offset = 0;
      int ret = vsnprintf(buffer + offset, sizeof(buffer) - offset, format, ap);
      if (ret < 0)
         return ret;
      offset += ret;
      if (offset >= sizeof(buffer) || (offset > 0 && buffer[offset - 1] == '\n')) {
         _FX_LOG(severity(stream), TAG, buffer);
         offset = 0;
      }
   }
   return 0;
}
