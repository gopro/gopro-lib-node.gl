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

#ifndef BSTR_H
#define BSTR_H

#include <stdarg.h>

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bstr;

struct bstr *ngli_bstr_create(void);
void ngli_bstr_print(struct bstr *b, const char *str);
void ngli_bstr_printf(struct bstr *b, const char *fmt, ...) ngli_printf_format(2, 3);
void ngli_bstr_clear(struct bstr *b);
int ngli_bstr_truncate(struct bstr *b, int len);
char *ngli_bstr_strdup(const struct bstr *b);
const char *ngli_bstr_strptr(const struct bstr *b);
int ngli_bstr_len(const struct bstr *b);
int ngli_bstr_check(const struct bstr *b);
void ngli_bstr_freep(struct bstr **bp);

#ifdef __cplusplus
}
#endif

#endif
