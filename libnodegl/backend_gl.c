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

#include <string.h>

#include "log.h"
#include "gctx.h"
#include "nodes.h"
#include "backend.h"
#include "glcontext.h"
#include "memory.h"

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#if defined(HAVE_VAAPI)
#include "vaapi.h"
#endif

static int offscreen_rendertarget_init(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &s->config;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) && config->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, "
            "multisample anti-aliasing will be disabled");
        config->samples = 0;
    }

    struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    attachment_params.width = config->width;
    attachment_params.height = config->height;
    attachment_params.samples = config->samples;
    attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
    int ret = ngli_texture_init(&s->rt_color, s, &attachment_params);
    if (ret < 0)
        return ret;

    attachment_params.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    ret = ngli_texture_init(&s->rt_depth, s, &attachment_params);
    if (ret < 0)
        return ret;

    struct rendertarget_params rt_params = {
        .width = config->width,
        .height = config->height,
        .nb_colors = 1,
        .colors[0] = &s->rt_color,
        .depth_stencil = &s->rt_depth,
    };
    ret = ngli_rendertarget_init(&s->rt, s, &rt_params);
    if (ret < 0)
        return ret;

    ngli_gctx_set_rendertarget(s, &s->rt);
    const int vp[4] = {0, 0, config->width, config->height};
    ngli_gctx_set_viewport(s, vp);

    return 0;
}

static void offscreen_rendertarget_reset(struct ngl_ctx *s)
{
    ngli_rendertarget_reset(&s->rt);
    ngli_texture_reset(&s->rt_color);
    ngli_texture_reset(&s->rt_depth);
}

static void capture_default(struct ngl_ctx *s)
{
    struct ngl_config *config = &s->config;
    struct rendertarget *rt = &s->rt;
    struct rendertarget *capture_rt = &s->capture_rt;

    ngli_rendertarget_blit(rt, capture_rt, 1);
    ngli_rendertarget_read_pixels(capture_rt, config->capture_buffer);
}

static void capture_ios(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct rendertarget *rt = &s->rt;
    struct rendertarget *capture_rt = &s->capture_rt;

    ngli_rendertarget_blit(rt, capture_rt, 1);
    ngli_glFinish(gl);
}

static void capture_gles_msaa(struct ngl_ctx *s)
{
    struct ngl_config *config = &s->config;
    struct rendertarget *rt = &s->rt;
    struct rendertarget *capture_rt = &s->capture_rt;
    struct rendertarget *oes_resolve_rt = &s->oes_resolve_rt;

    ngli_rendertarget_blit(rt, oes_resolve_rt, 0);
    ngli_rendertarget_blit(oes_resolve_rt, capture_rt, 1);
    ngli_rendertarget_read_pixels(capture_rt, config->capture_buffer);
}

static void capture_ios_msaa(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct rendertarget *rt = &s->rt;
    struct rendertarget *capture_rt = &s->capture_rt;
    struct rendertarget *oes_resolve_rt = &s->oes_resolve_rt;

    ngli_rendertarget_blit(rt, oes_resolve_rt, 0);
    ngli_rendertarget_blit(oes_resolve_rt, capture_rt, 1);
    ngli_glFinish(gl);
}

static void capture_cpu_fallback(struct ngl_ctx *s)
{
    struct ngl_config *config = &s->config;
    struct rendertarget *rt = &s->rt;

    ngli_rendertarget_read_pixels(rt, s->capture_buffer);
    const int step = config->width * 4;
    const uint8_t *src = s->capture_buffer + (config->height - 1) * step;
    uint8_t *dst = config->capture_buffer;
    for (int i = 0; i < config->height; i++) {
        memcpy(dst, src, step);
        dst += step;
        src -= step;
    }
}

static int capture_init(struct ngl_ctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &s->config;
    const int ios_capture = gl->platform == NGL_PLATFORM_IOS && config->window;

    if (!config->capture_buffer && !ios_capture)
        return 0;

    if (gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) {
        if (ios_capture) {
#if defined(TARGET_IPHONE)
            CVPixelBufferRef capture_cvbuffer = (CVPixelBufferRef)config->window;
            s->capture_cvbuffer = (CVPixelBufferRef)CFRetain(capture_cvbuffer);
            if (!s->capture_cvbuffer)
                return NGL_ERROR_MEMORY;

            CVOpenGLESTextureCacheRef *cache = ngli_glcontext_get_texture_cache(gl);
            int width = CVPixelBufferGetWidth(s->capture_cvbuffer);
            int height = CVPixelBufferGetHeight(s->capture_cvbuffer);
            CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                        *cache,
                                                                        s->capture_cvbuffer,
                                                                        NULL,
                                                                        GL_TEXTURE_2D,
                                                                        GL_RGBA,
                                                                        width,
                                                                        height,
                                                                        GL_BGRA,
                                                                        GL_UNSIGNED_BYTE,
                                                                        0,
                                                                        &s->capture_cvtexture);
            if (err != noErr) {
                LOG(ERROR, "could not create CoreVideo texture from CVPixelBuffer: 0x%x", err);
                return NGL_ERROR_EXTERNAL;
            }

            GLuint id = CVOpenGLESTextureGetName(s->capture_cvtexture);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_B8G8R8A8_UNORM;
            attachment_params.width = width;
            attachment_params.height = height;
            int ret = ngli_texture_wrap(&s->capture_rt_color, s, &attachment_params, id);
            if (ret < 0)
                return ret;
#endif
        } else {
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            attachment_params.width = config->width;
            attachment_params.height = config->height;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            int ret = ngli_texture_init(&s->capture_rt_color, s, &attachment_params);
            if (ret < 0)
                return ret;
        }

        struct rendertarget_params rt_params = {
            .width = config->width,
            .height = config->height,
            .nb_colors = 1,
            .colors[0] = &s->capture_rt_color,
        };
        int ret = ngli_rendertarget_init(&s->capture_rt, s, &rt_params);
        if (ret < 0)
            return ret;

        if (gl->backend == NGL_BACKEND_OPENGLES && config->samples > 0) {
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            attachment_params.width = config->width;
            attachment_params.height = config->height;
            attachment_params.samples = 0;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            int ret = ngli_texture_init(&s->oes_resolve_rt_color, s, &attachment_params);
            if (ret < 0)
                return ret;

            struct rendertarget_params rt_params = {
                .width = config->width,
                .height = config->height,
                .nb_colors = 1,
                .colors[0] = &s->oes_resolve_rt_color,
            };
            ret = ngli_rendertarget_init(&s->oes_resolve_rt, s, &rt_params);
            if (ret < 0)
                return ret;

            s->capture_func = config->capture_buffer ? capture_gles_msaa : capture_ios_msaa;
        } else {
            s->capture_func = config->capture_buffer ? capture_default : capture_ios;
        }

    } else {
        if (ios_capture) {
            LOG(WARNING, "context does not support the framebuffer object feature, "
                "capturing to a CVPixelBuffer is not supported");
            return NGL_ERROR_UNSUPPORTED;
        }
        s->capture_buffer = ngli_calloc(config->width * config->height, 4 /* RGBA */);
        if (!s->capture_buffer)
            return NGL_ERROR_MEMORY;

        s->capture_func = capture_cpu_fallback;
    }

    ngli_assert(s->capture_func);

    return 0;
}

static void capture_reset(struct ngl_ctx *s)
{
    ngli_rendertarget_reset(&s->capture_rt);
    ngli_texture_reset(&s->capture_rt_color);
    ngli_rendertarget_reset(&s->oes_resolve_rt);
    ngli_texture_reset(&s->oes_resolve_rt_color);
    ngli_free(s->capture_buffer);
    s->capture_buffer = NULL;
#if defined(TARGET_IPHONE)
    if (s->capture_cvbuffer) {
        CFRelease(s->capture_cvbuffer);
        s->capture_cvbuffer = NULL;
    }
    if (s->capture_cvtexture) {
        CFRelease(s->capture_cvtexture);
        s->capture_cvtexture = NULL;
    }
#endif
    s->capture_func = NULL;
}

static int gl_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    int ret;

    memcpy(&s->config, config, sizeof(s->config));

    s->glcontext = ngli_glcontext_new(&s->config);
    if (!s->glcontext)
        return NGL_ERROR_MEMORY;

    struct glcontext *gl = s->glcontext;
    if (gl->offscreen) {
        ret = offscreen_rendertarget_init(s);
        if (ret < 0)
            return ret;

        ret = capture_init(s);
        if (ret < 0)
            return ret;
    }

    ngli_glstate_probe(gl, &s->glstate);

    ret = ngli_pgcache_init(&s->pgcache, s);
    if (ret < 0)
        return ret;

    /* This field is used by the pipeline API in order to reduce the total
     * number of GL program switches. This means pipeline draw calls may alter
     * this value, but we don't want it to be hard-reconfigure resilient (the
     * value is specific to a given GL context). As a result, we need to make
     * sure the value is always reset. */
    s->program_id = 0;

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        ngli_gctx_set_viewport(s, viewport);
    } else {
        const int default_viewport[] = {0, 0, gl->width, gl->height};
        ngli_gctx_set_viewport(s, default_viewport);
    }

    const GLint scissor[] = {0, 0, gl->width, gl->height};
    ngli_gctx_set_scissor(s, scissor);

    ngli_gctx_set_clear_color(s, config->clear_color);

    struct graphicstate *graphicstate = &s->graphicstate;
    ngli_graphicstate_init(graphicstate);

#if defined(HAVE_VAAPI)
    ret = ngli_vaapi_init(s);
    if (ret < 0)
        LOG(WARNING, "could not initialize vaapi");
#endif

    return 0;
}

static int gl_resize(struct ngl_ctx *s, int width, int height, const int *viewport)
{
    struct glcontext *gl = s->glcontext;
    if (gl->offscreen)
        return NGL_ERROR_INVALID_USAGE;

    int ret = ngli_glcontext_resize(gl, width, height);
    if (ret < 0)
        return ret;

    if (viewport && viewport[2] > 0 && viewport[3] > 0) {
        ngli_gctx_set_viewport(s, viewport);
    } else {
        const int default_viewport[] = {0, 0, gl->width, gl->height};
        ngli_gctx_set_viewport(s, default_viewport);
    }

    const int scissor[] = {0, 0, gl->width, gl->height};
    ngli_gctx_set_scissor(s, scissor);

    return 0;
}

static int gl_pre_draw(struct ngl_ctx *s, double t)
{
    ngli_gctx_clear_color(s);
    ngli_gctx_clear_depth_stencil(s);

    return 0;
}

static int gl_post_draw(struct ngl_ctx *s, double t)
{
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &s->config;

    ngli_glstate_update(s, &s->graphicstate);

    if (s->capture_func)
        s->capture_func(s);

    int ret = 0;
    if (ngli_glcontext_check_gl_error(gl, __FUNCTION__))
        ret = -1;

    if (config->set_surface_pts)
        ngli_glcontext_set_surface_pts(gl, t);

    ngli_glcontext_swap_buffers(gl);

    return ret;
}

static void gl_destroy(struct ngl_ctx *s)
{
    ngli_pgcache_reset(&s->pgcache);
    capture_reset(s);
    offscreen_rendertarget_reset(s);
#if defined(HAVE_VAAPI)
    ngli_vaapi_reset(s);
#endif
    ngli_glcontext_freep(&s->glcontext);
}

const struct backend ngli_backend_gl = {
    .name         = "OpenGL",
    .configure    = gl_configure,
    .resize       = gl_resize,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,
};

const struct backend ngli_backend_gles = {
    .name         = "OpenGL ES",
    .configure    = gl_configure,
    .resize       = gl_resize,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,
};
