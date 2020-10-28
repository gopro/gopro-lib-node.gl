/*
 * Copyright 2020 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <float.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "distmap.h"
#include "gpu_ctx.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "path.h"
#include "path_frag.h"
#include "path_vert.h"
#include "pgcraft.h"
#include "pipeline.h"
#include "texture.h"
#include "topology.h"
#include "type.h"
#include "utils.h"


struct pipeline_desc {
    struct pgcraft *crafter;
    struct pipeline *pipeline;

    int modelview_matrix_index;
    int projection_matrix_index;
    int color_index;
    int outline_index;
    int glow_index;
    int glow_color_index;
    int blur_index;
};

struct pathdraw_priv {
    struct ngl_node *path_node;

    struct ngl_node *color_node;
    struct ngl_node *outline_node;
    struct ngl_node *glow_node;
    struct ngl_node *glow_color_node;
    struct ngl_node *blur_node;

    float color[4];
    float outline;
    float glow;
    float glow_color[4];
    float blur;

    const float *color_p;
    const float *outline_p;
    const float *glow_p;
    const float *glow_color_p;
    const float *blur_p;

    float poly_corner[2];
    float poly_width[2];
    float poly_height[2];

    struct distmap *distmap;
    struct buffer *vertices;
    struct darray pipeline_descs;
};

#define FLOAT_NODE_TYPES (const int[]){NGL_NODE_UNIFORMFLOAT, NGL_NODE_ANIMATEDFLOAT, NGL_NODE_NOISEFLOAT, -1}
#define VEC4_NODE_TYPES  (const int[]){NGL_NODE_UNIFORMVEC4,  NGL_NODE_ANIMATEDVEC4,  -1}

#define OFFSET(x) offsetof(struct pathdraw_priv, x)
static const struct node_param pathdraw_params[] = {
    {"path",         NGLI_PARAM_TYPE_NODE, OFFSET(path_node),
                     .node_types=(const int[]){NGL_NODE_PATH, NGL_NODE_SMOOTHPATH, -1},
                     .flags=NGLI_PARAM_FLAG_NON_NULL,
                     .desc=NGLI_DOCSTRING("path to draw")},

    {"color",        NGLI_PARAM_TYPE_NODE, OFFSET(color_node),
                     .node_types=VEC4_NODE_TYPES,
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .desc=NGLI_DOCSTRING("characters fill color")},
    {"outline",      NGLI_PARAM_TYPE_NODE, OFFSET(outline_node),
                     .node_types=FLOAT_NODE_TYPES,
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .desc=NGLI_DOCSTRING("characters outline width")},
    {"glow",         NGLI_PARAM_TYPE_NODE, OFFSET(glow_node),
                     .node_types=FLOAT_NODE_TYPES,
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .desc=NGLI_DOCSTRING("characters stroke width")},
    {"glow_color",   NGLI_PARAM_TYPE_NODE, OFFSET(glow_color_node),
                     .node_types=VEC4_NODE_TYPES,
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .desc=NGLI_DOCSTRING("characters stroke color")},
    {"blur",         NGLI_PARAM_TYPE_NODE, OFFSET(blur_node),
                     .node_types=FLOAT_NODE_TYPES,
                     .flags=NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .desc=NGLI_DOCSTRING("characters blur")},

    {"poly_corner",  NGLI_PARAM_TYPE_VEC2, OFFSET(poly_corner), {.vec={-1.0f, -1.0f}},
                     .desc=NGLI_DOCSTRING("origin coordinates of `poly_width` and `poly_height` vectors")},
    {"poly_width",   NGLI_PARAM_TYPE_VEC2, OFFSET(poly_width),  {.vec={2.0f, 0.0f}},
                     .desc=NGLI_DOCSTRING("width vector of the coordinate space")},
    {"poly_height",  NGLI_PARAM_TYPE_VEC2, OFFSET(poly_height), {.vec={0.0f, 2.0f}},
                     .desc=NGLI_DOCSTRING("height vector of the coordinate space")},

    // XXX corner style?
    // XXX cap style?
    {NULL}
};

#define PATH_DISTMAP_SIZE 256
#define PATH_DISTMAP_SPREAD 32

#define ASSIGN_EFFECT_PTR_VECTOR(name) do {                         \
    if (s->name##_node) {                                           \
        const struct variable_priv *v = s->name##_node->priv_data;  \
        s->name##_p = v->vector;                                    \
    } else {                                                        \
        s->name##_p = s->name;                                      \
    }                                                               \
} while (0)

#define ASSIGN_EFFECT_PTR_SCALAR(name) do {                         \
    if (s->name##_node) {                                           \
        const struct variable_priv *v = s->name##_node->priv_data;  \
        s->name##_p = &v->scalar;                                   \
    } else {                                                        \
        s->name##_p = &s->name;                                     \
    }                                                               \
} while (0)

static int pathdraw_init(struct ngl_node *node)
{
    struct pathdraw_priv *s = node->priv_data;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    /* default values */
    s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.f;
    s->outline = 0.005f;
    s->glow = 0.f;
    s->glow_color[0] = s->glow_color[1] = s->glow_color[2] = s->glow_color[3] = 1.f;
    s->blur = 0.f;

    ASSIGN_EFFECT_PTR_VECTOR(color);
    ASSIGN_EFFECT_PTR_SCALAR(outline);
    ASSIGN_EFFECT_PTR_SCALAR(glow);
    ASSIGN_EFFECT_PTR_VECTOR(glow_color);
    ASSIGN_EFFECT_PTR_SCALAR(blur);

    s->distmap = ngli_distmap_create(node->ctx);
    if (!s->distmap)
        return NGL_ERROR_MEMORY;

    const struct distmap_params params = {
        //.spread      = PATH_DISTMAP_SPREAD,
        .shape_w     = PATH_DISTMAP_SIZE,
        .shape_h     = PATH_DISTMAP_SIZE,
        .poly_corner = {NGLI_ARG_VEC2(s->poly_corner)},
        .poly_width  = {NGLI_ARG_VEC2(s->poly_width)},
        .poly_height = {NGLI_ARG_VEC2(s->poly_height)},
    };
    int ret = ngli_distmap_init(s->distmap, &params);
    if (ret < 0)
        return ret;

    struct path *path = *(struct path **)s->path_node->priv_data;
    ret = ngli_path_add_to_distmap(path, s->distmap, 0);
    if (ret < 0)
        return ret;

    return ngli_distmap_generate_texture(s->distmap);
}

static const struct pgcraft_iovar io_vars[] = {
    {.name = "var_tex_coord", .type = NGLI_TYPE_VEC2},
};

static int pathdraw_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct pathdraw_priv *s = node->priv_data;

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    ctx->rnode_pos->id = ngli_darray_count(&s->pipeline_descs) - 1;

    memset(desc, 0, sizeof(*desc));

    struct rnode *rnode = ctx->rnode_pos;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    float uvcoords[8];
    ngli_distmap_get_shape_coords(s->distmap, 0, uvcoords);
    const float vertices[] = {
        -1.0f, -1.0f, uvcoords[0], uvcoords[1],
         1.0f, -1.0f, uvcoords[2], uvcoords[3],
        -1.0f,  1.0f, uvcoords[4], uvcoords[5],
         1.0f,  1.0f, uvcoords[6], uvcoords[7],
    };

    // XXX: memleak alert! each prepare will override the existing pointers
    s->vertices = ngli_buffer_create(gpu_ctx);
    if (!s->vertices)
        return NGL_ERROR_MEMORY;

    int ret = ngli_buffer_init(s->vertices, sizeof(vertices), NGLI_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                              NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(s->vertices, vertices, sizeof(vertices), 0);
    if (ret < 0)
        return ret;

    struct texture *texture = ngli_distmap_get_texture(s->distmap);
    struct pgcraft_texture textures[] = {
        {.name = "tex", .type = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D, .stage = NGLI_PROGRAM_SHADER_FRAG, .texture = texture},
    };

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4,  .stage = NGLI_PROGRAM_SHADER_VERT},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4,  .stage = NGLI_PROGRAM_SHADER_VERT},
        {.name = "color",             .type = NGLI_TYPE_VEC4,  .stage = NGLI_PROGRAM_SHADER_FRAG},
        {.name = "outline",           .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG},
        {.name = "glow",              .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG},
        {.name = "glow_color",        .type = NGLI_TYPE_VEC4,  .stage = NGLI_PROGRAM_SHADER_FRAG},
        {.name = "blur",              .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG},
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * 4,
            .buffer   = s->vertices,
        },
    };

    struct graphicstate state = rnode->graphicstate;
    state.blend = 1;
    state.blend_src_factor   = NGLI_BLEND_FACTOR_SRC_ALPHA;
    state.blend_dst_factor   = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.blend_src_factor_a = NGLI_BLEND_FACTOR_SRC_ALPHA;
    state.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology       = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state          = state,
            .rt_desc        = rnode->rendertarget_desc,
        }
    };

    const struct pgcraft_params crafter_params = {
        .vert_base        = path_vert,
        .frag_base        = path_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = io_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(io_vars),
    };

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    struct pipeline_resource_params pipeline_resource_params = {0};
    ret = ngli_pgcraft_craft(desc->crafter, &pipeline_params, &pipeline_resource_params, &crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline = ngli_pipeline_create(gpu_ctx);
    if (!desc->pipeline)
        return NGL_ERROR_MEMORY;

    ret = ngli_pipeline_init(desc->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    ret = ngli_pipeline_set_resources(desc->pipeline, &pipeline_resource_params);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index  = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix",  NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);

    desc->color_index      = ngli_pgcraft_get_uniform_index(desc->crafter, "color",      NGLI_PROGRAM_SHADER_FRAG);
    desc->outline_index    = ngli_pgcraft_get_uniform_index(desc->crafter, "outline",    NGLI_PROGRAM_SHADER_FRAG);
    desc->glow_index       = ngli_pgcraft_get_uniform_index(desc->crafter, "glow",       NGLI_PROGRAM_SHADER_FRAG);
    desc->glow_color_index = ngli_pgcraft_get_uniform_index(desc->crafter, "glow_color", NGLI_PROGRAM_SHADER_FRAG);
    desc->blur_index       = ngli_pgcraft_get_uniform_index(desc->crafter, "blur",       NGLI_PROGRAM_SHADER_FRAG);

    return 0;
}

static int pathdraw_update(struct ngl_node *node, double t)
{
    struct pathdraw_priv *s = node->priv_data;

    if (s->color_node) {
        int ret = ngli_node_update(s->color_node, t);
        if (ret < 0)
            return ret;
    }

    if (s->outline_node) {
        int ret = ngli_node_update(s->outline_node, t);
        if (ret < 0)
            return ret;
    }

    if (s->glow_node) {
        int ret = ngli_node_update(s->glow_node, t);
        if (ret < 0)
            return ret;
    }

    if (s->glow_color_node) {
        int ret = ngli_node_update(s->glow_color_node, t);
        if (ret < 0)
            return ret;
    }

    if (s->blur_node) {
        int ret = ngli_node_update(s->blur_node, t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void pathdraw_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct pathdraw_priv *s = node->priv_data;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];

    if (ctx->begin_render_pass) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->begin_render_pass = 0;
    }

    ngli_pipeline_update_uniform(desc->pipeline, desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_update_uniform(desc->pipeline, desc->projection_matrix_index, projection_matrix);

    ngli_pipeline_update_uniform(desc->pipeline, desc->color_index, s->color_p);
    ngli_pipeline_update_uniform(desc->pipeline, desc->outline_index, s->outline_p);
    ngli_pipeline_update_uniform(desc->pipeline, desc->glow_index, s->glow_p);
    ngli_pipeline_update_uniform(desc->pipeline, desc->glow_color_index, s->glow_color_p);
    ngli_pipeline_update_uniform(desc->pipeline, desc->blur_index, s->blur_p);

    ngli_pipeline_draw(desc->pipeline, 4, 1);
}

static void pathdraw_uninit(struct ngl_node *node)
{
    struct pathdraw_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const int nb_descs = ngli_darray_count(&s->pipeline_descs);
    for (int i = 0; i < nb_descs; i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_freep(&desc->pipeline);
        ngli_pgcraft_freep(&desc->crafter);
    }
    ngli_buffer_freep(&s->vertices);
    ngli_darray_reset(&s->pipeline_descs);
    ngli_distmap_freep(&s->distmap);
}

const struct node_class ngli_pathdraw_class = {
    .id        = NGL_NODE_PATHDRAW,
    .name      = "PathDraw",
    .init      = pathdraw_init,
    .prepare   = pathdraw_prepare,
    .update    = pathdraw_update,
    .draw      = pathdraw_draw,
    .uninit    = pathdraw_uninit,
    .priv_size = sizeof(struct pathdraw_priv),
    .params    = pathdraw_params,
    .file      = __FILE__,
};
