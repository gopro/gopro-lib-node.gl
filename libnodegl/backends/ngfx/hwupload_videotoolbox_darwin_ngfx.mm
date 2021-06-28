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

// TODO: consolidate duplicate code with hwupload_videotoolbox_darwin_vk.m

#include <stddef.h>
#include <stdio.h>
#include <string.h>
extern "C" {
    #include <sxplayer.h>
}
#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurface.h>
#include <Metal/Metal.h>
extern "C" {
#include "format.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "type.h"
}
#include "gpu_ctx_ngfx.h"
#include "texture_ngfx.h"
#include "ngfx/porting/metal/MTLTexture.h"
#include "ngfx/porting/metal/MTLGraphicsContext.h"

struct hwupload_vt_darwin {
    struct sxplayer_frame *frame;
    struct texture *planes[2];
    id<MTLDevice> device;
    CVMetalTextureCacheRef texture_cache;
};

static int vt_darwin_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct texture_priv *s = (struct texture_priv *)node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = (struct hwupload_vt_darwin *)hwupload->hwmap_priv_data;

    sxplayer_release_frame(vt->frame);
    vt->frame = frame;

    CVPixelBufferRef cvpixbuf = (CVPixelBufferRef)frame->data;
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(cvpixbuf);
    if (!surface) {
        LOG(ERROR, "could not get IOSurface from buffer");
        return NGL_ERROR_EXTERNAL;
    }

    OSType format = IOSurfaceGetPixelFormat(surface);
    if (format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
        LOG(ERROR, "unsupported IOSurface format: 0x%x", format);
        return NGL_ERROR_EXTERNAL;
    }

    for (int i = 0; i < 2; i++) {
        struct texture *plane = vt->planes[i];
        struct texture_ngfx *plane_ngfx = (struct texture_ngfx *)plane;

        const size_t width = CVPixelBufferGetWidthOfPlane(cvpixbuf, i);
        const size_t height = CVPixelBufferGetHeightOfPlane(cvpixbuf, i);
        const MTLPixelFormat pixel_format = i == 0 ? MTLPixelFormatR8Unorm : MTLPixelFormatRG8Unorm;

        CVMetalTextureRef texture_ref = NULL;
        CVReturn status = CVMetalTextureCacheCreateTextureFromImage(NULL, vt->texture_cache, cvpixbuf, NULL, pixel_format, width, height, i, &texture_ref);
        if (status != kCVReturnSuccess) {
            LOG(ERROR, "unable to create texture from image on plane %d", i);
            return NGL_ERROR_EXTERNAL;
        }

        id<MTLTexture> mtl_texture = CVMetalTextureGetTexture(texture_ref);

        ngfx::MTLTexture *texture_ngfx = (ngfx::MTLTexture *)plane_ngfx->v;
        texture_ngfx->v = mtl_texture;

        //TODO: call CFRelease(texture_ref) when texture is rendered
    }
    return 0;
}

static int support_direct_rendering(struct ngl_node *node)
{
    struct texture_priv *s = (struct texture_priv *)node->priv_data;
    int direct_rendering = s->supported_image_layouts & (1 << NGLI_IMAGE_LAYOUT_NV12);

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
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gpu_ctx_ngfx *gpu_ctx_ngfx = (struct gpu_ctx_ngfx *)gpu_ctx;
    struct texture_priv *s = (struct texture_priv *)node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = (struct hwupload_vt_darwin *)hwupload->hwmap_priv_data;
    ngfx::MTLGraphicsContext *mtl_gfx_ctx = (ngfx::MTLGraphicsContext *)gpu_ctx_ngfx->graphics_context;
    vt->device = mtl_gfx_ctx->mtlDevice.v;
    CVMetalTextureCacheCreate(NULL, NULL, vt->device, NULL, &vt->texture_cache);

    //struct texture_params plane_params = s->params;
    //struct texture_params plane_params = NGLI_TEXTURE_PARAM_DEFAULTS;

    for (int i = 0; i < 2; i++) {
        struct texture_ngfx *plane = (struct texture_ngfx *)ngli_texture_create(gpu_ctx);
        ngfx::MTLTexture *mtl_texture = new ngfx::MTLTexture();
        MTLSamplerDescriptor *mtl_sampler_desc = [MTLSamplerDescriptor new];
        mtl_texture->mtlSamplerState = [mtl_gfx_ctx->mtlDevice.v newSamplerStateWithDescriptor:mtl_sampler_desc];
        plane->v = mtl_texture;
        vt->planes[i] = (struct texture *)plane;
        if (!vt->planes[i])
            return NGL_ERROR_MEMORY;

        //int ret = ngli_texture_init(vt->planes[i], &plane_params);
        //if (ret < 0)
        //    return ret;
    }

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_NV12,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwupload->mapped_image, &image_params, vt->planes);

    hwupload->require_hwconv = !support_direct_rendering(node);
    return 0;
}

static void vt_darwin_uninit(struct ngl_node *node)
{
    struct texture_priv *s = (struct texture_priv *)node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = (struct hwupload_vt_darwin *)hwupload->hwmap_priv_data;

    for (int i = 0; i < 2; i++)
        ngli_texture_freep(&vt->planes[i]);

    sxplayer_release_frame(vt->frame);
    vt->frame = NULL;
}

extern "C" const struct hwmap_class ngli_hwmap_vt_darwin_ngfx_class = {
    .name      = "videotoolbox (iosurface → nv12)",
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwupload_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};
