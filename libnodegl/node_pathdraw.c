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
    //int color_index;
};

struct pathdraw_priv {
    struct ngl_node *path_node;
    int fill;

    struct distmap *distmap;
    struct buffer *vertices;
    struct darray pipeline_descs;
};

//#define FLOAT_NODE_TYPES (const int[]){NGL_NODE_UNIFORMFLOAT, NGL_NODE_ANIMATEDFLOAT, NGL_NODE_NOISE, -1}

#define OFFSET(x) offsetof(struct pathdraw_priv, x)
static const struct node_param pathdraw_params[] = {
    {"path",         NGLI_PARAM_TYPE_NODE, OFFSET(path_node),
                     .node_types=(const int[]){NGL_NODE_PATH, NGL_NODE_SMOOTHPATH, -1},
                     .flags=NGLI_PARAM_FLAG_NON_NULL,
                     .desc=NGLI_DOCSTRING("path to draw")},
    {"fill",         NGLI_PARAM_TYPE_BOOL, OFFSET(fill),
                     .desc=NGLI_DOCSTRING("fill the content of the path")},
    //{"start_pos",    PARAM_TYPE_NODE, OFFSET(start_pos_node),
    //                 .node_types=FLOAT_NODE_TYPES,
    //                 .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
    //                 .desc=NGLI_DOCSTRING("path position where the drawing starts")},
    //{"end_pos",      PARAM_TYPE_NODE, OFFSET(end_pos_node),
    //                 .node_types=FLOAT_NODE_TYPES,
    //                 .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
    //                 .desc=NGLI_DOCSTRING("path position where the drawing ends")},
    //{"thickness",    PARAM_TYPE_NODE, OFFSET(thickness_node),
    //                 .node_types=FLOAT_NODE_TYPES,
    //                 .desc=NGLI_DOCSTRING("thickness of the drawn path")},
    //{"colors",       PARAM_TYPE_NODE, OFFSET(colors_node),
    //                 .node_types=(const int[]){
    //                     NGL_NODE_UNIFORMVEC4,
    //                     NGL_NODE_ANIMATEDVEC4,
    //                     NGL_NODE_BUFFERVEC4,
    //                     NGL_NODE_ANIMATEDBUFFERVEC4,
    //                     -1
    //                 },
    //                 .desc=NGLI_DOCSTRING("path color if single vec4, color at every point if buffer")},
    // corner style
    // cap style
    {NULL}
};


static int pathdraw_init(struct ngl_node *node)
{
    struct pathdraw_priv *s = node->priv_data;

    //if (s->colors_node) {
    //    if (s->colors_node->class->category == NGLI_NODE_CATEGORY_BUFFER) {
    //        const struct pathdraw_priv *path = s->path_node->priv_data;
    //        const struct buffer_priv *points = path->points_buffer->priv_data;
    //        const struct buffer_priv *colors = s->colors_node->priv_data;
    //        if (colors->count != points->count) {
    //            LOG(ERROR, "the number of colors in the buffer (%d) must match the number of points in the path (%d)",
    //                colors->count, points->count);
    //            return NGL_ERROR_INVALID_ARG;
    //        }
    //    }
    //}

    struct path *path = *(struct path **)s->path_node->priv_data;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    s->distmap = ngli_distmap_create(node->ctx);
    if (!s->distmap)
        return NGL_ERROR_MEMORY;

    return ngli_path_init_distmap(path, s->distmap);
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
        {.name = "fill",              .type = NGLI_TYPE_BOOL, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = &s->fill},
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
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

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology       = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state          = rnode->graphicstate,
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

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    //desc->color_index = ngli_pgcraft_get_uniform_index(desc->crafter, "color", NGLI_PROGRAM_SHADER_FRAG);

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
    //ngli_pipeline_update_uniform(desc->pipeline, desc->color_index, s->color);
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
    .draw      = pathdraw_draw,
    .uninit    = pathdraw_uninit,
    .priv_size = sizeof(struct pathdraw_priv),
    .params    = pathdraw_params,
    .file      = __FILE__,
};
