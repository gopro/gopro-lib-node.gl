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

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLIOSurface.h>

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

struct hwupload_vt_darwin {
    struct sxplayer_frame *frame;
    struct texture *planes[2];
};

static int vt_darwin_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)ctx->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = hwupload->hwmap_priv_data;

    sxplayer_release_frame(vt->frame);
    vt->frame = frame;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return -1;
    }

    OSType format = IOSurfaceGetPixelFormat(surface);
    if (format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
        LOG(ERROR, "unsupported IOSurface format: 0x%x", format);
        return -1;
    }

    for (int i = 0; i < 2; i++) {
        struct texture *plane = vt->planes[i];
        struct texture_gl *plane_gl = (struct texture_gl *)plane;

        ngli_glBindTexture(gl, plane_gl->target, plane_gl->id);

        int width = IOSurfaceGetWidthOfPlane(surface, i);
        int height = IOSurfaceGetHeightOfPlane(surface, i);
        ngli_texture_gl_set_dimensions(plane, width, height, 0);

        CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(), plane_gl->target,
                                              plane_gl->internal_format, width, height,
                                              plane_gl->format, plane_gl->format_type, surface, i);
        if (err != kCGLNoError) {
            LOG(ERROR, "could not bind IOSurface plane %d to texture %d: %d", i, plane_gl->id, err);
            return -1;
        }

        ngli_glBindTexture(gl, GL_TEXTURE_RECTANGLE, 0);
    }

    return 0;
}

static int support_direct_rendering(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    int direct_rendering = s->supported_image_layouts & (1 << NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);

    if (direct_rendering && s->params.mipmap_filter) {
        LOG(WARNING, "IOSurface NV12 buffers do not support mipmapping: "
            "disabling direct rendering");
        direct_rendering = 0;
    }

    return direct_rendering;
}

static int vt_darwin_init(struct ngl_node *node, struct sxplayer_frame * frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = hwupload->hwmap_priv_data;

    for (int i = 0; i < 2; i++) {
        struct texture_params plane_params = {
            .type             = NGLI_TEXTURE_TYPE_2D,
            .format           = i == 0 ? NGLI_FORMAT_R8_UNORM : NGLI_FORMAT_R8G8_UNORM,
            .usage            = NGLI_TEXTURE_USAGE_SAMPLED_BIT,
            .rectangle        = 1,
            .external_storage = 1,
        };

        vt->planes[i] = ngli_texture_create(gctx);
        if (!vt->planes[i])
            return NGL_ERROR_MEMORY;

        int ret = ngli_texture_init(vt->planes[i], &plane_params);
        if (ret < 0)
            return ret;
    }

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_NV12_RECTANGLE,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwupload->mapped_image, &image_params, vt->planes);

    hwupload->require_hwconv = !support_direct_rendering(node);

    return 0;
}

static void vt_darwin_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = hwupload->hwmap_priv_data;

    for (int i = 0; i < 2; i++)
        ngli_texture_freep(&vt->planes[i]);

    sxplayer_release_frame(vt->frame);
    vt->frame = NULL;
}

const struct hwmap_class ngli_hwmap_vt_darwin_gl_class = {
    .name      = "videotoolbox (iosurface → nv12)",
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwupload_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};
