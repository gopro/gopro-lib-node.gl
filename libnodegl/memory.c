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

#include "config.h"

#ifndef TARGET_MINGW_W64
#define _POSIX_C_SOURCE 200809L // posix_memalign()
#endif

#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "utils.h"

#if DEBUG_MEM
static int failure_requested(void)
{
    static int alloc_counter;

    const char *mem_politic = getenv("NGL_MEM_ALLOC_FAIL");
    if (!mem_politic)
        return 0;
    if (!strcmp(mem_politic, "count")) {
        alloc_counter++;
        fprintf(stderr, "MEMCOUNTER: %d\n", alloc_counter);
        return 0;
    }
    char *endptr = NULL;
    long int n = strtol(mem_politic, &endptr, 0);
    int should_fail = 0;
    const int r = rand();
    if (endptr && *endptr == '%') {
        should_fail = r % 100 < n;
    } else if (alloc_counter == n) {
        should_fail = 1;
    }
    if (should_fail)
        fprintf(stderr, "WARNING: next alloc (%d) will fail\n", alloc_counter);
    alloc_counter++;
    return should_fail;
}
#else
static inline int failure_requested(void)
{
    return 0;
}
#endif

void *ngli_malloc(size_t size)
{
    if (failure_requested())
        return NULL;
    return malloc(size);
}

void *ngli_calloc(size_t n, size_t size)
{
    if (failure_requested())
        return NULL;
    return calloc(n, size);
}

void *ngli_malloc_aligned(size_t size)
{
    if (failure_requested())
        return NULL;

    void *ptr;
#ifdef TARGET_WINDOWS
    ptr = _aligned_malloc(size, NGLI_ALIGN_VAL);
#else
    if (posix_memalign(&ptr, NGLI_ALIGN_VAL, size))
        ptr = NULL;
#endif
    return ptr;
}

void *ngli_realloc(void *ptr, size_t size)
{
    if (failure_requested())
        return NULL;
    return realloc(ptr, size);
}

void ngli_free(void *ptr)
{
    free(ptr);
}

void ngli_freep(void *ptr)
{
    ngli_free(*(void **)ptr);
    memset(ptr, 0, sizeof(void *));
}

void ngli_free_aligned(void *ptr)
{
#ifdef TARGET_WINDOWS
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void ngli_freep_aligned(void *ptr)
{
    ngli_free_aligned(*(void **)ptr);
    memset(ptr, 0, sizeof(void *));
}
