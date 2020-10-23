/*
 * Copyright 2019 GoPro Inc.
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
#include "memory.h"
#include "buffer_ngfx.h"
#include "gctx_ngfx.h"
#include "graphics/Buffer.h"
#include "log.h"
using namespace ngfx;

struct buffer *ngli_buffer_ngfx_create(struct gctx *gctx)
{
    buffer_ngfx *s = (buffer_ngfx*)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct buffer *)s;
}

int ngli_buffer_ngfx_init(struct buffer *s, int size, int usage)
{
#ifdef GRAPHICS_BACKEND_METAL //TODO: pass correct size from higher level
    size = (size + 15) / 16 * 16; //align to multiple of 16
#endif
    struct gctx_ngfx *ctx = (struct gctx_ngfx *)s->gctx;
    struct buffer_ngfx *s_priv = (struct buffer_ngfx *)s;
    s->size = size;
    s->usage = usage;
    //TODO: pass usage flags (e.g. vertex buffer, index buffer, uniform buffer, etc
    s_priv->v = Buffer::create(ctx->graphics_context, NULL, size,
                   BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT);
    return 0;
}

int ngli_buffer_ngfx_upload(struct buffer *s, const void *data, uint32_t size, uint32_t offset)
{
    struct buffer_ngfx *s_priv = (struct buffer_ngfx *)s;
    s_priv->v->upload(data, size);
    return 0;
}

int ngli_buffer_ngfx_download(struct buffer* s, void* data, uint32_t size, uint32_t offset)
{
    TODO("Buffer download");
    return 0;
}

int ngli_buffer_ngfx_map(struct buffer *s, int size, uint32_t offset, void** data)
{
    TODO("Buffer::map");
    return 0;
}

void ngli_buffer_ngfx_unmap(struct buffer* s) {
    TODO("Buffer::unmap");
}

void ngli_buffer_ngfx_freep(struct buffer **sp) {
    if (!sp) return;
    buffer *s = *sp;
    buffer_ngfx *s_priv = (buffer_ngfx *)s;
    delete s_priv->v;
    ngli_freep(sp);
}
