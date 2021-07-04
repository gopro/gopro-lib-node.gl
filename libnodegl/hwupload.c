/*
 * Copyright 2017 GoPro Inc.
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

#include "config.h"
#include "hwupload.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

extern const struct hwmap_class ngli_hwmap_common_class;
extern const struct hwmap_class ngli_hwmap_mc_gl_class;
extern const struct hwmap_class ngli_hwmap_vt_darwin_gl_class;
extern const struct hwmap_class ngli_hwmap_vt_ios_gl_class;
extern const struct hwmap_class ngli_hwmap_vaapi_gl_class;

static const struct hwmap_class *hwupload_gl_class_map[] = {
    [SXPLAYER_PIXFMT_RGBA]        = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_BGRA]        = &ngli_hwmap_common_class,
    [SXPLAYER_SMPFMT_FLT]         = &ngli_hwmap_common_class,
#ifdef BACKEND_GL
#if defined(TARGET_ANDROID)
    [SXPLAYER_PIXFMT_MEDIACODEC]  = &ngli_hwmap_mc_gl_class,
#elif defined(TARGET_DARWIN)
    [SXPLAYER_PIXFMT_VT]          = &ngli_hwmap_vt_darwin_gl_class,
#elif defined(TARGET_IPHONE)
    [SXPLAYER_PIXFMT_VT]          = &ngli_hwmap_vt_ios_gl_class,
#elif defined(HAVE_VAAPI)
    [SXPLAYER_PIXFMT_VAAPI]       = &ngli_hwmap_vaapi_gl_class,
#endif
#endif
    [SXPLAYER_PIXFMT_NV12]        = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_YUV420P]     = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_YUV422P]     = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_YUV444P]     = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_P010LE]      = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_YUV420P10LE] = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_YUV422P10LE] = &ngli_hwmap_common_class,
    [SXPLAYER_PIXFMT_YUV444P10LE] = &ngli_hwmap_common_class,
};

static const struct hwmap_class *get_hwmap_class(int backend, struct sxplayer_frame *frame)
{
    if (backend == NGL_BACKEND_OPENGL || backend == NGL_BACKEND_OPENGLES) {
        if (frame->pix_fmt < 0 || frame->pix_fmt >= NGLI_ARRAY_NB(hwupload_gl_class_map))
            return NULL;

        return hwupload_gl_class_map[frame->pix_fmt];
    }

    return NULL;
}

static int init_hwconv(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct image *mapped_image = &hwupload->mapped_image;
    struct image *hwconv_image = &hwupload->hwconv_image;
    struct hwconv *hwconv = &hwupload->hwconv;

    ngli_hwconv_reset(hwconv);
    ngli_image_reset(hwconv_image);
    ngli_texture_freep(&hwupload->hwconv_texture);

    LOG(DEBUG, "converting texture '%s' from %s to rgba", node->label, hwupload->hwmap_class->name);

    struct texture_params params = s->params;
    params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    params.width  = mapped_image->params.width;
    params.height = mapped_image->params.height;
    params.usage |= NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    hwupload->hwconv_texture = ngli_texture_create(gpu_ctx);
    if (!hwupload->hwconv_texture)
        return NGL_ERROR_MEMORY;
    int ret = ngli_texture_init(hwupload->hwconv_texture, &params);
    if (ret < 0)
        goto end;

    struct image_params image_params = {
        .width = mapped_image->params.width,
        .height = mapped_image->params.height,
        .layout = NGLI_IMAGE_LAYOUT_DEFAULT,
        .color_scale = 1.f,
        .color_info = NGLI_COLOR_INFO_DEFAULTS,
    };
    ngli_image_init(hwconv_image, &image_params, &hwupload->hwconv_texture);

    ret = ngli_hwconv_init(hwconv, ctx, hwconv_image, &mapped_image->params);
    if (ret < 0)
        goto end;

    return 0;

end:
    ngli_hwconv_reset(hwconv);
    ngli_image_reset(hwconv_image);
    ngli_texture_freep(&hwupload->hwconv_texture);
    return ret;
}

static int exec_hwconv(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    struct texture *texture = hwupload->hwconv_texture;
    const struct texture_params *texture_params = &texture->params;
    struct image *mapped_image = &hwupload->mapped_image;
    struct hwconv *hwconv = &hwupload->hwconv;

    int ret = ngli_hwconv_convert_image(hwconv, mapped_image);
    if (ret < 0)
        return ret;

    if (texture_params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE)
        ngli_texture_generate_mipmap(texture);

    return 0;
}

int ngli_hwupload_upload_frame(struct ngl_node *node, struct sxplayer_frame *frame, struct image *image)
{
    struct ngl_ctx *ctx = node->ctx;
    const struct ngl_config *config = &ctx->config;
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;

    if (frame->width  != hwupload->width ||
        frame->height != hwupload->height ||
        frame->pix_fmt != hwupload->pix_fmt) {
        ngli_hwupload_uninit(node);

        const struct hwmap_class *hwmap_class = get_hwmap_class(config->backend, frame);
        if (!hwmap_class) {
            sxplayer_release_frame(frame);
            return NGL_ERROR_UNSUPPORTED;
        }
        ngli_assert(hwmap_class->priv_size);

        hwupload->hwmap_priv_data = ngli_calloc(1, hwmap_class->priv_size);
        if (!hwupload->hwmap_priv_data) {
            sxplayer_release_frame(frame);
            return NGL_ERROR_MEMORY;
        }

        int ret = hwmap_class->init(node, frame);
        if (ret < 0) {
            sxplayer_release_frame(frame);
            return ret;
        }
        hwupload->hwmap_class = hwmap_class;
        hwupload->pix_fmt = frame->pix_fmt;
        hwupload->width = frame->width;
        hwupload->height = frame->height;

        LOG(DEBUG, "mapping texture '%s' with method: %s", node->label, hwmap_class->name);
    }

    int ret = hwupload->hwmap_class->map_frame(node, frame);
    if (ret < 0)
        goto end;

    if (hwupload->require_hwconv) {
        if (!hwupload->hwconv_initialized) {
            ret = init_hwconv(node);
            if (ret < 0)
                return ret;
            hwupload->hwconv_initialized = 1;
        }
        ret = exec_hwconv(node);
        if (ret < 0)
            return ret;
        *image = hwupload->hwconv_image;
    } else {
        *image = hwupload->mapped_image;
    }

end:
    image->ts = frame->ts;

    if (!(hwupload->hwmap_class->flags &  HWMAP_FLAG_FRAME_OWNER))
        sxplayer_release_frame(frame);
    return ret;
}

void ngli_hwupload_uninit(struct ngl_node *node)
{
    struct texture_priv *s = node->priv_data;
    struct hwupload *hwupload = &s->hwupload;
    ngli_hwconv_reset(&hwupload->hwconv);
    hwupload->hwconv_initialized = 0;
    hwupload->require_hwconv = 0;
    ngli_image_reset(&hwupload->mapped_image);
    if (hwupload->hwmap_class && hwupload->hwmap_class->uninit) {
        hwupload->hwmap_class->uninit(node);
    }
    ngli_freep(&hwupload->hwmap_priv_data);
    hwupload->hwmap_class = NULL;
    hwupload->pix_fmt = 0;
    hwupload->width = 0;
    hwupload->height = 0;
}
