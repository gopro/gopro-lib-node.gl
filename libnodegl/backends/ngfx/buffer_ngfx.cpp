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

#include "buffer_ngfx.h"
extern "C" {
#include "memory.h"
}
#include "gpu_ctx_ngfx.h"
#include "ngfx/graphics/Buffer.h"
#include "debugutil_ngfx.h"

using namespace ngfx;

struct buffer *ngli_buffer_ngfx_create(struct gpu_ctx *gpu_ctx)
{
    buffer_ngfx *s = (buffer_ngfx*)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct buffer *)s;
}

int ngli_buffer_ngfx_init(struct buffer *s, int size, int usage)
{
#ifdef NGFX_GRAPHICS_BACKEND_METAL //TODO: pass correct size from higher level
    size = (size + 15) / 16 * 16; //align to multiple of 16
#endif
    struct gpu_ctx_ngfx *ctx = (struct gpu_ctx_ngfx *)s->gpu_ctx;
    struct buffer_ngfx *s_priv = (struct buffer_ngfx *)s;
    s->size = size;
    s->usage = usage;
    //TODO: pass usage flags (e.g. vertex buffer, index buffer, uniform buffer, etc
    s_priv->v = Buffer::create(ctx->graphics_context, NULL, size,
                   BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT);
    return 0;
}

int ngli_buffer_ngfx_upload(struct buffer *s, const void *data, int size, int offset)
{
    struct buffer_ngfx *s_priv = (struct buffer_ngfx *)s;
    s_priv->v->upload(data, size);
    return 0;
}

void ngli_buffer_ngfx_freep(struct buffer **sp) {
    if (!sp) return;
    (*sp)->gpu_ctx->cls->wait_idle((*sp)->gpu_ctx);
    buffer *s = *sp;
    buffer_ngfx *s_priv = (buffer_ngfx *)s;
    delete s_priv->v;
    ngli_freep(sp);
}
