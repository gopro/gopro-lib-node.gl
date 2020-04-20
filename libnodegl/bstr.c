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

#include <stdio.h>
#include <stdlib.h>

#include "bstr.h"
#include "memory.h"

#define BUFFER_PADDING 1024

struct bstr {
    char *str;
    int len;
    int bufsize;
};

struct bstr *ngli_bstr_create(void)
{
    struct bstr *b = ngli_calloc(1, sizeof(*b));

    if (!b)
        return NULL;
    b->bufsize = BUFFER_PADDING;
    b->str = ngli_malloc(b->bufsize);
    if (!b->str) {
        ngli_free(b);
        return NULL;
    }
    b->str[0] = 0;
    return b;
}

int ngli_bstr_print(struct bstr *b, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    const int avail = b->bufsize - b->len;
    int len = vsnprintf(b->str + b->len, avail, fmt, va);
    va_end(va);
    if (len < 0) {
        b->str[b->len] = 0;
        return len;
    }

    if (len + 1 > avail) {
        const int new_size = b->len + len + 1 + BUFFER_PADDING;
        void *ptr = ngli_realloc(b->str, new_size);
        if (!ptr)
            return 0;
        b->str = ptr;
        b->bufsize = new_size;
        va_start(va, fmt);
        len = vsnprintf(b->str + b->len, len + 1, fmt, va);
        va_end(va);
        if (len < 0) {
            b->str[b->len] = 0;
            return len;
        }
    }

    b->len = b->len + len;
    return 0;
}

void ngli_bstr_clear(struct bstr *b)
{
    b->len = 0;
    b->str[0] = 0;
}

char *ngli_bstr_strdup(struct bstr *b)
{
    return ngli_strdup(b->str);
}

char *ngli_bstr_strptr(struct bstr *b)
{
    return b->str;
}

int ngli_bstr_len(struct bstr *b)
{
    return b->len;
}

void ngli_bstr_freep(struct bstr **bp)
{
    struct bstr *b = *bp;
    if (!b)
        return;
    ngli_free(b->str);
    ngli_free(b);
    *bp = NULL;
}
