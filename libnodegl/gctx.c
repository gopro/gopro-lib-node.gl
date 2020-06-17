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
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "rendertarget_gl.h"

extern const struct gctx_class ngli_gctx_gl;
extern const struct gctx_class ngli_gctx_gles;

static const struct gctx_class *backend_map[] = {
    [NGL_BACKEND_OPENGL]   = &ngli_gctx_gl,
    [NGL_BACKEND_OPENGLES] = &ngli_gctx_gles,
};

struct gctx *ngli_gctx_create(struct ngl_ctx *ctx)
{
    struct gctx *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_gctx_init(struct gctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_config *config = &ctx->config;

    if (config->backend < 0 ||
        config->backend >= NGLI_ARRAY_NB(backend_map) ||
        !backend_map[config->backend]) {
        LOG(ERROR, "unknown backend %d", config->backend);
        return NGL_ERROR_INVALID_ARG;
    }
    s->class = backend_map[config->backend];

    return s->class->init(s);
}

int ngli_gctx_resize(struct gctx *s, int width, int height, const int *viewport)
{
    const struct gctx_class *class = s->class;
    return class->resize(s, width, height, viewport);
}

int ngli_gctx_draw(struct gctx *s, double t)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct gctx_class *class = s->class;

    int ret = class->pre_draw(s, t);
    if (ret < 0)
        goto end;

    if (ctx->scene) {
        LOG(DEBUG, "draw scene %s @ t=%f", ctx->scene->label, t);
        ngli_node_draw(ctx->scene);
    }

end:;
    int end_ret = class->post_draw(s, t);
    if (end_ret < 0)
        return end_ret;

    return ret;
}

void ngli_gctx_freep(struct gctx **sp)
{
    if (!*sp)
        return;

    struct gctx *s = *sp;
    const struct gctx_class *class = s->class;
    if (class)
        class->destroy(s);

    ngli_freep(sp);
}

void ngli_gctx_set_rendertarget(struct gctx *s, struct rendertarget *rt)
{
    struct glcontext *gl = s->glcontext;

    if (rt == s->rendertarget)
        return;

    struct rendertarget_gl *rt_gl = (struct rendertarget_gl *)rt;
    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    s->rendertarget = rt;
}

struct rendertarget *ngli_gctx_get_rendertarget(struct gctx *s)
{
    return s->rendertarget;
}

void ngli_gctx_set_viewport(struct gctx *s, const int *viewport)
{
    struct glcontext *gl = s->glcontext;
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
    memcpy(&s->viewport, viewport, sizeof(s->viewport));
}

void ngli_gctx_get_viewport(struct gctx *s, int *viewport)
{
    memcpy(viewport, &s->viewport, sizeof(s->viewport));
}

void ngli_gctx_set_scissor(struct gctx *s, const int *scissor)
{
    struct glcontext *gl = s->glcontext;
    ngli_glScissor(gl, scissor[0], scissor[1], scissor[2], scissor[3]);
    memcpy(&s->scissor, scissor, sizeof(s->scissor));
}

void ngli_gctx_get_scissor(struct gctx *s, int *scissor)
{
    memcpy(scissor, &s->scissor, sizeof(s->scissor));
}

void ngli_gctx_set_clear_color(struct gctx *s, const float *color)
{
    struct glcontext *gl = s->glcontext;
    memcpy(s->clear_color, color, sizeof(s->clear_color));
    ngli_glClearColor(gl, color[0], color[1], color[2], color[3]);
}

void ngli_gctx_get_clear_color(struct gctx *s, float *color)
{
    memcpy(color, &s->clear_color, sizeof(s->clear_color));
}

void ngli_gctx_clear_color(struct gctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct glstate *glstate = &s->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

void ngli_gctx_clear_depth_stencil(struct gctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct glstate *glstate = &s->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

void ngli_gctx_invalidate_depth_stencil(struct gctx *s)
{
    struct glcontext *gl = s->glcontext;

    if (!(gl->features & NGLI_FEATURE_INVALIDATE_SUBDATA))
        return;

    static const GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    ngli_glInvalidateFramebuffer(gl, GL_FRAMEBUFFER, NGLI_ARRAY_NB(attachments), attachments);
}
