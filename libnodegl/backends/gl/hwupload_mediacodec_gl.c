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
#include <android/hardware_buffer_jni.h>

#include "android_imagereader.h"
#include "android_surface.h"
#include "egl.h"
#include "format.h"
#include "gpu_ctx_gl.h"
#include "glincludes.h"
#include "hwupload.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "texture_gl.h"

struct hwupload_mc {
    struct android_image *android_image;
    EGLImageKHR egl_image;
    struct texture *texture;
};

static int support_direct_rendering(struct hwupload *hwupload)
{
    struct hwupload_params *params = &hwupload->params;

    int direct_rendering = params->output_image_layouts & (1 << NGLI_IMAGE_LAYOUT_MEDIACODEC);

    if (direct_rendering) {
        if (params->output_texture_params.mipmap_filter) {
            LOG(WARNING, "external textures do not support mipmapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        } else if (params->output_texture_params.wrap_s != NGLI_WRAP_CLAMP_TO_EDGE || params->output_texture_params.wrap_t != NGLI_WRAP_CLAMP_TO_EDGE) {
            LOG(WARNING, "external textures only support clamp to edge wrapping: "
                "disabling direct rendering");
            direct_rendering = 0;
        }
    }

    return direct_rendering;
}

static int mc_init(struct hwupload *hwupload, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = hwupload->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    struct gpu_ctx *gpu_ctx = (struct gpu_ctx *)hwupload->gpu_ctx;
    struct hwupload_params *params = &hwupload->params;
    struct hwupload_mc *mc = hwupload->hwmap_priv_data;

    struct texture_params texture_params = {
        .type         = NGLI_TEXTURE_TYPE_2D,
        .format       = NGLI_FORMAT_UNDEFINED,
        .min_filter   = params->output_texture_params.min_filter,
        .mag_filter   = params->output_texture_params.mag_filter,
        .wrap_s       = NGLI_WRAP_CLAMP_TO_EDGE,
        .wrap_t       = NGLI_WRAP_CLAMP_TO_EDGE,
        .wrap_r       = NGLI_WRAP_CLAMP_TO_EDGE,
        .usage        = NGLI_TEXTURE_USAGE_SAMPLED_BIT,
        .external_oes = 1,
    };

    mc->texture = ngli_texture_create(gpu_ctx);
    if (!mc->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngli_texture_init(mc->texture, &texture_params);
    if (ret < 0)
        return ret;

    struct image_params image_params = {
        .width = frame->width,
        .height = frame->height,
        .layout = NGLI_IMAGE_LAYOUT_MEDIACODEC,
        .color_scale = 1.f,
        .color_info = ngli_color_info_from_sxplayer_frame(frame),
    };
    ngli_image_init(&hwupload->mapped_image, &image_params, &mc->texture);

    hwupload->require_hwconv = !support_direct_rendering(hwupload);

    if (!android_ctx->has_native_imagereader_api) {
        struct texture_gl *texture_gl = (struct texture_gl *)mc->texture;
        ret = ngli_android_surface_attach_to_gl_context(params->android_surface, texture_gl->id);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int mc_map_frame_surfacetexture(struct hwupload *hwupload, struct sxplayer_frame *frame)
{
    struct hwupload_params *params = &hwupload->params;
    struct hwupload_mc *mc = hwupload->hwmap_priv_data;
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;

    NGLI_ALIGNED_MAT(flip_matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };

    float *matrix = hwupload->mapped_image.coordinates_matrix;
    ngli_android_surface_render_buffer(params->android_surface, buffer, matrix);
    ngli_mat4_mul(matrix, matrix, flip_matrix);

    ngli_texture_gl_set_dimensions(mc->texture, frame->width, frame->height, 0);

    return 0;
}

static int mc_map_frame_imagereader(struct hwupload *hwupload, struct sxplayer_frame *frame)
{
    struct hwupload_params *params = &hwupload->params;
    struct hwupload_mc *mc = hwupload->hwmap_priv_data;
    struct gpu_ctx *gpu_ctx = hwupload->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;

    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data;
    int ret = av_mediacodec_release_buffer(buffer, 1);
    if (ret < 0)
        return ret;

    struct android_image *android_image;
    ret = ngli_android_imagereader_acquire_next_image(params->android_imagereader, &android_image);
    if (ret < 0)
        return ret;

    ngli_eglDestroyImageKHR(gl, mc->egl_image);
    mc->egl_image = NULL;
    ngli_android_image_freep(&mc->android_image);

    mc->android_image = android_image;

    AHardwareBuffer *hardware_buffer = ngli_android_image_get_hardware_buffer(mc->android_image);
    if (!hardware_buffer)
        return NGL_ERROR_EXTERNAL;

    EGLClientBuffer egl_buffer = ngli_eglGetNativeClientBufferANDROID(gl, hardware_buffer);
    if (!egl_buffer)
        return NGL_ERROR_EXTERNAL;

    static const EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR,
        EGL_TRUE,
        EGL_NONE,
    };

    const struct texture_gl *texture_gl = (struct texture_gl *)mc->texture;
    const GLuint id = texture_gl->id;

    mc->egl_image = ngli_eglCreateImageKHR(gl, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, egl_buffer, attrs);
    if (!mc->egl_image) {
        LOG(ERROR, "failed to create egl image");
        return NGL_ERROR_EXTERNAL;
    }

    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, id);
    ngli_glEGLImageTargetTexture2DOES(gl, GL_TEXTURE_EXTERNAL_OES, mc->egl_image);

    ngli_texture_gl_set_dimensions(mc->texture, frame->width, frame->height, 0);

    return 0;
}

static int mc_map_frame(struct hwupload *hwupload, struct sxplayer_frame *frame)
{
    struct ngl_ctx *ctx = hwupload->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;

    if (android_ctx->has_native_imagereader_api)
        return mc_map_frame_imagereader(hwupload, frame);

    return mc_map_frame_surfacetexture(hwupload, frame);
}

static void mc_uninit(struct hwupload *hwupload)
{
    struct ngl_ctx *ctx = hwupload->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    struct gpu_ctx *gpu_ctx = hwupload->gpu_ctx;
    struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    struct glcontext *gl = gpu_ctx_gl->glcontext;
    struct hwupload_mc *mc = hwupload->hwmap_priv_data;

    ngli_texture_freep(&mc->texture);

    if (android_ctx->has_native_imagereader_api) {
        ngli_eglDestroyImageKHR(gl, mc->egl_image);
        ngli_android_image_freep(&mc->android_image);
    }
}

const struct hwmap_class ngli_hwmap_mc_gl_class = {
    .name      = "mediacodec (oes zero-copy)",
    .priv_size = sizeof(struct hwupload_mc),
    .init      = mc_init,
    .map_frame = mc_map_frame,
    .uninit    = mc_uninit,
};
