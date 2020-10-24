/*
 * Copyright 2016 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef LOG_H
#define LOG_H
#ifdef __cplusplus
extern "C" {
#endif
#include "nodegl.h"
#include "utils.h"

#if defined(WIN32)
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#define LOG(log_level, ...) ngli_log_print(NGL_LOG_##log_level, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#ifdef LOGTRACE
# define TRACE(...) LOG(VERBOSE, __VA_ARGS__)
#else
# define TRACE(...) do { if (0) LOG(VERBOSE, __VA_ARGS__); } while (0)
#endif

void ngli_log_print(int log_level, const char *filename,
                    int ln, const char *fn, const char *fmt, ...) ngli_printf_format(5, 6);

#define NGLI_RET_STR(ret) ngli_log_ret_str((char[128]){0}, 128, ret)

char *ngli_log_ret_str(char *buf, size_t buf_size, int ret);

#define TODO(fmt, ...) LOG(WARNING, "TODO " fmt, ##__VA_ARGS__)
#ifdef __cplusplus
}
#endif
#endif /* LOG_H */
