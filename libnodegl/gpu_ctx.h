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

#ifndef GPU_CTX_H
#define GPU_CTX_H

#include <stdint.h>

#include "buffer.h"
#include "feature.h"
#include "gpu_limits.h"
#include "nodegl.h"
#include "pipeline.h"
#include "rendertarget.h"
#include "texture.h"

struct gpu_ctx_class {
    const char *name;

    struct gpu_ctx *(*create)(const struct ngl_config *config);
    int (*init)(struct gpu_ctx *s);
    int (*resize)(struct gpu_ctx *s, int width, int height, const int *viewport);
    int (*set_capture_buffer)(struct gpu_ctx *s, void *capture_buffer);
    int (*begin_update)(struct gpu_ctx *s, double t);
    int (*end_update)(struct gpu_ctx *s, double t);
    int (*begin_draw)(struct gpu_ctx *s, double t);
    int (*end_draw)(struct gpu_ctx *s, double t);
    int (*query_draw_time)(struct gpu_ctx *s, int64_t *time);
    void (*wait_idle)(struct gpu_ctx *s);
    void (*destroy)(struct gpu_ctx *s);

    int (*transform_cull_mode)(struct gpu_ctx *s, int cull_mode);
    void (*transform_projection_matrix)(struct gpu_ctx *s, float *dst);
    void (*get_rendertarget_uvcoord_matrix)(struct gpu_ctx *s, float *dst);

    struct rendertarget *(*get_default_rendertarget)(struct gpu_ctx *s);
    const struct rendertarget_desc *(*get_default_rendertarget_desc)(struct gpu_ctx *s);

    void (*begin_render_pass)(struct gpu_ctx *s, struct rendertarget *rt);
    void (*end_render_pass)(struct gpu_ctx *s);

    void (*set_viewport)(struct gpu_ctx *s, const int *viewport);
    void (*get_viewport)(struct gpu_ctx *s, int *viewport);
    void (*set_scissor)(struct gpu_ctx *s, const int *scissor);
    void (*get_scissor)(struct gpu_ctx *s, int *scissor);
    int (*get_preferred_depth_format)(struct gpu_ctx *s);
    int (*get_preferred_depth_stencil_format)(struct gpu_ctx *s);

    struct buffer *(*buffer_create)(struct gpu_ctx *ctx);
    int (*buffer_init)(struct buffer *s, int size, int usage);
    int (*buffer_upload)(struct buffer *s, const void *data, int size, int offset);
    void (*buffer_freep)(struct buffer **sp);

    struct pipeline *(*pipeline_create)(struct gpu_ctx *ctx);
    int (*pipeline_init)(struct pipeline *s, const struct pipeline_params *params);
    int (*pipeline_set_resources)(struct pipeline *s, const struct pipeline_resource_params *data_params);
    int (*pipeline_update_attribute)(struct pipeline *s, int index, struct buffer *buffer);
    int (*pipeline_update_uniform)(struct pipeline *s, int index, const void *value);
    int (*pipeline_update_texture)(struct pipeline *s, int index, struct texture *texture);
    int (*pipeline_update_buffer)(struct pipeline *s, int index, struct buffer *buffer);
    void (*pipeline_draw)(struct pipeline *s, int nb_vertices, int nb_instances);
    void (*pipeline_draw_indexed)(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances);
    void (*pipeline_dispatch)(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z);
    void (*pipeline_freep)(struct pipeline **sp);

    struct program *(*program_create)(struct gpu_ctx *ctx);
    int (*program_init)(struct program *s, const char *vertex, const char *fragment, const char *compute);
    void (*program_freep)(struct program **sp);

    struct rendertarget *(*rendertarget_create)(struct gpu_ctx *ctx);
    int (*rendertarget_init)(struct rendertarget *s, const struct rendertarget_params *params);
    void (*rendertarget_read_pixels)(struct rendertarget *s, uint8_t *data);
    void (*rendertarget_freep)(struct rendertarget **sp);

    struct texture *(*texture_create)(struct gpu_ctx *ctx);
    int (*texture_init)(struct texture *s, const struct texture_params *params);
    int (*texture_upload)(struct texture *s, const uint8_t *data, int linesize);
    int (*texture_generate_mipmap)(struct texture *s);
    void (*texture_freep)(struct texture **sp);
};

struct gpu_ctx {
    struct ngl_config config;
    const char *backend_str;
    const struct gpu_ctx_class *cls;
    int version;
    int language_version;
    uint64_t features;
    struct gpu_limits limits;
#if DEBUG_GPU_CAPTURE
    struct gpu_capture_ctx *gpu_capture_ctx;
    int gpu_capture;
#endif
};

struct gpu_ctx *ngli_gpu_ctx_create(const struct ngl_config *config);
int ngli_gpu_ctx_init(struct gpu_ctx *s);
int ngli_gpu_ctx_resize(struct gpu_ctx *s, int width, int height, const int *viewport);
int ngli_gpu_ctx_set_capture_buffer(struct gpu_ctx *s, void *capture_buffer);
int ngli_gpu_ctx_update(struct gpu_ctx *s, struct ngl_node *scene, double t);
int ngli_gpu_ctx_begin_draw(struct gpu_ctx *s, double t);
int ngli_gpu_ctx_query_draw_time(struct gpu_ctx *s, int64_t *time);
int ngli_gpu_ctx_end_draw(struct gpu_ctx *s, double t);
void ngli_gpu_ctx_wait_idle(struct gpu_ctx *s);
void ngli_gpu_ctx_freep(struct gpu_ctx **sp);

int ngli_gpu_ctx_transform_cull_mode(struct gpu_ctx *s, int cull_mode);
void ngli_gpu_ctx_transform_projection_matrix(struct gpu_ctx *s, float *dst);
void ngli_gpu_ctx_get_rendertarget_uvcoord_matrix(struct gpu_ctx *s, float *dst);

struct rendertarget *ngli_gpu_ctx_get_default_rendertarget(struct gpu_ctx *s);
const struct rendertarget_desc *ngli_gpu_ctx_get_default_rendertarget_desc(struct gpu_ctx *s);

void ngli_gpu_ctx_begin_render_pass(struct gpu_ctx *s, struct rendertarget *rt);
void ngli_gpu_ctx_end_render_pass(struct gpu_ctx *s);

void ngli_gpu_ctx_set_viewport(struct gpu_ctx *s, const int *viewport);
void ngli_gpu_ctx_get_viewport(struct gpu_ctx *s, int *viewport);
void ngli_gpu_ctx_set_scissor(struct gpu_ctx *s, const int *scissor);
void ngli_gpu_ctx_get_scissor(struct gpu_ctx *s, int *scissor);

int ngli_gpu_ctx_get_preferred_depth_format(struct gpu_ctx *s);
int ngli_gpu_ctx_get_preferred_depth_stencil_format(struct gpu_ctx *s);

#endif
