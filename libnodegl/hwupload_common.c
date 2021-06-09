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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include "format.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"

struct hwupload_common {
    int width;
    int height;
    int nb_planes;
    struct texture *planes[4];
};

static const struct format_desc {
    int layout;
    int depth;
    int shift;
    int nb_planes;
    int log2_chroma_width;
    int log2_chroma_height;
    int format_depth;
    int formats[4];
} format_descs[] = {
    [SXPLAYER_PIXFMT_RGBA] = {
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .depth = 8,
        .nb_planes = 1,
        .format_depth = 8,
        .formats[0] = NGLI_FORMAT_R8G8B8A8_UNORM,
    },
    [SXPLAYER_PIXFMT_BGRA] = {
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .depth = 8,
        .nb_planes = 1,
        .format_depth = 8,
        .formats[0] = NGLI_FORMAT_B8G8R8A8_UNORM,
    },
    [SXPLAYER_SMPFMT_FLT] = {
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .nb_planes = 1,
        .formats[0] = NGLI_FORMAT_R32_SFLOAT,
    },
    [SXPLAYER_PIXFMT_NV12] = {
        .layout = NGLI_IMAGE_LAYOUT_NV12,
        .depth = 8,
        .nb_planes = 2,
        .log2_chroma_width = 1,
        .log2_chroma_height = 1,
        .format_depth = 8,
        .formats[0] = NGLI_FORMAT_R8_UNORM,
        .formats[1] = NGLI_FORMAT_R8G8_UNORM,
    },
    [SXPLAYER_PIXFMT_YUV420P] = {
        .layout = NGLI_IMAGE_LAYOUT_YUV,
        .depth = 8,
        .nb_planes = 3,
        .log2_chroma_width = 1,
        .log2_chroma_height = 1,
        .format_depth = 8,
        .formats[0] = NGLI_FORMAT_R8_UNORM,
        .formats[1] = NGLI_FORMAT_R8_UNORM,
        .formats[2] = NGLI_FORMAT_R8_UNORM,
    },
    [SXPLAYER_PIXFMT_YUV422P] = {
        .layout = NGLI_IMAGE_LAYOUT_YUV,
        .depth = 8,
        .nb_planes = 3,
        .log2_chroma_width = 1,
        .log2_chroma_height = 0,
        .format_depth = 8,
        .formats[0] = NGLI_FORMAT_R8_UNORM,
        .formats[1] = NGLI_FORMAT_R8_UNORM,
        .formats[2] = NGLI_FORMAT_R8_UNORM,
    },
    [SXPLAYER_PIXFMT_YUV444P] = {
        .layout = NGLI_IMAGE_LAYOUT_YUV,
        .depth = 8,
        .nb_planes = 3,
        .log2_chroma_width = 0,
        .log2_chroma_height = 0,
        .format_depth = 8,
        .formats[0] = NGLI_FORMAT_R8_UNORM,
        .formats[1] = NGLI_FORMAT_R8_UNORM,
        .formats[2] = NGLI_FORMAT_R8_UNORM,
    },
};

static const struct format_desc *common_get_format_desc(int pix_fmt)
{
    if (pix_fmt < 0 || pix_fmt >= NGLI_ARRAY_NB(format_descs))
        return NULL;

    const struct format_desc *desc = &format_descs[pix_fmt];
    if (!desc->layout)
        return NULL;

    return desc;
}

static int support_direct_rendering(struct ngl_node *node, const struct format_desc *desc)
{
    struct texture_priv *s = node->priv_data;

    int direct_rendering = 1;
    if (desc->layout != NGLI_IMAGE_LAYOUT_DEFAULT) {
        direct_rendering = (s->supported_image_layouts & (1 << desc->layout));
        if (s->params.mipmap_filter)
            direct_rendering = 0;
    }

    return direct_rendering;
}

static int common_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_common *common = hwupload->hwmap_priv_data;

    const struct format_desc *desc = common_get_format_desc(frame->pix_fmt);
    if (!desc)
        return NGL_ERROR_UNSUPPORTED;

    common->width = frame->width;
    common->height = frame->height;
    common->nb_planes = desc->nb_planes;

    for (int i = 0; i < common->nb_planes; i++) {
        struct texture_params params = s->params;
        params.width  = i == 0 ? frame->width : NGLI_CEIL_RSHIFT(frame->width, desc->log2_chroma_width);
        params.height = i == 0 ? frame->height : NGLI_CEIL_RSHIFT(frame->height, desc->log2_chroma_height);
        params.format = desc->formats[i];

        common->planes[i] = ngli_texture_create(gctx);
        if (!common->planes[i])
            return NGL_ERROR_MEMORY;

        int ret = ngli_texture_init(common->planes[i], &params);
        if (ret < 0)
            return ret;
    }

    const int src_max = ((1 << desc->depth) - 1) << desc->shift;
    const int dst_max = (1 << desc->format_depth) - 1;
    const float color_scale = (float)(dst_max)/(src_max);

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = desc->layout,
        .color_scale = color_scale,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwupload->mapped_image, &image_params, common->planes);

    hwupload->require_hwconv = !support_direct_rendering(node, desc);

    return 0;
}

static void common_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_common *common = hwupload->hwmap_priv_data;

    for (int i = 0; i < NGLI_ARRAY_NB(common->planes); i++)
        ngli_texture_freep(&common->planes[i]);
}

static int common_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_common *common = hwupload->hwmap_priv_data;

    for (int i = 0; i < common->nb_planes; i++) {
        struct texture *plane = common->planes[i];
        struct texture_params *params = &plane->params;
        const int linesize = frame->linesizep[i] / ngli_format_get_bytes_per_pixel(params->format);
        int ret = ngli_texture_upload(plane, frame->datap[i], linesize);
        if (ret < 0)
            return ret;
    }

    return 0;
}

const struct hwmap_class ngli_hwmap_common_class = {
    .name      = "default",
    .priv_size = sizeof(struct hwupload_common),
    .init      = common_init,
    .map_frame = common_map_frame,
    .uninit    = common_uninit,
};
