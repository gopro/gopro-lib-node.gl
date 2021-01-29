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
#include "rendertarget.h"

extern const struct gctx_class ngli_gctx_gl;
extern const struct gctx_class ngli_gctx_gles;

static const struct {
    const char *string_id;
    const struct gctx_class *cls;
} backend_map[] = {
    [NGL_BACKEND_OPENGL] = {
        .string_id = "opengl",
#ifdef BACKEND_GL
        .cls = &ngli_gctx_gl,
#endif
    },
    [NGL_BACKEND_OPENGLES] = {
        .string_id = "opengles",
#ifdef BACKEND_GL
        .cls = &ngli_gctx_gles,
#endif
    },
};

struct gctx *ngli_gctx_create(const struct ngl_config *config)
{
    if (config->backend < 0 ||
        config->backend >= NGLI_ARRAY_NB(backend_map)) {
        LOG(ERROR, "unknown backend %d", config->backend);
        return NULL;
    }
    if (!backend_map[config->backend].cls) {
        LOG(ERROR, "backend \"%s\" not available with this build",
            backend_map[config->backend].string_id);
        return NULL;
    }
    const struct gctx_class *class = backend_map[config->backend].cls;
    struct gctx *s = class->create(config);
    if (!s)
        return NULL;
    s->config = *config;
    s->backend_str = backend_map[config->backend].string_id;
    s->class = class;
    return s;
}

int ngli_gctx_init(struct gctx *s)
{
    return s->class->init(s);
}

int ngli_gctx_resize(struct gctx *s, int width, int height, const int *viewport)
{
    const struct gctx_class *class = s->class;
    return class->resize(s, width, height, viewport);
}

int ngli_gctx_set_capture_buffer(struct gctx *s, void *capture_buffer)
{
    const struct gctx_class *class = s->class;
    return class->set_capture_buffer(s, capture_buffer);
}

int ngli_gctx_begin_draw(struct gctx *s, double t)
{
    return s->class->begin_draw(s, t);
}

int ngli_gctx_end_draw(struct gctx *s, double t)
{
    return s->class->end_draw(s, t);
}

int ngli_gctx_query_draw_time(struct gctx *s, int64_t *time)
{
    return s->class->query_draw_time(s, time);
}

void ngli_gctx_wait_idle(struct gctx *s)
{
    s->class->wait_idle(s);
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

int ngli_gctx_transform_cull_mode(struct gctx *s, int cull_mode)
{
    return s->class->transform_cull_mode(s, cull_mode);
}

void ngli_gctx_transform_projection_matrix(struct gctx *s, float *dst)
{
    s->class->transform_projection_matrix(s, dst);
}

void ngli_gctx_begin_render_pass(struct gctx *s, struct rendertarget *rt)
{
    s->class->begin_render_pass(s, rt);
}

void ngli_gctx_end_render_pass(struct gctx *s)
{
    s->class->end_render_pass(s);
}

void ngli_gctx_get_rendertarget_uvcoord_matrix(struct gctx *s, float *dst)
{
    s->class->get_rendertarget_uvcoord_matrix(s, dst);
}

struct rendertarget *ngli_gctx_get_default_rendertarget(struct gctx *s)
{
    return s->class->get_default_rendertarget(s);
}

const struct rendertarget_desc *ngli_gctx_get_default_rendertarget_desc(struct gctx *s)
{
    return s->class->get_default_rendertarget_desc(s);
}

void ngli_gctx_set_viewport(struct gctx *s, const int *viewport)
{
    s->class->set_viewport(s, viewport);
}

void ngli_gctx_get_viewport(struct gctx *s, int *viewport)
{
    s->class->get_viewport(s, viewport);
}

void ngli_gctx_set_scissor(struct gctx *s, const int *scissor)
{
    s->class->set_scissor(s, scissor);
}

void ngli_gctx_get_scissor(struct gctx *s, int *scissor)
{
    s->class->get_scissor(s, scissor);
}

int ngli_gctx_get_preferred_depth_format(struct gctx *s)
{
    return s->class->get_preferred_depth_format(s);
}

int ngli_gctx_get_preferred_depth_stencil_format(struct gctx *s)
{
    return s->class->get_preferred_depth_stencil_format(s);
}
