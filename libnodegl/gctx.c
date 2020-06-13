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

#include <string.h>

#include "gctx.h"
#include "nodes.h"

void ngli_gctx_set_rendertarget(struct ngl_ctx *s, struct rendertarget *rt)
{
    struct glcontext *gl = s->glcontext;

    if (rt == s->rendertarget)
        return;

    const GLuint fbo_id = rt ? rt->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    s->rendertarget = rt;
}

struct rendertarget *ngli_gctx_get_rendertarget(struct ngl_ctx *s)
{
    return s->rendertarget;
}

void ngli_gctx_set_viewport(struct ngl_ctx *s, const int *viewport)
{
    struct glcontext *gl = s->glcontext;
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
    memcpy(&s->viewport, viewport, sizeof(s->viewport));
}

void ngli_gctx_get_viewport(struct ngl_ctx *s, int *viewport)
{
    memcpy(viewport, &s->viewport, sizeof(s->viewport));
}

void ngli_gctx_set_scissor(struct ngl_ctx *s, const int *scissor)
{
    struct glcontext *gl = s->glcontext;
    ngli_glScissor(gl, scissor[0], scissor[1], scissor[2], scissor[3]);
    memcpy(&s->scissor, scissor, sizeof(s->scissor));
}

void ngli_gctx_get_scissor(struct ngl_ctx *s, int *scissor)
{
    memcpy(scissor, &s->scissor, sizeof(s->scissor));
}

void ngli_gctx_set_clear_color(struct ngl_ctx *s, const float *color)
{
    struct glcontext *gl = s->glcontext;
    memcpy(s->clear_color, color, sizeof(s->clear_color));
    ngli_glClearColor(gl, color[0], color[1], color[2], color[3]);
}

void ngli_gctx_get_clear_color(struct ngl_ctx *s, float *color)
{
    memcpy(color, &s->clear_color, sizeof(s->clear_color));
}

void ngli_gctx_clear_color(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct glstate *glstate = &s->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

void ngli_gctx_clear_depth_stencil(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct glstate *glstate = &s->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

void ngli_gctx_invalidate_depth_stencil(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;

    if (!(gl->features & NGLI_FEATURE_INVALIDATE_SUBDATA))
        return;

    static const GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    ngli_glInvalidateFramebuffer(gl, GL_FRAMEBUFFER, NGLI_ARRAY_NB(attachments), attachments);
}
