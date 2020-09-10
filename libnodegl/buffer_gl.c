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
#include "gctx_gl.h"
#include "glcontext.h"
#include "glincludes.h"
#include "memory.h"
#include "nodes.h"
#include "log.h"

static const GLenum gl_usage_map[NGLI_BUFFER_USAGE_NB] = {
    [NGLI_BUFFER_USAGE_STATIC]  = GL_STATIC_DRAW,
    [NGLI_BUFFER_USAGE_DYNAMIC] = GL_DYNAMIC_DRAW,
};

static GLenum get_gl_usage(int usage)
{
    return gl_usage_map[usage];
}

struct buffer *ngli_buffer_gl_create(struct gctx *gctx)
{
    struct buffer_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct buffer *)s;
}

int ngli_buffer_gl_init(struct buffer *s, int size, int usage)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct buffer_gl *s_priv = (struct buffer_gl *)s;

    s->size = size;
    s->usage = usage;
    ngli_glGenBuffers(gl, 1, &s_priv->id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s_priv->id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, size, NULL, get_gl_usage(usage));
    return 0;
}

int ngli_buffer_gl_upload(struct buffer *s, const void *data, int size)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct buffer_gl *s_priv = (struct buffer_gl *)s;
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s_priv->id);
    ngli_glBufferSubData(gl, GL_ARRAY_BUFFER, 0, size, data);
    return 0;
}

int ngli_buffer_gl_download(struct buffer *s, void *data, uint32_t size, uint32_t offset)
{
    void *src;
    int ret = ngli_buffer_gl_map(s, size, offset, &src);
    if (ret < 0)
        return ret;
    memcpy(data, src, size);
    ngli_buffer_gl_unmap(s);
    return 0;
}

int ngli_buffer_gl_map(struct buffer *s, int size, uint32_t offset, void **data)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct buffer_gl *s_priv = (struct buffer_gl *)s;

    if (!(gl->features & NGLI_FEATURE_MAP_BUFFER)) {
        LOG(WARNING, "context does not support glMapBufferRange");
        return NGL_ERROR_UNSUPPORTED;
    }

    ngli_assert(data);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s_priv->id);
    // TODO: use persistent mapped buffers, asynchronous
    *data = ngli_glMapBufferRange(gl, GL_ARRAY_BUFFER, offset, size, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT); //TODO: pass access flags

    return 0;
}

void ngli_buffer_gl_unmap(struct buffer *s)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct buffer_gl *s_priv = (struct buffer_gl *)s;

    if (!(gl->features & NGLI_FEATURE_MAP_BUFFER)) {
        LOG(WARNING, "context does not support glUnmapBuffer");
        return;
    }
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s_priv->id);
    ngli_glUnmapBuffer(gl, GL_ARRAY_BUFFER);
}

void ngli_buffer_gl_freep(struct buffer **sp)
{
    if (!*sp)
        return;
    struct buffer *s = *sp;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct buffer_gl *s_priv = (struct buffer_gl *)s;
    ngli_glDeleteBuffers(gl, 1, &s_priv->id);
    ngli_freep(sp);
}
