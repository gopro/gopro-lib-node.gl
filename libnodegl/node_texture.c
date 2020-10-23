/*
 * Copyright 2016 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include "format.h"
#include "gctx.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "texture.h"

const struct param_choices ngli_mipmap_filter_choices = {
    .name = "mipmap_filter",
    .consts = {
        {"none",    NGLI_MIPMAP_FILTER_NONE,    .desc=NGLI_DOCSTRING("no mipmap generation")},
        {"nearest", NGLI_MIPMAP_FILTER_NEAREST, .desc=NGLI_DOCSTRING("nearest filtering")},
        {"linear",  NGLI_MIPMAP_FILTER_LINEAR,  .desc=NGLI_DOCSTRING("linear filtering")},
        {NULL}
    }
};

const struct param_choices ngli_filter_choices = {
    .name = "filter",
    .consts = {
        {"nearest", NGLI_FILTER_NEAREST, .desc=NGLI_DOCSTRING("nearest filtering")},
        {"linear",  NGLI_FILTER_LINEAR,  .desc=NGLI_DOCSTRING("linear filtering")},
        {NULL}
    }
};

static const struct param_choices wrap_choices = {
    .name = "wrap",
    .consts = {
        {"clamp_to_edge",   NGLI_WRAP_CLAMP_TO_EDGE,   .desc=NGLI_DOCSTRING("clamp to edge wrapping")},
        {"mirrored_repeat", NGLI_WRAP_MIRRORED_REPEAT, .desc=NGLI_DOCSTRING("mirrored repeat wrapping")},
        {"repeat",          NGLI_WRAP_REPEAT,          .desc=NGLI_DOCSTRING("repeat pattern wrapping")},
        {NULL}
    }
};

/* These formats are not in format.h because they do not represent a native GPU format */
#define NGLI_FORMAT_AUTO_DEPTH         (NGLI_FORMAT_NB + 1)
#define NGLI_FORMAT_AUTO_DEPTH_STENCIL (NGLI_FORMAT_NB + 2)

static const struct param_choices format_choices = {
    .name = "format",
    .consts = {
        {"undefined",            NGLI_FORMAT_UNDEFINED,           .desc=NGLI_DOCSTRING("undefined")},
        {"r8_unorm",             NGLI_FORMAT_R8_UNORM,            .desc=NGLI_DOCSTRING("8-bit unsigned normalized R component")},
        {"r8_snorm",             NGLI_FORMAT_R8_SNORM,            .desc=NGLI_DOCSTRING("8-bit signed normalized R component")},
        {"r8_uint",              NGLI_FORMAT_R8_UINT,             .desc=NGLI_DOCSTRING("8-bit unsigned integer R component")},
        {"r8_sint",              NGLI_FORMAT_R8_SINT,             .desc=NGLI_DOCSTRING("8-bit signed integer R component")},
        {"r8g8_unorm",           NGLI_FORMAT_R8G8_UNORM,          .desc=NGLI_DOCSTRING("8-bit unsigned normalized RG components")},
        {"r8g8_snorm",           NGLI_FORMAT_R8G8_SNORM,          .desc=NGLI_DOCSTRING("8-bit signed normalized RG components")},
        {"r8g8_uint",            NGLI_FORMAT_R8G8_UINT,           .desc=NGLI_DOCSTRING("8-bit unsigned integer RG components")},
        {"r8g8_sint",            NGLI_FORMAT_R8G8_SINT,           .desc=NGLI_DOCSTRING("8-bit signed normalized RG components")},
        {"r8g8b8a8_unorm",       NGLI_FORMAT_R8G8B8A8_UNORM,      .desc=NGLI_DOCSTRING("8-bit unsigned normalized RGBA components")},
        {"r8g8b8a8_snorm",       NGLI_FORMAT_R8G8B8A8_SNORM,      .desc=NGLI_DOCSTRING("8-bit signed normalized RGBA components")},
        {"r8g8b8a8_uint",        NGLI_FORMAT_R8G8B8A8_UINT,       .desc=NGLI_DOCSTRING("8-bit unsigned integer RGBA components")},
        {"r8g8b8a8_sint",        NGLI_FORMAT_R8G8B8A8_SINT,       .desc=NGLI_DOCSTRING("8-bit signed integer RGBA components")},
        {"r8g8b8a8_srgb",        NGLI_FORMAT_R8G8B8A8_SRGB,       .desc=NGLI_DOCSTRING("8-bit unsigned normalized RGBA components")},
        {"b8g8r8a8_unorm",       NGLI_FORMAT_B8G8R8A8_UNORM,      .desc=NGLI_DOCSTRING("8-bit unsigned normalized BGRA components")},
        {"b8g8r8a8_snorm",       NGLI_FORMAT_B8G8R8A8_SNORM,      .desc=NGLI_DOCSTRING("8-bit signed normalized BGRA components")},
        {"b8g8r8a8_uint",        NGLI_FORMAT_B8G8R8A8_UINT,       .desc=NGLI_DOCSTRING("8-bit unsigned integer BGRA components")},
        {"b8g8r8a8_sint",        NGLI_FORMAT_B8G8R8A8_SINT,       .desc=NGLI_DOCSTRING("8-bit signed integer BGRA components")},
        {"r16_unorm",            NGLI_FORMAT_R16_UNORM,           .desc=NGLI_DOCSTRING("16-bit unsigned normalized R component")},
        {"r16_snorm",            NGLI_FORMAT_R16_SNORM,           .desc=NGLI_DOCSTRING("16-bit signed normalized R component")},
        {"r16_uint",             NGLI_FORMAT_R16_UINT,            .desc=NGLI_DOCSTRING("16-bit unsigned integer R component")},
        {"r16_sint",             NGLI_FORMAT_R16_SINT,            .desc=NGLI_DOCSTRING("16-bit signed integer R component")},
        {"r16_sfloat",           NGLI_FORMAT_R16_SFLOAT,          .desc=NGLI_DOCSTRING("16-bit signed float R component")},
        {"r16g16_unorm",         NGLI_FORMAT_R16G16_UNORM,        .desc=NGLI_DOCSTRING("16-bit unsigned normalized RG components")},
        {"r16g16_snorm",         NGLI_FORMAT_R16G16_SNORM,        .desc=NGLI_DOCSTRING("16-bit signed normalized RG components")},
        {"r16g16_uint",          NGLI_FORMAT_R16G16_UINT,         .desc=NGLI_DOCSTRING("16-bit unsigned integer RG components")},
        {"r16g16_sint",          NGLI_FORMAT_R16G16_SINT,         .desc=NGLI_DOCSTRING("16-bit signed integer RG components")},
        {"r16g16_sfloat",        NGLI_FORMAT_R16G16_SFLOAT,       .desc=NGLI_DOCSTRING("16-bit signed float RG components")},
        {"r16g16b16a16_unorm",   NGLI_FORMAT_R16G16B16A16_UNORM,  .desc=NGLI_DOCSTRING("16-bit unsigned normalized RGBA components")},
        {"r16g16b16a16_snorm",   NGLI_FORMAT_R16G16B16A16_SNORM,  .desc=NGLI_DOCSTRING("16-bit signed normalized RGBA components")},
        {"r16g16b16a16_uint",    NGLI_FORMAT_R16G16B16A16_UINT,   .desc=NGLI_DOCSTRING("16-bit unsigned integer RGBA components")},
        {"r16g16b16a16_sint",    NGLI_FORMAT_R16G16B16A16_SINT,   .desc=NGLI_DOCSTRING("16-bit signed integer RGBA components")},
        {"r16g16b16a16_sfloat",  NGLI_FORMAT_R16G16B16A16_SFLOAT, .desc=NGLI_DOCSTRING("16-bit signed float RGBA components")},
        {"r32_uint",             NGLI_FORMAT_R32_UINT,            .desc=NGLI_DOCSTRING("32-bit unsigned integer R component")},
        {"r32_sint",             NGLI_FORMAT_R32_SINT,            .desc=NGLI_DOCSTRING("32-bit signed integer R component")},
        {"r32_sfloat",           NGLI_FORMAT_R32_SFLOAT,          .desc=NGLI_DOCSTRING("32-bit signed float R component")},
        {"r32g32_uint",          NGLI_FORMAT_R32G32_UINT,         .desc=NGLI_DOCSTRING("32-bit unsigned integer RG components")},
        {"r32g32_sint",          NGLI_FORMAT_R32G32_SINT,         .desc=NGLI_DOCSTRING("32-bit signed integer RG components")},
        {"r32g32_sfloat",        NGLI_FORMAT_R32G32_SFLOAT,       .desc=NGLI_DOCSTRING("32-bit signed float RG components")},
        {"r32g32b32a32_uint",    NGLI_FORMAT_R32G32B32A32_UINT,   .desc=NGLI_DOCSTRING("32-bit unsigned integer RGBA components")},
        {"r32g32b32a32_sint",    NGLI_FORMAT_R32G32B32A32_SINT,   .desc=NGLI_DOCSTRING("32-bit signed integer RGBA components")},
        {"r32g32b32a32_sfloat",  NGLI_FORMAT_R32G32B32A32_SFLOAT, .desc=NGLI_DOCSTRING("32-bit signed float RGBA components")},
        {"d16_unorm",            NGLI_FORMAT_D16_UNORM,           .desc=NGLI_DOCSTRING("16-bit unsigned normalized depth component")},
        {"d24_unorm",            NGLI_FORMAT_X8_D24_UNORM_PACK32, .desc=NGLI_DOCSTRING("32-bit packed format that has 24-bit unsigned "
                                                                                       "normalized depth component + 8-bit of unused data")},
        {"d32_sfloat",           NGLI_FORMAT_D32_SFLOAT,          .desc=NGLI_DOCSTRING("32-bit signed float depth component")},
        {"d24_unorm_s8_uint",    NGLI_FORMAT_D24_UNORM_S8_UINT,   .desc=NGLI_DOCSTRING("32-bit packed format that has 24-bit unsigned "
                                                                                       "normalized depth component + 8-bit unsigned "
                                                                                       "integer stencil component")},
        {"d32_sfloat_s8_uint",   NGLI_FORMAT_D32_SFLOAT_S8_UINT,  .desc=NGLI_DOCSTRING("64-bit packed format that has 32-bit signed "
                                                                                       "float depth component + 8-bit unsigned integer "
                                                                                       "stencil component + 24-bit of unused data")},
        {"s8_uint",              NGLI_FORMAT_S8_UINT,             .desc=NGLI_DOCSTRING("8-bit unsigned integer stencil component")},
        {"auto_depth",           NGLI_FORMAT_AUTO_DEPTH,          .desc=NGLI_DOCSTRING("select automatically the preferred depth format")},
        {"auto_depth_stencil",   NGLI_FORMAT_AUTO_DEPTH_STENCIL,  .desc=NGLI_DOCSTRING("select automatically the preferred depth + stencil format")},
        {NULL}
    }
};

#define BUFFER_NODES                \
    NGL_NODE_ANIMATEDBUFFERFLOAT,   \
    NGL_NODE_ANIMATEDBUFFERVEC2,    \
    NGL_NODE_ANIMATEDBUFFERVEC4,    \
    NGL_NODE_BUFFERBYTE,            \
    NGL_NODE_BUFFERBVEC2,           \
    NGL_NODE_BUFFERBVEC4,           \
    NGL_NODE_BUFFERINT,             \
    NGL_NODE_BUFFERIVEC2,           \
    NGL_NODE_BUFFERIVEC4,           \
    NGL_NODE_BUFFERSHORT,           \
    NGL_NODE_BUFFERSVEC2,           \
    NGL_NODE_BUFFERSVEC4,           \
    NGL_NODE_BUFFERUBYTE,           \
    NGL_NODE_BUFFERUBVEC2,          \
    NGL_NODE_BUFFERUBVEC4,          \
    NGL_NODE_BUFFERUINT,            \
    NGL_NODE_BUFFERUIVEC2,          \
    NGL_NODE_BUFFERUIVEC4,          \
    NGL_NODE_BUFFERUSHORT,          \
    NGL_NODE_BUFFERUSVEC2,          \
    NGL_NODE_BUFFERUSVEC4,          \
    NGL_NODE_BUFFERFLOAT,           \
    NGL_NODE_BUFFERVEC2,            \
    NGL_NODE_BUFFERVEC4,            \


#define DATA_SRC_TYPES_LIST_2D (const int[]){NGL_NODE_MEDIA,                   \
                                             BUFFER_NODES                      \
                                             -1}

#define DATA_SRC_TYPES_LIST_3D (const int[]){BUFFER_NODES                      \
                                             -1}


#define OFFSET(x) offsetof(struct texture_priv, x)
static const struct node_param texture2d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(params.height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i64=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_2D,
                 .desc=NGLI_DOCSTRING("data source")},
    {"direct_rendering", PARAM_TYPE_BOOL, OFFSET(direct_rendering), {.i64=1},
                         .desc=NGLI_DOCSTRING("whether direct rendering is allowed or not for media playback")},
    {NULL}
};

static const struct node_param texture3d_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"width", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
              .desc=NGLI_DOCSTRING("width of the texture")},
    {"height", PARAM_TYPE_INT, OFFSET(params.height), {.i64=0},
               .desc=NGLI_DOCSTRING("height of the texture")},
    {"depth", PARAM_TYPE_INT, OFFSET(params.depth), {.i64=0},
               .desc=NGLI_DOCSTRING("depth of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i64=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static const struct node_param texturecube_params[] = {
    {"format", PARAM_TYPE_SELECT, OFFSET(format), {.i64=NGLI_FORMAT_R8G8B8A8_UNORM}, .choices=&format_choices,
               .desc=NGLI_DOCSTRING("format of the pixel data")},
    {"size", PARAM_TYPE_INT, OFFSET(params.width), {.i64=0},
             .desc=NGLI_DOCSTRING("width and height of the texture")},
    {"min_filter", PARAM_TYPE_SELECT, OFFSET(params.min_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture minifying function")},
    {"mag_filter", PARAM_TYPE_SELECT, OFFSET(params.mag_filter), {.i64=NGLI_FILTER_NEAREST}, .choices=&ngli_filter_choices,
                   .desc=NGLI_DOCSTRING("texture magnification function")},
    {"mipmap_filter", PARAM_TYPE_SELECT, OFFSET(params.mipmap_filter), {.i64=NGLI_MIPMAP_FILTER_NONE},
                      .choices=&ngli_mipmap_filter_choices,
                      .desc=NGLI_DOCSTRING("texture minifying mipmap function")},
    {"wrap_s", PARAM_TYPE_SELECT, OFFSET(params.wrap_s), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the s dimension (horizontal)")},
    {"wrap_t", PARAM_TYPE_SELECT, OFFSET(params.wrap_t), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the t dimension (vertical)")},
    {"wrap_r", PARAM_TYPE_SELECT, OFFSET(params.wrap_r), {.i64=NGLI_WRAP_CLAMP_TO_EDGE}, .choices=&wrap_choices,
               .desc=NGLI_DOCSTRING("wrap parameter for the texture on the r dimension (depth)")},
    {"data_src", PARAM_TYPE_NODE, OFFSET(data_src), .node_types=DATA_SRC_TYPES_LIST_3D,
                 .desc=NGLI_DOCSTRING("data source")},
    {NULL}
};

static int texture_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct texture_priv *s = node->priv_data;
    struct texture_params *params = &s->params;

    if (params->type == NGLI_TEXTURE_TYPE_CUBE)
        params->height = params->width;

    if (gctx->features & NGLI_FEATURE_TEXTURE_STORAGE)
        params->immutable = 1;

    const uint8_t *data = NULL;

    if (s->data_src) {
        switch (s->data_src->class->id) {
        case NGL_NODE_MEDIA:
            return 0;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
        case NGL_NODE_BUFFERBYTE:
        case NGL_NODE_BUFFERBVEC2:
        case NGL_NODE_BUFFERBVEC4:
        case NGL_NODE_BUFFERINT:
        case NGL_NODE_BUFFERIVEC2:
        case NGL_NODE_BUFFERIVEC4:
        case NGL_NODE_BUFFERSHORT:
        case NGL_NODE_BUFFERSVEC2:
        case NGL_NODE_BUFFERSVEC4:
        case NGL_NODE_BUFFERUBYTE:
        case NGL_NODE_BUFFERUBVEC2:
        case NGL_NODE_BUFFERUBVEC4:
        case NGL_NODE_BUFFERUINT:
        case NGL_NODE_BUFFERUIVEC2:
        case NGL_NODE_BUFFERUIVEC4:
        case NGL_NODE_BUFFERUSHORT:
        case NGL_NODE_BUFFERUSVEC2:
        case NGL_NODE_BUFFERUSVEC4:
        case NGL_NODE_BUFFERFLOAT:
        case NGL_NODE_BUFFERVEC2:
        case NGL_NODE_BUFFERVEC4: {
            struct buffer_priv *buffer = s->data_src->priv_data;

            if (params->type == NGLI_TEXTURE_TYPE_2D) {
                if (buffer->count != params->width * params->height) {
                    LOG(ERROR, "dimensions (%dx%d) do not match buffer count (%d),"
                        " assuming %dx1", params->width, params->height,
                        buffer->count, buffer->count);
                    params->width = buffer->count;
                    params->height = 1;
                }
            } else if (params->type == NGLI_TEXTURE_TYPE_3D) {
                if (buffer->count != params->width * params->height * params->depth) {
                    LOG(ERROR, "dimensions (%dx%dx%d) do not match buffer count (%d),"
                        " assuming %dx1x1", params->width, params->height, params->depth,
                        buffer->count, buffer->count);
                    params->width = buffer->count;
                    params->height = params->depth = 1;
                }
            }
            data = buffer->data;
            params->format = buffer->data_format;
            break;
        }
        default:
            ngli_assert(0);
        }
    }

    s->texture = ngli_texture_create(gctx);
    if (!s->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngli_texture_init(s->texture, params);
    if (ret < 0)
        return ret;

    ret = ngli_texture_upload(s->texture, data, 0);
    if (ret < 0)
        return ret;

    struct image_params image_params = {
        .width = params->width,
        .height = params->height,
        .depth = params->depth,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
    };
    ngli_image_init(&s->image, &image_params, &s->texture);

    return 0;
}

static void handle_media_frame(struct ngl_node *node)
{
    int ret = ngli_hwupload_upload_frame(node);
    if (ret < 0)
        LOG(ERROR, "could not map media frame");
}

static void handle_buffer_frame(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct buffer_priv *buffer = s->data_src->priv_data;
    const uint8_t *data = buffer->data;

    ngli_texture_upload(s->texture, data, 0);
}

static int texture_update(struct ngl_node *node, double t)
{
    struct texture_priv *s = node->priv_data;

    if (!s->data_src)
        return 0;

    int ret = ngli_node_update(s->data_src, t);
    if (ret < 0)
        return ret;

    switch (s->data_src->class->id) {
        case NGL_NODE_MEDIA:
            handle_media_frame(node);
            break;
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
            ret = ngli_node_update(s->data_src, t);
            if (ret < 0)
                return ret;
            handle_buffer_frame(node);
            break;
    }

    return 0;
}

static void texture_release(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;

    ngli_hwupload_uninit(node);
    ngli_texture_freep(&s->texture);
    ngli_image_reset(&s->image);
}

static int get_preferred_format(struct gctx *gctx, int format)
{
    switch (format) {
    case NGLI_FORMAT_AUTO_DEPTH:
        return ngli_gctx_get_preferred_depth_format(gctx);
    case NGLI_FORMAT_AUTO_DEPTH_STENCIL:
        return ngli_gctx_get_preferred_depth_stencil_format(gctx);
    default:
        return format;
    }
}

static int texture2d_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct texture_priv *s = node->priv_data;
    s->params.type = NGLI_TEXTURE_TYPE_2D;
    s->params.format = get_preferred_format(gctx, s->format);
    s->supported_image_layouts = s->direct_rendering ? -1 : (1 << NGLI_IMAGE_LAYOUT_DEFAULT);
    return 0;
}

static int texture3d_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;

    if (!(gctx->features & NGLI_FEATURE_TEXTURE_3D)) {
        LOG(ERROR, "context does not support 3D textures");
        return NGL_ERROR_UNSUPPORTED;
    }

    struct texture_priv *s = node->priv_data;
    s->params.type = NGLI_TEXTURE_TYPE_3D;
    s->params.format = get_preferred_format(gctx, s->format);

    return 0;
}

static int texturecube_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;

    if (!(gctx->features & NGLI_FEATURE_TEXTURE_CUBE_MAP)) {
        LOG(ERROR, "context does not support cube map textures");
        return NGL_ERROR_UNSUPPORTED;
    }

    struct texture_priv *s = node->priv_data;
    s->params.type = NGLI_TEXTURE_TYPE_CUBE;
    s->params.format = get_preferred_format(gctx, s->format);

    return 0;
}

const struct node_class ngli_texture2d_class = {
    .id        = NGL_NODE_TEXTURE2D,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture2D",
    .init      = texture2d_init,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texture2d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texture3d_class = {
    .id        = NGL_NODE_TEXTURE3D,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "Texture3D",
    .init      = texture3d_init,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texture3d_params,
    .file      = __FILE__,
};

const struct node_class ngli_texturecube_class = {
    .id        = NGL_NODE_TEXTURECUBE,
    .category  = NGLI_NODE_CATEGORY_TEXTURE,
    .name      = "TextureCube",
    .init      = texturecube_init,
    .prefetch  = texture_prefetch,
    .update    = texture_update,
    .release   = texture_release,
    .priv_size = sizeof(struct texture_priv),
    .params    = texturecube_params,
    .file      = __FILE__,
};
