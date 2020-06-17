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
#include "glcontext.h"
#include "memory.h"

#include "buffer_gl.h"
#include "gtimer_gl.h"
#include "pipeline_gl.h"
#include "program_gl.h"
#include "rendertarget_gl.h"
#include "texture_gl.h"

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#if defined(HAVE_VAAPI)
#include "vaapi.h"
#endif

static int offscreen_rendertarget_init(struct gctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &ctx->config;

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
    s->rt_color = ngli_texture_create(s);
    if (!s->rt_color)
        return NGL_ERROR_MEMORY;
    int ret = ngli_texture_init(s->rt_color, &attachment_params);
    if (ret < 0)
        return ret;

    attachment_params.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    s->rt_depth = ngli_texture_create(s);
    if (!s->rt_depth)
        return NGL_ERROR_MEMORY;
    ret = ngli_texture_init(s->rt_depth, &attachment_params);
    if (ret < 0)
        return ret;

    struct rendertarget_params rt_params = {
        .width = config->width,
        .height = config->height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = s->rt_color,
        },
        .depth_stencil = {
            .attachment = s->rt_depth
        },
    };
    s->rt = ngli_rendertarget_create(s);
    if (!s->rt)
        return NGL_ERROR_MEMORY;
    ret = ngli_rendertarget_init(s->rt, &rt_params);
    if (ret < 0)
        return ret;

    ngli_gctx_set_rendertarget(s, s->rt);
    const int vp[4] = {0, 0, config->width, config->height};
    ngli_gctx_set_viewport(s, vp);

    return 0;
}

static void offscreen_rendertarget_reset(struct gctx *s)
{
    ngli_rendertarget_freep(&s->rt);
    ngli_texture_freep(&s->rt_color);
    ngli_texture_freep(&s->rt_depth);
}

static void capture_default(struct gctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_config *config = &ctx->config;
    struct rendertarget *rt = s->rt;
    struct rendertarget *capture_rt = s->capture_rt;

    ngli_rendertarget_blit(rt, capture_rt, 1);
    ngli_rendertarget_read_pixels(capture_rt, config->capture_buffer);
}

static void capture_ios(struct gctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct rendertarget *rt = s->rt;
    struct rendertarget *capture_rt = s->capture_rt;

    ngli_rendertarget_blit(rt, capture_rt, 1);
    ngli_glFinish(gl);
}

static void capture_gles_msaa(struct gctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_config *config = &ctx->config;
    struct rendertarget *rt = s->rt;
    struct rendertarget *capture_rt = s->capture_rt;
    struct rendertarget *oes_resolve_rt = s->oes_resolve_rt;

    ngli_rendertarget_blit(rt, oes_resolve_rt, 0);
    ngli_rendertarget_blit(oes_resolve_rt, capture_rt, 1);
    ngli_rendertarget_read_pixels(capture_rt, config->capture_buffer);
}

static void capture_ios_msaa(struct gctx *s)
{
    struct glcontext *gl = s->glcontext;
    struct rendertarget *rt = s->rt;
    struct rendertarget *capture_rt = s->capture_rt;
    struct rendertarget *oes_resolve_rt = s->oes_resolve_rt;

    ngli_rendertarget_blit(rt, oes_resolve_rt, 0);
    ngli_rendertarget_blit(oes_resolve_rt, capture_rt, 1);
    ngli_glFinish(gl);
}

static void capture_cpu_fallback(struct gctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_config *config = &ctx->config;
    struct rendertarget *rt = s->rt;

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

static int capture_init(struct gctx *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &ctx->config;
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
            s->capture_rt_color = ngli_texture_create(s);
            if (!s->capture_rt_color)
                return NGL_ERROR_MEMORY;
            int ret = ngli_texture_gl_wrap(s->capture_rt_color, &attachment_params, id);
            if (ret < 0)
                return ret;
#endif
        } else {
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            attachment_params.width = config->width;
            attachment_params.height = config->height;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            s->capture_rt_color = ngli_texture_create(s);
            if (!s->capture_rt_color)
                return NGL_ERROR_MEMORY;
            int ret = ngli_texture_init(s->capture_rt_color, &attachment_params);
            if (ret < 0)
                return ret;
        }

        struct rendertarget_params rt_params = {
            .width = config->width,
            .height = config->height,
            .nb_colors = 1,
            .colors[0] = {
                .attachment = s->capture_rt_color,
            },
        };
        s->capture_rt = ngli_rendertarget_create(s);
        if (!s->capture_rt)
            return NGL_ERROR_MEMORY;
        int ret = ngli_rendertarget_init(s->capture_rt, &rt_params);
        if (ret < 0)
            return ret;

        if (gl->backend == NGL_BACKEND_OPENGLES && config->samples > 0) {
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            attachment_params.width = config->width;
            attachment_params.height = config->height;
            attachment_params.samples = 0;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            s->oes_resolve_rt_color = ngli_texture_create(s);
            if (!s->oes_resolve_rt_color)
                return NGL_ERROR_MEMORY;
            int ret = ngli_texture_init(s->oes_resolve_rt_color, &attachment_params);
            if (ret < 0)
                return ret;

            struct rendertarget_params rt_params = {
                .width = config->width,
                .height = config->height,
                .nb_colors = 1,
                .colors[0] = {
                    .attachment = s->oes_resolve_rt_color,
                }
            };
            s->oes_resolve_rt = ngli_rendertarget_create(s);
            if (!s->oes_resolve_rt)
                return NGL_ERROR_MEMORY;
            ret = ngli_rendertarget_init(s->oes_resolve_rt, &rt_params);
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

static void capture_reset(struct gctx *s)
{
    ngli_rendertarget_freep(&s->capture_rt);
    ngli_texture_freep(&s->capture_rt_color);
    ngli_rendertarget_freep(&s->oes_resolve_rt);
    ngli_texture_freep(&s->oes_resolve_rt_color);
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

static int gl_init(struct gctx *s)
{
    int ret;
    struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;

    s->glcontext = ngli_glcontext_new(config);
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

    s->version = gl->version;
    s->features = gl->features;
    s->limits = gl->limits;
    s->default_rendertarget_desc.nb_colors = 1;
    s->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_R8G8B8A8_UNORM;
    s->default_rendertarget_desc.colors[0].samples = gl->samples;
    s->default_rendertarget_desc.colors[0].resolve = gl->samples > 1;
    s->default_rendertarget_desc.depth_stencil.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    s->default_rendertarget_desc.depth_stencil.samples = gl->samples;
    s->default_rendertarget_desc.depth_stencil.resolve = gl->samples > 1;
    ctx->rendertarget_desc = &s->default_rendertarget_desc;

    ngli_glstate_probe(gl, &s->glstate);

    ret = ngli_pgcache_init(&s->pgcache, s->ctx);
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

    struct graphicstate *graphicstate = &ctx->graphicstate;
    ngli_graphicstate_init(graphicstate);

#if defined(HAVE_VAAPI)
    ret = ngli_vaapi_init(s->ctx);
    if (ret < 0)
        LOG(WARNING, "could not initialize vaapi");
#endif

    return 0;
}

static int gl_resize(struct gctx *s, int width, int height, const int *viewport)
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

static int gl_pre_draw(struct gctx *s, double t)
{
    ngli_gctx_clear_color(s);
    ngli_gctx_clear_depth_stencil(s);

    return 0;
}

static int gl_post_draw(struct gctx *s, double t)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = s->glcontext;
    struct ngl_config *config = &ctx->config;

    ngli_glstate_update(s, &ctx->graphicstate);

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

static void gl_destroy(struct gctx *s)
{
    ngli_pgcache_reset(&s->pgcache);
    capture_reset(s);
    offscreen_rendertarget_reset(s);
#if defined(HAVE_VAAPI)
    ngli_vaapi_reset(s->ctx);
#endif
    ngli_glcontext_freep(&s->glcontext);
}

const struct gctx_class ngli_gctx_gl = {
    .name         = "OpenGL",
    .init         = gl_init,
    .resize       = gl_resize,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,

    .buffer_create = ngli_buffer_gl_create,
    .buffer_init   = ngli_buffer_gl_init,
    .buffer_upload = ngli_buffer_gl_upload,
    .buffer_freep  = ngli_buffer_gl_freep,

    .gtimer_create = ngli_gtimer_gl_create,
    .gtimer_init   = ngli_gtimer_gl_init,
    .gtimer_start  = ngli_gtimer_gl_start,
    .gtimer_stop   = ngli_gtimer_gl_stop,
    .gtimer_read   = ngli_gtimer_gl_read,
    .gtimer_freep  = ngli_gtimer_gl_freep,

    .pipeline_create         = ngli_pipeline_gl_create,
    .pipeline_init           = ngli_pipeline_gl_init,
    .pipeline_update_uniform = ngli_pipeline_gl_update_uniform,
    .pipeline_update_texture = ngli_pipeline_gl_update_texture,
    .pipeline_exec           = ngli_pipeline_gl_exec,
    .pipeline_freep          = ngli_pipeline_gl_freep,

    .program_create = ngli_program_gl_create,
    .program_init   = ngli_program_gl_init,
    .program_freep  = ngli_program_gl_freep,

    .rendertarget_create      = ngli_rendertarget_gl_create,
    .rendertarget_init        = ngli_rendertarget_gl_init,
    .rendertarget_blit        = ngli_rendertarget_gl_blit,
    .rendertarget_resolve     = ngli_rendertarget_gl_resolve,
    .rendertarget_read_pixels = ngli_rendertarget_gl_read_pixels,
    .rendertarget_freep       = ngli_rendertarget_gl_freep,

    .texture_create           = ngli_texture_gl_create,
    .texture_init             = ngli_texture_gl_init,
    .texture_has_mipmap       = ngli_texture_gl_has_mipmap,
    .texture_match_dimensions = ngli_texture_gl_match_dimensions,
    .texture_upload           = ngli_texture_gl_upload,
    .texture_generate_mipmap  = ngli_texture_gl_generate_mipmap,
    .texture_freep            = ngli_texture_gl_freep,
};

const struct gctx_class ngli_gctx_gles = {
    .name         = "OpenGL ES",
    .init         = gl_init,
    .resize       = gl_resize,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .destroy      = gl_destroy,

    .buffer_create = ngli_buffer_gl_create,
    .buffer_init   = ngli_buffer_gl_init,
    .buffer_upload = ngli_buffer_gl_upload,
    .buffer_freep  = ngli_buffer_gl_freep,

    .gtimer_create = ngli_gtimer_gl_create,
    .gtimer_init   = ngli_gtimer_gl_init,
    .gtimer_start  = ngli_gtimer_gl_start,
    .gtimer_stop   = ngli_gtimer_gl_stop,
    .gtimer_read   = ngli_gtimer_gl_read,
    .gtimer_freep  = ngli_gtimer_gl_freep,

    .pipeline_create         = ngli_pipeline_gl_create,
    .pipeline_init           = ngli_pipeline_gl_init,
    .pipeline_update_uniform = ngli_pipeline_gl_update_uniform,
    .pipeline_update_texture = ngli_pipeline_gl_update_texture,
    .pipeline_exec           = ngli_pipeline_gl_exec,
    .pipeline_freep          = ngli_pipeline_gl_freep,

    .program_create = ngli_program_gl_create,
    .program_init   = ngli_program_gl_init,
    .program_freep  = ngli_program_gl_freep,

    .rendertarget_create      = ngli_rendertarget_gl_create,
    .rendertarget_init        = ngli_rendertarget_gl_init,
    .rendertarget_blit        = ngli_rendertarget_gl_blit,
    .rendertarget_resolve     = ngli_rendertarget_gl_resolve,
    .rendertarget_read_pixels = ngli_rendertarget_gl_read_pixels,
    .rendertarget_freep       = ngli_rendertarget_gl_freep,

    .texture_create           = ngli_texture_gl_create,
    .texture_init             = ngli_texture_gl_init,
    .texture_has_mipmap       = ngli_texture_gl_has_mipmap,
    .texture_match_dimensions = ngli_texture_gl_match_dimensions,
    .texture_upload           = ngli_texture_gl_upload,
    .texture_generate_mipmap  = ngli_texture_gl_generate_mipmap,
    .texture_freep            = ngli_texture_gl_freep,
};
