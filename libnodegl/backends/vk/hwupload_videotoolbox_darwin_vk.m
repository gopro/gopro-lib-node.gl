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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sxplayer.h>

#include <vulkan/vulkan.h>

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurface.h>
#include <Metal/Metal.h>

#include <MoltenVK/vk_mvk_moltenvk.h>

#include "format.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "gctx_vk.h"
#include "texture_vk.h"
#include "type.h"

struct hwupload_vt_darwin {
    struct sxplayer_frame *frame;
    struct texture *planes[2];
    id<MTLDevice> device;
    CVMetalTextureCacheRef texture_cache;
};

static int vt_darwin_map_frame(struct ngl_node *node, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = hwupload->hwmap_priv_data;

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
        return NGL_ERROR_UNSUPPORTED;
    }

    for (int i = 0; i < 2; i++) {
        struct texture *plane = vt->planes[i];
        struct texture_vk *plane_vk = (struct texture_vk *)plane;

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

        const VkImageUsageFlagBits usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                         //| VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                         | VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateInfo image_create_info = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .extent        = {frame->width, frame->height, 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .format        = i == 0 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8_UNORM,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage         = usage,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
            //.flags         = flags,
        };

        VkResult vkret = vkCreateImage(vk->device, &image_create_info, NULL, &plane_vk->image);
        if (vkret != VK_SUCCESS)
            return NGL_ERROR_EXTERNAL;

        struct texture_params plane_params = s->params;
        // XXX: format should not be required with a wrap since we do declare them ourselves?
        plane_params.format = i == 0 ? NGLI_FORMAT_R8_UNORM : NGLI_FORMAT_R8G8_UNORM;
        //plane_params.usage |= NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY; // XXX: workaround to skip buffer create in texture wrap
        plane_params.type = NGLI_TEXTURE_TYPE_2D;
        plane_params.external_storage = 1;

        int ret = ngli_texture_vk_wrap(vt->planes[i], &plane_params, plane_vk->image, VK_IMAGE_LAYOUT_GENERAL);
        if (ret < 0)
            return ret;

        vkret = vkSetMTLTextureMVK(plane_vk->image, mtl_texture);
        if (vkret != VK_SUCCESS) {
            LOG(ERROR, "error set mtl texture");
            return NGL_ERROR_EXTERNAL;
        }

        // FIXME: release in error codepath
        CFRelease(texture_ref);
    }

    return 0;
}

static int support_direct_rendering(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
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
    struct gctx *gctx = ctx->gctx;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)gctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct hwupload_vt_darwin *vt = hwupload->hwmap_priv_data;

    vkGetMTLDeviceMVK(gctx_vk->vkcontext->phy_device, &vt->device);

    CVMetalTextureCacheCreate(NULL, NULL, vt->device, NULL, &vt->texture_cache);

    //struct texture_params plane_params = s->params;
    //struct texture_params plane_params = NGLI_TEXTURE_PARAM_DEFAULTS;

    for (int i = 0; i < 2; i++) {

        vt->planes[i] = ngli_texture_create(gctx);
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

const struct hwmap_class ngli_hwmap_vt_darwin_vk_class = {
    .name      = "videotoolbox (iosurface → nv12)",
    .flags     = HWMAP_FLAG_FRAME_OWNER,
    .priv_size = sizeof(struct hwupload_vt_darwin),
    .init      = vt_darwin_init,
    .map_frame = vt_darwin_map_frame,
    .uninit    = vt_darwin_uninit,
};
