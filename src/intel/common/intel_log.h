/*
 * Copyright 2017 Google
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

#ifndef INTEL_LOG_H
#define INTEL_LOG_H

#include <stdarg.h>

#include "util/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INTEL_LOG_TAG
#define INTEL_LOG_TAG "INTEL-MESA"
#endif

enum intel_log_level {
   INTEL_LOG_ERROR,
   INTEL_LOG_WARN,
   INTEL_LOG_INFO,
   INTEL_LOG_DEBUG,
};

void PRINTFLIKE(5, 6) intel_log(enum intel_log_level, const char* tag, const char* file, int line,
                                const char* format, ...);

void intel_log_v(enum intel_log_level, const char* tag, const char* file, int line,
                 const char* format, va_list va);

#define intel_loge(fmt, ...)                                                                       \
   intel_log(INTEL_LOG_ERROR, (INTEL_LOG_TAG), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define intel_logw(fmt, ...)                                                                       \
   intel_log(INTEL_LOG_WARN, (INTEL_LOG_TAG), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define intel_logi(fmt, ...)                                                                       \
   intel_log(INTEL_LOG_INFO, (INTEL_LOG_TAG), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#ifdef DEBUG
#define intel_logd(fmt, ...)                                                                       \
   intel_log(INTEL_LOG_DEBUG, (INTEL_LOG_TAG), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#else
#define intel_logd(fmt, ...) __intel_log_use_args(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#endif

#define intel_loge_v(file, line, fmt, va)                                                          \
   intel_log_v(INTEL_LOG_ERROR, (INTEL_LOG_TAG), (file), (line), (fmt), (va))
#define intel_logw_v(file, line, fmt, va)                                                          \
   intel_log_v(INTEL_LOG_WARN, (INTEL_LOG_TAG), (file), (line), (fmt), (va))
#define intel_logi_v(file, line, fmt, va)                                                          \
   intel_log_v(INTEL_LOG_INFO, (INTEL_LOG_TAG), (file), (line), (fmt), (va))
#ifdef DEBUG
#define intel_logd_v(file, line, fmt, va)                                                          \
   intel_log_v(INTEL_LOG_DEBUG, (INTEL_LOG_TAG), (file), (line), (fmt), (va))
#else
#define intel_logd_v(file, line, fmt, va) __intel_log_use_args((file), (line), (fmt), (va))
#endif

#ifndef DEBUG
/* Suppres -Wunused */
static inline void PRINTFLIKE(3, 4)
    __intel_log_use_args(const char* file, int line, const char* format, ...)
{
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* INTEL_LOG_H */
