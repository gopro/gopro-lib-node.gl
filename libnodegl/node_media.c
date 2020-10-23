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

#include "config.h"

#if defined(TARGET_ANDROID)
#include <libavcodec/mediacodec.h>
#include "android_imagereader.h"
#endif

#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#if defined(TARGET_ANDROID)
#include "gctx.h"
#include "backends/gl/texture_gl.h"
#endif

static const struct param_choices sxplayer_log_level_choices = {
    .name = "sxplayer_log_level",
    .consts = {
        {"verbose", SXPLAYER_LOG_VERBOSE, .desc=NGLI_DOCSTRING("verbose messages")},
        {"debug",   SXPLAYER_LOG_DEBUG,   .desc=NGLI_DOCSTRING("debugging messages")},
        {"info",    SXPLAYER_LOG_INFO,    .desc=NGLI_DOCSTRING("informational messages")},
        {"warning", SXPLAYER_LOG_WARNING, .desc=NGLI_DOCSTRING("warning messages")},
        {"error",   SXPLAYER_LOG_ERROR,   .desc=NGLI_DOCSTRING("error messages")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct media_priv, x)
static const struct node_param media_params[] = {
    {"filename", PARAM_TYPE_STR, OFFSET(filename), {.str=NULL}, PARAM_FLAG_NON_NULL,
                 .desc=NGLI_DOCSTRING("path to input media file")},
    {"sxplayer_min_level", PARAM_TYPE_SELECT, OFFSET(sxplayer_min_level), {.i64=SXPLAYER_LOG_WARNING},
                           .choices=&sxplayer_log_level_choices,
                           .desc=NGLI_DOCSTRING("sxplayer min logging level")},
    {"time_anim", PARAM_TYPE_NODE, OFFSET(anim),
                  .node_types=(const int[]){NGL_NODE_ANIMATEDTIME, -1},
                  .desc=NGLI_DOCSTRING("time remapping animation (must use a `linear` interpolation)")},
    {"audio_tex", PARAM_TYPE_BOOL, OFFSET(audio_tex),
                  .desc=NGLI_DOCSTRING("load the audio and expose it as a stereo waves and frequencies buffer")},
    {"max_nb_packets", PARAM_TYPE_INT, OFFSET(max_nb_packets), {.i64=1},
                       .desc=NGLI_DOCSTRING("maximum number of packets in sxplayer demuxing queue")},
    {"max_nb_frames",  PARAM_TYPE_INT, OFFSET(max_nb_frames),  {.i64=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in sxplayer decoding queue")},
    {"max_nb_sink",    PARAM_TYPE_INT, OFFSET(max_nb_sink),    {.i64=1},
                       .desc=NGLI_DOCSTRING("maximum number of frames in sxplayer filtering queue")},
    {"max_pixels",     PARAM_TYPE_INT, OFFSET(max_pixels),     {.i64=0},
                       .desc=NGLI_DOCSTRING("maximum number of pixels per frame")},
    {"stream_idx",     PARAM_TYPE_INT, OFFSET(stream_idx),     {.i64=-1},
                       .desc=NGLI_DOCSTRING("force a stream number instead of picking the \"best\" one")},
    {NULL}
};

static const int log_levels[] = {
    [SXPLAYER_LOG_VERBOSE] = NGL_LOG_VERBOSE,
    [SXPLAYER_LOG_DEBUG]   = NGL_LOG_DEBUG,
    [SXPLAYER_LOG_INFO]    = NGL_LOG_INFO,
    [SXPLAYER_LOG_WARNING] = NGL_LOG_WARNING,
    [SXPLAYER_LOG_ERROR]   = NGL_LOG_ERROR,
};

static void callback_sxplayer_log(void *arg, int level, const char *filename, int ln,
                                  const char *fn, const char *fmt, va_list vl)
{
    if (level < 0 || level >= NGLI_ARRAY_NB(log_levels))
        return;

    struct media_priv *s = arg;
    if (level < s->sxplayer_min_level)
        return;

    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, vl);
    if (buf[0])
        ngli_log_print(log_levels[level], __FILE__, __LINE__, __func__,
                       "[SXPLAYER %s:%d %s] %s", filename, ln, fn, buf);
}

static int media_init(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;

    s->player = sxplayer_create(s->filename);
    if (!s->player)
        return NGL_ERROR_MEMORY;

    sxplayer_set_log_callback(s->player, s, callback_sxplayer_log);

    struct ngl_node *anim_node = s->anim;
    if (anim_node) {
        struct variable_priv *anim = anim_node->priv_data;

        // Set the media time boundaries using the time remapping animation
        if (anim->nb_animkf) {
            const struct animkeyframe_priv *kf0 = anim->animkf[0]->priv_data;
            const double initial_seek = kf0->scalar;

            sxplayer_set_option(s->player, "skip", initial_seek);

            if (anim->nb_animkf > 1) {
                const struct animkeyframe_priv *kfn = anim->animkf[anim->nb_animkf - 1]->priv_data;
                const double last_time = kfn->scalar;
                sxplayer_set_option(s->player, "trim_duration", last_time - initial_seek);
            }
        }
    }

    if (s->max_nb_packets) sxplayer_set_option(s->player, "max_nb_packets", s->max_nb_packets);
    if (s->max_nb_frames)  sxplayer_set_option(s->player, "max_nb_frames",  s->max_nb_frames);
    if (s->max_nb_sink)    sxplayer_set_option(s->player, "max_nb_sink",    s->max_nb_sink);
    if (s->max_pixels)     sxplayer_set_option(s->player, "max_pixels",     s->max_pixels);

    sxplayer_set_option(s->player, "stream_idx", s->stream_idx);

    sxplayer_set_option(s->player, "sw_pix_fmt", SXPLAYER_PIXFMT_RGBA);
#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    sxplayer_set_option(s->player, "vt_pix_fmt", "nv12");
#endif

    if (s->audio_tex) {
        sxplayer_set_option(s->player, "avselect", SXPLAYER_SELECT_AUDIO);
        sxplayer_set_option(s->player, "audio_texture", 1);
        return 0;
    }

#if defined(TARGET_ANDROID)
    struct ngl_ctx *ctx = node->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    const struct ngl_config *config = &ctx->config;
    struct gctx *gctx = ctx->gctx;

    if (config->backend == NGL_BACKEND_OPENGLES) {
        struct texture_params params = {
            .type         = NGLI_TEXTURE_TYPE_2D,
            .format       = NGLI_FORMAT_UNDEFINED,
            .min_filter   = NGLI_FILTER_NEAREST,
            .mag_filter   = NGLI_FILTER_NEAREST,
            .wrap_s       = NGLI_WRAP_CLAMP_TO_EDGE,
            .wrap_t       = NGLI_WRAP_CLAMP_TO_EDGE,
            .wrap_r       = NGLI_WRAP_CLAMP_TO_EDGE,
            .usage        = NGLI_TEXTURE_USAGE_SAMPLED_BIT,
            .external_oes = 1,
        };

        s->android_texture = ngli_texture_create(gctx);
        if (!s->android_texture)
            return NGL_ERROR_MEMORY;

        int ret = ngli_texture_init(s->android_texture, &params);
        if (ret < 0)
            return ret;

        void *android_surface = NULL;
        if (android_ctx->has_native_imagereader_api) {
            s->android_imagereader = ngli_android_imagereader_create(android_ctx, 1, 1,
                                                                     NGLI_ANDROID_IMAGE_FORMAT_YUV_420_888, 2);
            if (!s->android_imagereader)
                return NGL_ERROR_MEMORY;

            ret = ngli_android_imagereader_get_window(s->android_imagereader, &android_surface);
            if (ret < 0)
                return ret;
        } else {
            s->android_handlerthread = ngli_android_handlerthread_new();
            if (!s->android_handlerthread)
                return NGL_ERROR_MEMORY;

            void *handler = ngli_android_handlerthread_get_native_handler(s->android_handlerthread);
            if (!handler)
                return NGL_ERROR_EXTERNAL;

            struct texture_gl *texture_gl = (struct texture_gl *)s->android_texture;
            s->android_surface = ngli_android_surface_new(texture_gl->id, handler);
            if (!s->android_surface)
                return NGL_ERROR_MEMORY;

            void *android_surface = ngli_android_surface_get_surface(s->android_surface);
            if (!android_surface)
                return NGL_ERROR_EXTERNAL;
        }

        sxplayer_set_option(s->player, "opaque", &android_surface);
    }
#elif defined(HAVE_VAAPI)
    struct ngl_ctx *ctx = node->ctx;
    sxplayer_set_option(s->player, "opaque", &ctx->va_display);
#endif

    return 0;
}

static int media_prepare(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    if (s->nb_parents++) {
        /*
         * On Android, the frame can only be uploaded once and each subsequent
         * upload will be a noop which results in an empty texture. This
         * limitation prevents us from sharing the Media node.
         */
        LOG(ERROR, "Media nodes can not be shared, the Texture should be shared instead");
        return NGL_ERROR_INVALID_USAGE;
    }
    return 0;
}

static int media_prefetch(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    sxplayer_start(s->player);
    return 0;
}

static const char * const pix_fmt_names[] = {
    [SXPLAYER_PIXFMT_RGBA]       = "rgba",
    [SXPLAYER_PIXFMT_BGRA]       = "bgra",
    [SXPLAYER_PIXFMT_VT]         = "vt",
    [SXPLAYER_PIXFMT_MEDIACODEC] = "mediacodec",
    [SXPLAYER_PIXFMT_VAAPI]      = "vaapi",
};

static int media_update(struct ngl_node *node, double t)
{
    struct media_priv *s = node->priv_data;
    struct ngl_node *anim_node = s->anim;
    double media_time = t;

    if (anim_node) {
        struct variable_priv *anim = anim_node->priv_data;

        if (anim->nb_animkf >= 1) {
            const struct animkeyframe_priv *kf0 = anim->animkf[0]->priv_data;
            const double initial_seek = kf0->scalar;

            if (anim->nb_animkf == 1) {
                media_time = NGLI_MAX(0, t - kf0->time);
            } else {
                int ret = ngli_node_update(anim_node, t);
                if (ret < 0)
                    return ret;
                media_time = NGLI_MAX(0, anim->dval - initial_seek);
            }

            TRACE("remapped time f(%g)=%g", t, media_time);
        }
    }

    sxplayer_release_frame(s->frame);

    TRACE("get frame from %s at t=%g", node->label, media_time);
    struct sxplayer_frame *frame = sxplayer_get_frame(s->player, media_time);
    if (frame) {
        const char *pix_fmt_str = frame->pix_fmt >= 0 &&
                                  frame->pix_fmt < NGLI_ARRAY_NB(pix_fmt_names) ? pix_fmt_names[frame->pix_fmt]
                                                                                : NULL;
        if (s->audio_tex) {
            if (frame->pix_fmt != SXPLAYER_SMPFMT_FLT) {
                LOG(ERROR, "unexpected %s (%d) sxplayer frame",
                    pix_fmt_str ? pix_fmt_str : "unknown", frame->pix_fmt);
                return NGL_ERROR_BUG;
            }
            pix_fmt_str = "audio";
        } else if (!pix_fmt_str) {
            LOG(ERROR, "invalid pixel format %d in sxplayer frame", frame->pix_fmt);
            return NGL_ERROR_BUG;
        }
        TRACE("got frame %dx%d %s with ts=%f", frame->width, frame->height,
              pix_fmt_str, frame->ts);
    }
    s->frame = frame;
    return 0;
}

static void media_release(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    sxplayer_release_frame(s->frame);
    s->frame = NULL;
    sxplayer_stop(s->player);
}

static void media_uninit(struct ngl_node *node)
{
    struct media_priv *s = node->priv_data;
    sxplayer_free(&s->player);

#if defined(TARGET_ANDROID)
    struct ngl_ctx *ctx = node->ctx;
    struct android_ctx *android_ctx = &ctx->android_ctx;
    if (android_ctx->has_native_imagereader_api) {
        ngli_android_imagereader_freep(&s->android_imagereader);
    } else {
        ngli_android_surface_free(&s->android_surface);
        ngli_android_handlerthread_free(&s->android_handlerthread);
    }
    ngli_texture_freep(&s->android_texture);
#endif
}

const struct node_class ngli_media_class = {
    .id        = NGL_NODE_MEDIA,
    .name      = "Media",
    .init      = media_init,
    .prepare   = media_prepare,
    .prefetch  = media_prefetch,
    .update    = media_update,
    .release   = media_release,
    .uninit    = media_uninit,
    .priv_size = sizeof(struct media_priv),
    .params    = media_params,
    .file      = __FILE__,
};
