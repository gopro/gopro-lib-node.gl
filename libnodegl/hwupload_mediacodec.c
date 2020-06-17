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

#include <libavcodec/mediacodec.h>

#include "android_surface.h"
#include "format.h"
#include "gctx_gl.h"
#include "glincludes.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "texture_gl.h"

static int support_direct_rendering(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    int direct_rendering = s->supported_image_layouts & (1 << NGLI_IMAGE_LAYOUT_MEDIACODEC);

    if (direct_rendering) {
        const struct texture_params *params = &s->params;

        if (params->mipmap_filter) {
            LOG(WARNING, "external textures do not support mipmapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        } else if (params->wrap_s != NGLI_WRAP_CLAMP_TO_EDGE || params->wrap_t != NGLI_WRAP_CLAMP_TO_EDGE) {
            LOG(WARNING, "external textures only support clamp to edge wrapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        }
    }

    return direct_rendering;
}

static int mc_init(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)ctx->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct texture_priv *s = node->priv_data;
    const struct texture_params *params = &s->params;
    struct media_priv *media = s->data_src->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct texture_gl *android_texture_gl = (struct texture_gl *)media->android_texture;

    GLint id = android_texture_gl->id;
    GLenum target = android_texture_gl->target;
    const GLint min_filter = ngli_texture_get_gl_min_filter(params->min_filter, params->mipmap_filter);
    const GLint mag_filter = ngli_texture_get_gl_mag_filter(params->mag_filter);

    ngli_glBindTexture(gl, target, id);
    ngli_glTexParameteri(gl, target, GL_TEXTURE_MIN_FILTER, min_filter);
    ngli_glTexParameteri(gl, target, GL_TEXTURE_MAG_FILTER, mag_filter);
    ngli_glBindTexture(gl, target, 0);

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_MEDIACODEC,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwupload->mapped_image, &image_params, &media->android_texture);

    hwupload->require_hwconv = !support_direct_rendering(node);

    return 0;
}

static int mc_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct media_priv *media = s->data_src->priv_data;
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;

    NGLI_ALIGNED_MAT(flip_matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };

    float *matrix = hwupload->mapped_image.coordinates_matrix;
    ngli_android_surface_render_buffer(media->android_surface, buffer, matrix);
    ngli_mat4_mul(matrix, matrix, flip_matrix);

    ngli_texture_gl_set_dimensions(media->android_texture, frame->width, frame->height, 0);

    return 0;
}

const struct hwmap_class ngli_hwmap_mc_class = {
    .name      = "mediacodec (oes zero-copy)",
    .init      = mc_init,
    .map_frame = mc_map_frame,
};
