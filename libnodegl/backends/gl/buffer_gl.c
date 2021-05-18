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

#include "buffer_gl.h"
#include "gpu_ctx_gl.h"
#include "glcontext.h"
#include "glincludes.h"
#include "memory.h"
#include "nodes.h"

static GLenum get_gl_usage(int usage)
{
    if (usage & NGLI_BUFFER_USAGE_DYNAMIC_BIT)
        return GL_DYNAMIC_DRAW;
    return GL_STATIC_DRAW;
}

struct buffer *ngli_buffer_gl_create(struct gpu_ctx *gpu_ctx)
{
    struct buffer_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct buffer *)s;
}

int ngli_buffer_gl_init(struct buffer *s, int size, int usage)
{
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct buffer_gl *s_priv = (struct buffer_gl *)s;

    s->size = size;
    s->usage = usage;
    ngli_glGenBuffers(gl, 1, &s_priv->id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s_priv->id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, size, NULL, get_gl_usage(usage));
    return 0;
}

int ngli_buffer_gl_upload(struct buffer *s, const void *data, int size, int offset)
{
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    const struct buffer_gl *s_priv = (struct buffer_gl *)s;
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s_priv->id);
    ngli_glBufferSubData(gl, GL_ARRAY_BUFFER, offset, size, data);
    return 0;
}

void ngli_buffer_gl_freep(struct buffer **sp)
{
    if (!*sp)
        return;
    struct buffer *s = *sp;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)s->gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct buffer_gl *s_priv = (struct buffer_gl *)s;
    ngli_glDeleteBuffers(gl, 1, &s_priv->id);
    ngli_freep(sp);
}
