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

#include "buffer.h"
#include "colorconv.h"
#include "hmap.h"
#include "hwconv.h"
#include "glincludes.h"
#include "glcontext.h"
#include "gctx.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodes.h"
#include "pgcache.h"
#include "pipeline.h"
#include "program.h"
#include "texture.h"
#include "topology.h"
#include "type.h"
#include "utils.h"

#define VERTEX_DATA                                                              \
    "#version 100"                                                          "\n" \
    "precision highp float;"                                                "\n" \
    "attribute vec4 position;"                                              "\n" \
    "uniform mat4 tex_coord_matrix;"                                        "\n" \
    "varying vec2 tex_coord;"                                               "\n" \
    "void main()"                                                           "\n" \
    "{"                                                                     "\n" \
    "    gl_Position = vec4(position.xy, 0.0, 1.0);"                        "\n" \
    "    tex_coord = (tex_coord_matrix * vec4(position.zw, 0.0, 1.0)).xy;"  "\n" \
    "}"

#define OES_TO_RGBA_FRAGMENT_DATA                                                \
    "#version 100"                                                          "\n" \
    "#extension GL_OES_EGL_image_external : require"                        "\n" \
    "precision mediump float;"                                              "\n" \
    "uniform samplerExternalOES tex0;"                                      "\n" \
    "varying vec2 tex_coord;"                                               "\n" \
    "void main()"                                                           "\n" \
    "{"                                                                     "\n" \
    "    vec4 color = texture2D(tex0, tex_coord);"                          "\n" \
    "    gl_FragColor = vec4(color.rgb, 1.0);"                              "\n" \
    "}"

#define NV12_TO_RGBA_FRAGMENT_DATA                                               \
    "#version 100"                                                          "\n" \
    "precision mediump float;"                                              "\n" \
    "uniform sampler2D tex0;"                                               "\n" \
    "uniform sampler2D tex1;"                                               "\n" \
    "varying vec2 tex_coord;"                                               "\n" \
    "uniform mat4 color_matrix;"                                            "\n" \
    "void main()"                                                           "\n" \
    "{"                                                                     "\n" \
    "    vec3 yuv;"                                                         "\n" \
    "    yuv.x = texture2D(tex0, tex_coord).r;"                             "\n" \
    "    yuv.yz = texture2D(tex1, tex_coord).%s;"                           "\n" \
    "    gl_FragColor = color_matrix * vec4(yuv, 1.0);"                     "\n" \
    "}"

#define NV12_RECTANGLE_TO_RGBA_VERTEX_DATA                                       \
    "#version 410"                                                          "\n" \
    "precision highp float;"                                                "\n" \
    "uniform mat4 tex_coord_matrix;"                                        "\n" \
    "uniform vec2 tex_dimensions[2];"                                       "\n" \
    "in vec4 position;"                                                     "\n" \
    "out vec2 tex0_coord;"                                                  "\n" \
    "out vec2 tex1_coord;"                                                  "\n" \
    "void main()"                                                           "\n" \
    "{"                                                                     "\n" \
    "    gl_Position = vec4(position.xy, 0.0, 1.0);"                        "\n" \
    "    vec2 coord = (tex_coord_matrix * vec4(position.zw, 0.0, 1.0)).xy;" "\n" \
    "    tex0_coord = coord * tex_dimensions[0];"                           "\n" \
    "    tex1_coord = coord * tex_dimensions[1];"                           "\n" \
    "}"

#define NV12_RECTANGLE_TO_RGBA_FRAGMENT_DATA                                     \
    "#version 410"                                                          "\n" \
    "uniform mediump sampler2DRect tex0;"                                   "\n" \
    "uniform mediump sampler2DRect tex1;"                                   "\n" \
    "uniform mat4 color_matrix;"                                            "\n" \
    "in vec2 tex0_coord;"                                                   "\n" \
    "in vec2 tex1_coord;"                                                   "\n" \
    "out vec4 color;"                                                       "\n" \
    "void main()"                                                           "\n" \
    "{"                                                                     "\n" \
    "    vec3 yuv;"                                                         "\n" \
    "    yuv.x = texture(tex0, tex0_coord).r;"                              "\n" \
    "    yuv.yz = texture(tex1, tex1_coord).%s;"                            "\n" \
    "    color = color_matrix * vec4(yuv, 1.0);"                            "\n" \
    "}"

static const struct hwconv_desc {
    int nb_planes;
    const char *vertex_data;
    const char *fragment_data;
} hwconv_descs[] = {
    [NGLI_IMAGE_LAYOUT_MEDIACODEC] = {
        .nb_planes = 1,
        .vertex_data = VERTEX_DATA,
        .fragment_data = OES_TO_RGBA_FRAGMENT_DATA,
    },
    [NGLI_IMAGE_LAYOUT_NV12] = {
        .nb_planes = 2,
        .vertex_data = VERTEX_DATA,
        .fragment_data = NV12_TO_RGBA_FRAGMENT_DATA,
    },
    [NGLI_IMAGE_LAYOUT_NV12_RECTANGLE] = {
        .nb_planes = 2,
        .vertex_data = NV12_RECTANGLE_TO_RGBA_VERTEX_DATA,
        .fragment_data = NV12_RECTANGLE_TO_RGBA_FRAGMENT_DATA,
    },
};

int ngli_hwconv_init(struct hwconv *hwconv, struct ngl_ctx *ctx,
                     const struct image *dst_image,
                     const struct image_params *src_params)
{
    hwconv->ctx = ctx;
    hwconv->src_params = *src_params;

    if (dst_image->params.layout != NGLI_IMAGE_LAYOUT_DEFAULT) {
        LOG(ERROR, "unsupported output image layout: 0x%x", dst_image->params.layout);
        return NGL_ERROR_UNSUPPORTED;
    }

    struct glcontext *gl = ctx->glcontext;
    const struct texture *texture = dst_image->planes[0];

    struct rendertarget_params rt_params = {
        .width = dst_image->params.width,
        .height = dst_image->params.height,
        .nb_colors = 1,
        .colors[0] = texture,
    };
    int ret = ngli_rendertarget_init(&hwconv->rt, ctx, &rt_params);
    if (ret < 0)
        return ret;

    enum image_layout src_layout = src_params->layout;
    if (src_layout != NGLI_IMAGE_LAYOUT_NV12 &&
        src_layout != NGLI_IMAGE_LAYOUT_NV12_RECTANGLE &&
        src_layout != NGLI_IMAGE_LAYOUT_MEDIACODEC) {
        LOG(ERROR, "unsupported texture layout: 0x%x", src_layout);
        return NGL_ERROR_UNSUPPORTED;
    }
    const struct hwconv_desc *desc = &hwconv_descs[src_layout];

    const char *uv = gl->version < 300 ? "ra": "rg";
    char *fragment_data = ngli_asprintf(desc->fragment_data, uv);
    if (!fragment_data)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcache_get_graphics_program(&ctx->pgcache, &hwconv->program, desc->vertex_data, fragment_data);
    ngli_free(fragment_data);
    if (ret < 0)
        return ret;

    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };
    ret = ngli_buffer_init(&hwconv->vertices, ctx, sizeof(vertices), NGLI_BUFFER_USAGE_STATIC);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(&hwconv->vertices, vertices, sizeof(vertices));
    if (ret < 0)
        return ret;

    ngli_mat4_identity(hwconv->src_color_matrix);
    if (src_layout == NGLI_IMAGE_LAYOUT_NV12 ||
        src_layout == NGLI_IMAGE_LAYOUT_NV12_RECTANGLE) {
        ngli_colorconv_get_ycbcr_to_rgb_color_matrix(hwconv->src_color_matrix, &src_params->color_info);
    }

    const struct pipeline_uniform uniforms[] = {
        {.name = "tex_coord_matrix", .type = NGLI_TYPE_MAT4, .count = 1,               .data  = NULL},
        {.name = "tex_dimensions",   .type = NGLI_TYPE_VEC2, .count = desc->nb_planes, .data  = NULL},
        {.name = "color_matrix",     .type = NGLI_TYPE_MAT4, .count = 1,               .data  = hwconv->src_color_matrix},
    };

    struct pipeline_texture textures[] = {
        {.name = "tex0", .texture = NULL},
        {.name = "tex1", .texture = NULL},
    };
    for (int i = 0; i < desc->nb_planes; i++) {
        const struct program_variable_info *info = ngli_hmap_get(hwconv->program.uniforms, textures[i].name);
        if (!info)
            continue;
        textures[i].type     = info->type;
        textures[i].location = info->location;
        textures[i].binding  = info->binding;
    }

    const struct program_variable_info *position = ngli_hmap_get(hwconv->program.attributes, "position");
    ngli_assert(position);

    const struct pipeline_attribute attributes[] = {
        {
            .name     = "position",
            .location = position->location,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * 4,
            .buffer   = &hwconv->vertices,
        },
    };

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .program       = &hwconv->program,
        .textures      = textures,
        .nb_textures   = desc->nb_planes,
        .uniforms      = uniforms,
        .nb_uniforms   = NGLI_ARRAY_NB(uniforms),
        .attributes    = attributes,
        .nb_attributes = NGLI_ARRAY_NB(attributes),
        .graphics      = {
            .topology    = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
            .nb_vertices = 4,
            .state       = ctx->graphicstate,
        },
    };

    ret = ngli_pipeline_init(&hwconv->pipeline, ctx, &pipeline_params);
    if (ret < 0)
        return ret;

    hwconv->tex_indices[0] = ngli_pipeline_get_texture_index(&hwconv->pipeline, "tex0");
    hwconv->tex_indices[1] = ngli_pipeline_get_texture_index(&hwconv->pipeline, "tex1");
    hwconv->tex_coord_matrix_index = ngli_pipeline_get_uniform_index(&hwconv->pipeline, "tex_coord_matrix");
    hwconv->tex_dimensions_index = ngli_pipeline_get_uniform_index(&hwconv->pipeline, "tex_dimensions");

    return 0;
}

int ngli_hwconv_convert_image(struct hwconv *hwconv, const struct image *image)
{
    struct ngl_ctx *ctx = hwconv->ctx;
    ngli_assert(hwconv->src_params.layout == image->params.layout);

    struct rendertarget *rt = &hwconv->rt;
    struct rendertarget *prev_rt = ngli_gctx_get_rendertarget(ctx);
    ngli_gctx_set_rendertarget(ctx, rt);

    int prev_vp[4] = {0};
    ngli_gctx_get_viewport(ctx, prev_vp);

    const int vp[4] = {0, 0, rt->width, rt->height};
    ngli_gctx_set_viewport(ctx, vp);

    ngli_gctx_clear_color(ctx);

    const struct hwconv_desc *desc = &hwconv_descs[hwconv->src_params.layout];
    float dimensions[4] = {0};
    for (int i = 0; i < desc->nb_planes; i++) {
        struct texture *plane = image->planes[i];
        ngli_pipeline_update_texture(&hwconv->pipeline, hwconv->tex_indices[i], plane);

        const struct texture_params *params = &plane->params;
        dimensions[i*2 + 0] = params->width;
        dimensions[i*2 + 1] = params->height;
    }
    ngli_pipeline_update_uniform(&hwconv->pipeline, hwconv->tex_coord_matrix_index, image->coordinates_matrix);
    ngli_pipeline_update_uniform(&hwconv->pipeline, hwconv->tex_dimensions_index, dimensions);

    ngli_pipeline_exec(&hwconv->pipeline);

    ngli_gctx_set_rendertarget(ctx, prev_rt);
    ngli_gctx_set_viewport(ctx, prev_vp);

    return 0;
}

void ngli_hwconv_reset(struct hwconv *hwconv)
{
    struct ngl_ctx *ctx = hwconv->ctx;
    if (!ctx)
        return;

    ngli_pipeline_reset(&hwconv->pipeline);
    ngli_buffer_reset(&hwconv->vertices);
    ngli_pgcache_release_program(&hwconv->program);
    ngli_rendertarget_reset(&hwconv->rt);

    memset(hwconv, 0, sizeof(*hwconv));
}
