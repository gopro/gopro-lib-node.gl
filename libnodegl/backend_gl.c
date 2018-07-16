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

#include "backend.h"
#include "glcontext.h"

static int gl_reconfigure(struct ngl_ctx *s, const struct ngl_config *config)
{
    if (s->glcontext->wrapped)
        return -1;

    int ret = ngli_glcontext_resize(s->glcontext, config->width, config->height);
    if (ret < 0)
        return ret;

    struct ngl_config *current_config = &s->config;
    current_config->width = config->width;
    current_config->height = config->height;

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);
        memcpy(current_config->viewport, config->viewport, sizeof(config->viewport));
    }

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);
    memcpy(current_config->clear_color, config->clear_color, sizeof(config->clear_color));

    return 0;
}

static int gl_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    memcpy(&s->config, config, sizeof(s->config));

    s->glcontext = ngli_glcontext_new(&s->config);
    if (!s->glcontext)
        return -1;

    if (!s->glcontext->wrapped) {
        ngli_glcontext_make_current(s->glcontext, 1);
        if (s->config.swap_interval >= 0)
            ngli_glcontext_set_swap_interval(s->glcontext, s->config.swap_interval);
    }

    int ret = ngli_glcontext_load_extensions(s->glcontext);
    if (ret < 0)
        return ret;

    ngli_glstate_probe(s->glcontext, &s->glstate);

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0)
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);

    return 0;
}

#if defined(TARGET_IPHONE)
static int cmd_make_current(struct ngl_ctx *s, void *arg)
{
    const int current = *(int *)arg;
    ngli_glcontext_make_current(s->glcontext, current);
    return 0;
}

static int gl_configure_ios(struct ngl_ctx *s, const struct ngl_config *config)
{
    if (!s->has_thread)
        return gl_configure(s, config);

    int ret = gl_configure(s, config);
    if (ret < 0)
        return ret;
    cmd_make_current(s, DONE_CURRENT);

    return ngli_dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
}

#define MAKE_CURRENT &(int[]){1}
#define DONE_CURRENT &(int[]){0}
static int gl_reconfigure_ios(struct ngl_ctx *s, const struct ngl_config *config)
{
    if (!s->has_thread)
        return gl_reconfigure(s, config);

    int ret = ngli_dispatch_cmd(s, cmd_make_current, DONE_CURRENT);
    if (ret < 0)
        return ret;

    cmd_make_current(s, MAKE_CURRENT);
    ret = gl_reconfigure(s, config);
    cmd_make_current(s, DONE_CURRENT);

    int ret_dispatch = ngli_dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
    return ret ? ret : ret_dispatch;
}
#endif

static int gl_pre_draw(struct ngl_ctx *s)
{
    const struct glcontext *gl = s->glcontext;

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    return 0;
}

static int gl_post_draw(struct ngl_ctx *s, double t, int ret)
{
    struct glcontext *gl = s->glcontext;

    if (ret == 0 && ngli_glcontext_check_gl_error(gl, __FUNCTION__))
        ret = -1;

    if (gl->set_surface_pts)
        ngli_glcontext_set_surface_pts(gl, t);

    if (!gl->wrapped)
        ngli_glcontext_swap_buffers(gl);

    return ret;
}

static int gl_destroy(struct ngl_ctx *s)
{
    ngli_glcontext_freep(&s->glcontext);
    return 0;
}

const struct backend ngli_backend_gl = {
    .name         = "OpenGL",
    .reconfigure  = gl_reconfigure,
    .configure    = gl_configure,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,
};

const struct backend ngli_backend_gles = {
    .name         = "OpenGL ES",
#if defined(TARGET_IPHONE)
    .int_cfg_dp   = 1,
    .reconfigure  = gl_reconfigure_ios,
    .configure    = gl_configure_ios,
#else
    .reconfigure  = gl_reconfigure,
    .configure    = gl_configure,
#endif
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,
};
