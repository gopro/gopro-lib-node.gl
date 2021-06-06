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
#include "nodegl.h"
#include "nodes.h"

struct hwupload_common {
    int width;
    int height;
    struct texture *planes[1];
};

static int common_get_data_format(int pix_fmt)
{
    switch (pix_fmt) {
    case SXPLAYER_PIXFMT_RGBA:
        return NGLI_FORMAT_R8G8B8A8_UNORM;
    case SXPLAYER_PIXFMT_BGRA:
        return NGLI_FORMAT_B8G8R8A8_UNORM;
    case SXPLAYER_SMPFMT_FLT:
        return NGLI_FORMAT_R32_SFLOAT;
    default:
        return -1;
    }
}

static int common_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_common *common = hwupload->hwmap_priv_data;

    common->width = frame->width;
    common->height = frame->height;

    struct texture_params params = s->params;
    params.width  = frame->width;
    params.height = frame->height;

    params.format = common_get_data_format(frame->pix_fmt);
    if (params.format < 0)
        return -1;

    common->planes[0] = ngli_texture_create(gctx);
    if (!common->planes[0])
        return NGL_ERROR_MEMORY;

    int ret = ngli_texture_init(common->planes[0], &params);
    if (ret < 0)
        return ret;

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwupload->mapped_image, &image_params, common->planes);

    hwupload->require_hwconv = 0;

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

    if (common->width != frame->width || common->height != frame->height) {
        common_uninit(node);

        int ret = common_init(node, frame);
        if (ret < 0)
            return ret;
    }

    const int linesize = frame->linesize >> 2;
    return ngli_texture_upload(common->planes[0], frame->data, linesize);
}

const struct hwmap_class ngli_hwmap_common_class = {
    .name      = "default",
    .priv_size = sizeof(struct hwupload_common),
    .init      = common_init,
    .map_frame = common_map_frame,
    .uninit    = common_uninit,
};
