/*
 * Copyright 2018 GoPro Inc.
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

#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

void *ngli_malloc(size_t size);
void *ngli_calloc(size_t n, size_t size);
void *ngli_malloc_aligned(size_t size);

void *ngli_realloc(void *ptr, size_t size);

void ngli_free(void *ptr);
void ngli_freep(void *ptr);
void ngli_free_aligned(void *ptr);
void ngli_freep_aligned(void *ptr);

#endif
