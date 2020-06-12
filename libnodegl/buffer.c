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
#include "glcontext.h"
#include "glincludes.h"
#include "memory.h"
#include "nodes.h"

static const GLenum gl_usage_map[NGLI_BUFFER_USAGE_NB] = {
    [NGLI_BUFFER_USAGE_STATIC]  = GL_STATIC_DRAW,
    [NGLI_BUFFER_USAGE_DYNAMIC] = GL_DYNAMIC_DRAW,
};

static GLenum get_gl_usage(int usage)
{
    return gl_usage_map[usage];
}

struct buffer *ngli_buffer_create(struct ngl_ctx *ctx)
{
    struct buffer *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_buffer_init(struct buffer *s, int size, int usage)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;

    s->size = size;
    s->usage = usage;
    ngli_glGenBuffers(gl, 1, &s->id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, size, NULL, get_gl_usage(usage));
    return 0;
}

int ngli_buffer_upload(struct buffer *s, const void *data, int size)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->id);
    ngli_glBufferSubData(gl, GL_ARRAY_BUFFER, 0, size, data);
    return 0;
}

void ngli_buffer_freep(struct buffer **sp)
{
    if (!*sp)
        return;
    struct buffer *s = *sp;
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;
    ngli_glDeleteBuffers(gl, 1, &s->id);
    ngli_freep(sp);
}
