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

#ifndef GCTX_H
#define GCTX_H

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "buffer.h"
#include "features.h"
#include "glcontext.h"
#include "glstate.h"
#include "gtimer.h"
#include "limits.h"
#include "nodegl.h"
#include "pgcache.h"
#include "pipeline.h"
#include "rendertarget.h"
#include "texture.h"

struct gctx_class {
    const char *name;
    int (*init)(struct gctx *s);
    int (*resize)(struct gctx *s, int width, int height, const int *viewport);
    int (*pre_draw)(struct gctx *s, double t);
    int (*post_draw)(struct gctx *s, double t);
    void (*destroy)(struct gctx *s);

    struct buffer *(*buffer_create)(struct gctx *ctx);
    int (*buffer_init)(struct buffer *s, int size, int usage);
    int (*buffer_upload)(struct buffer *s, const void *data, int size);
    void (*buffer_freep)(struct buffer **sp);

    struct gtimer *(*gtimer_create)(struct gctx *ctx);
    int (*gtimer_init)(struct gtimer *s);
    int (*gtimer_start)(struct gtimer *s);
    int (*gtimer_stop)(struct gtimer *s);
    int64_t (*gtimer_read)(struct gtimer *s);
    void (*gtimer_freep)(struct gtimer **sp);

    struct pipeline *(*pipeline_create)(struct gctx *ctx);
    int (*pipeline_init)(struct pipeline *s, const struct pipeline_params *params);
    int (*pipeline_update_uniform)(struct pipeline *s, int index, const void *value);
    int (*pipeline_update_texture)(struct pipeline *s, int index, struct texture *texture);
    void (*pipeline_exec)(struct pipeline *s);
    void (*pipeline_freep)(struct pipeline **sp);

    struct rendertarget *(*rendertarget_create)(struct gctx *ctx);
    int (*rendertarget_init)(struct rendertarget *s, const struct rendertarget_params *params);
    void (*rendertarget_blit)(struct rendertarget *s, struct rendertarget *dst, int vflip);
    void (*rendertarget_resolve)(struct rendertarget *s);
    void (*rendertarget_read_pixels)(struct rendertarget *s, uint8_t *data);
    void (*rendertarget_freep)(struct rendertarget **sp);

    struct texture *(*texture_create)(struct gctx* ctx);
    int (*texture_init)(struct texture *s, const struct texture_params *params);
    int (*texture_has_mipmap)(const struct texture *s);
    int (*texture_match_dimensions)(const struct texture *s, int width, int height, int depth);
    int (*texture_upload)(struct texture *s, const uint8_t *data, int linesize);
    int (*texture_generate_mipmap)(struct texture *s);
    void (*texture_freep)(struct texture **sp);
};

typedef void (*capture_func_type)(struct gctx *s);

struct gctx {
    struct ngl_ctx *ctx;
    const struct gctx_class *class;
    int version;
    int features;
    struct limits limits;
    struct glcontext *glcontext;
    struct glstate glstate;
    struct rendertarget *rendertarget;
    struct rendertarget_desc default_rendertarget_desc;
    int viewport[4];
    int scissor[4];
    float clear_color[4];
    int program_id;
    struct pgcache pgcache;
    int timer_active;
    /* Offscreen render target */
    struct rendertarget *rt;
    struct texture *rt_color;
    struct texture *rt_depth;
    /* Capture offscreen render target */
    capture_func_type capture_func;
    struct rendertarget *oes_resolve_rt;
    struct texture *oes_resolve_rt_color;
    struct rendertarget *capture_rt;
    struct texture *capture_rt_color;
    uint8_t *capture_buffer;
#if defined(TARGET_IPHONE)
    CVPixelBufferRef capture_cvbuffer;
    CVOpenGLESTextureRef capture_cvtexture;
#endif
};

struct gctx *ngli_gctx_create(struct ngl_ctx *ctx);
int ngli_gctx_init(struct gctx *s);
int ngli_gctx_resize(struct gctx *s, int width, int height, const int *viewport);
int ngli_gctx_draw(struct gctx *s, double t);
void ngli_gctx_freep(struct gctx **sp);

void ngli_gctx_set_rendertarget(struct gctx *s, struct rendertarget *rt);
struct rendertarget *ngli_gctx_get_rendertarget(struct gctx *s);

void ngli_gctx_set_viewport(struct gctx *s, const int *viewport);
void ngli_gctx_get_viewport(struct gctx *s, int *viewport);
void ngli_gctx_set_scissor(struct gctx *s, const int *scissor);
void ngli_gctx_get_scissor(struct gctx *s, int *scissor);

void ngli_gctx_set_clear_color(struct gctx *s, const float *color);
void ngli_gctx_get_clear_color(struct gctx *s, float *color);

void ngli_gctx_clear_color(struct gctx *s);
void ngli_gctx_clear_depth_stencil(struct gctx *s);
void ngli_gctx_invalidate_depth_stencil(struct gctx *s);

#endif
