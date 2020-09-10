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

#include <string.h>

#include "buffer.h"
#include "gctx.h"

struct buffer *ngli_buffer_create(struct gctx *gctx)
{
    return gctx->class->buffer_create(gctx);
}

int ngli_buffer_init(struct buffer *s, int size, int usage)
{
    return s->gctx->class->buffer_init(s, size, usage);
}

int ngli_buffer_upload(struct buffer *s, const void *data, int size)
{
    return s->gctx->class->buffer_upload(s, data, size);
}

int ngli_buffer_download(struct buffer *s, void *data, uint32_t size, uint32_t offset)
{
    return s->gctx->class->buffer_download(s, data, size, offset);
}

int ngli_buffer_map(struct buffer *s, int size, uint32_t offset, void **data)
{
    return s->gctx->class->buffer_map(s, size, offset, data);
}

void ngli_buffer_unmap(struct buffer *s)
{
    s->gctx->class->buffer_unmap(s);
}

void ngli_buffer_freep(struct buffer **sp)
{
    if (!*sp)
        return;
    (*sp)->gctx->class->buffer_freep(sp);
}
