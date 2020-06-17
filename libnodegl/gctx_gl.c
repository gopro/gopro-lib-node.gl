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
#include "gctx_gl.h"
#include "log.h"
#include "memory.h"
#include "rendertarget_gl.h"

void ngli_gctx_gl_set_rendertarget(struct gctx *s, struct rendertarget *rt)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    if (rt == s_priv->rendertarget)
        return;

    struct rendertarget_gl *rt_gl = (struct rendertarget_gl *)rt;
    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    s_priv->rendertarget = rt;
}

struct rendertarget *ngli_gctx_gl_get_rendertarget(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    return s_priv->rendertarget;
}

void ngli_gctx_gl_set_viewport(struct gctx *s, const int *viewport)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
    memcpy(&s_priv->viewport, viewport, sizeof(s_priv->viewport));
}

void ngli_gctx_gl_get_viewport(struct gctx *s, int *viewport)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(viewport, &s_priv->viewport, sizeof(s_priv->viewport));
}

void ngli_gctx_gl_set_scissor(struct gctx *s, const int *scissor)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    ngli_glScissor(gl, scissor[0], scissor[1], scissor[2], scissor[3]);
    memcpy(&s_priv->scissor, scissor, sizeof(s_priv->scissor));
}

void ngli_gctx_gl_get_scissor(struct gctx *s, int *scissor)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(scissor, &s_priv->scissor, sizeof(s_priv->scissor));
}

void ngli_gctx_gl_set_clear_color(struct gctx *s, const float *color)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    memcpy(s_priv->clear_color, color, sizeof(s_priv->clear_color));
    ngli_glClearColor(gl, color[0], color[1], color[2], color[3]);
}

void ngli_gctx_gl_get_clear_color(struct gctx *s, float *color)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(color, &s_priv->clear_color, sizeof(s_priv->clear_color));
}

void ngli_gctx_gl_clear_color(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct glstate *glstate = &s_priv->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

void ngli_gctx_gl_clear_depth_stencil(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct glstate *glstate = &s_priv->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

void ngli_gctx_gl_invalidate_depth_stencil(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    if (!(gl->features & NGLI_FEATURE_INVALIDATE_SUBDATA))
        return;

    static const GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    ngli_glInvalidateFramebuffer(gl, GL_FRAMEBUFFER, NGLI_ARRAY_NB(attachments), attachments);
}
