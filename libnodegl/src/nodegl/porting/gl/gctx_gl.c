/*
 * Copyright 2019 GoPro Inc.
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

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "nodegl/porting/gl/buffer_gl.h"
#include "nodegl/gctx.h"
#include "nodegl/porting/gl/gctx_gl.h"
#include "nodegl/porting/gl/glcontext.h"
#include "nodegl/porting/gl/gtimer_gl.h"
#include "nodegl/log.h"
#include "nodegl/math_utils.h"
#include "nodegl/memory.h"
#include "nodegl/nodes.h"
#include "nodegl/porting/gl/pipeline_gl.h"
#include "nodegl/porting/gl/program_gl.h"
#include "nodegl/porting/gl/rendertarget_gl.h"
#include "nodegl/porting/gl/texture_gl.h"

#if defined(HAVE_VAAPI)
#include "vaapi.h"
#endif

#define ENABLE_DEBUG

static void capture_default(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct ngl_config *config = &s->config;
    struct rendertarget *rt = s_priv->rt;

    ngli_rendertarget_resolve(rt);
    ngli_rendertarget_read_pixels(rt, config->capture_buffer);
}

static void capture_ios(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct rendertarget *rt = s_priv->rt;

    ngli_rendertarget_resolve(rt);
    ngli_glFinish(gl);
}

static int offscreen_rendertarget_init(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct ngl_config *config = &s->config;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) && config->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, "
            "multisample anti-aliasing will be disabled");
        config->samples = 0;
    }

    const int ios_capture = gl->platform == NGL_PLATFORM_IOS && config->window;
    if (ios_capture) {
#if defined(TARGET_IPHONE)
        CVPixelBufferRef capture_cvbuffer = (CVPixelBufferRef)config->window;
        s_priv->capture_cvbuffer = (CVPixelBufferRef)CFRetain(capture_cvbuffer);
        if (!s_priv->capture_cvbuffer)
            return NGL_ERROR_MEMORY;

        CVOpenGLESTextureCacheRef *cache = ngli_glcontext_get_texture_cache(gl);
        int width = CVPixelBufferGetWidth(s_priv->capture_cvbuffer);
        int height = CVPixelBufferGetHeight(s_priv->capture_cvbuffer);
        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                    *cache,
                                                                    s_priv->capture_cvbuffer,
                                                                    NULL,
                                                                    GL_TEXTURE_2D,
                                                                    GL_RGBA,
                                                                    width,
                                                                    height,
                                                                    GL_BGRA,
                                                                    GL_UNSIGNED_BYTE,
                                                                    0,
                                                                    &s_priv->capture_cvtexture);
        if (err != noErr) {
            LOG(ERROR, "could not create CoreVideo texture from CVPixelBuffer: 0x%x", err);
            return NGL_ERROR_EXTERNAL;
        }

        GLuint id = CVOpenGLESTextureGetName(s_priv->capture_cvtexture);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

        struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
        attachment_params.format = NGLI_FORMAT_B8G8R8A8_UNORM;
        attachment_params.width = width;
        attachment_params.height = height;
        s_priv->color = ngli_texture_create(s);
        if (!s_priv->color)
            return NGL_ERROR_MEMORY;
        int ret = ngli_texture_gl_wrap(s_priv->color, &attachment_params, id);
        if (ret < 0)
            return ret;
#endif
    } else {
        struct texture_params params = NGLI_TEXTURE_PARAM_DEFAULTS;
        params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
        params.width = config->width;
        params.height = config->height;
        params.usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
            NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
        s_priv->color = ngli_texture_create(s);
        if (!s_priv->color)
            return NGL_ERROR_MEMORY;
        int ret = ngli_texture_init(s_priv->color, &params);
        if (ret < 0)
            return ret;
    }

    if (config->samples) {
        struct texture_params params = NGLI_TEXTURE_PARAM_DEFAULTS;
        params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
        params.width = config->width;
        params.height = config->height;
        params.usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
                NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
        params.samples = config->samples;
        s_priv->ms_color = ngli_texture_create(s);
        if (!s_priv->ms_color)
            return NGL_ERROR_MEMORY;
        int ret = ngli_texture_init(s_priv->ms_color, &params);
        if (ret < 0)
            return ret;
    }

    struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    attachment_params.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    attachment_params.width = config->width;
    attachment_params.height = config->height;
    attachment_params.samples = config->samples;
    attachment_params.usage = NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT |
        NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    s_priv->depth = ngli_texture_create(s);
    if (!s_priv->depth)
        return NGL_ERROR_MEMORY;
    int ret = ngli_texture_init(s_priv->depth, &attachment_params);
    if (ret < 0)
        return ret;

    struct rendertarget_params rt_params = {
        .width = config->width,
        .height = config->height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment     = config->samples ? s_priv->ms_color : s_priv->color,
            .resolve_target = config->samples ? s_priv->color    : NULL,
        },
        .depth_stencil = {
            .attachment = s_priv->depth,
        },
    };

    s_priv->rt = ngli_rendertarget_create(s);
    if (!s_priv->rt)
        return NGL_ERROR_MEMORY;
    ret = ngli_rendertarget_init(s_priv->rt, &rt_params);
    if (ret < 0)
        return ret;

    s_priv->capture_func = ios_capture ? capture_ios : capture_default;

    ngli_gctx_bind_rendertarget(s, s_priv->rt);
    const int vp[4] = {0, 0, config->width, config->height};
    ngli_gctx_set_viewport(s, vp);

    return 0;
}

static void offscreen_rendertarget_reset(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    ngli_rendertarget_freep(&s_priv->rt);
    ngli_texture_freep(&s_priv->color);
    ngli_texture_freep(&s_priv->ms_color);
    ngli_texture_freep(&s_priv->depth);
#if defined(TARGET_IPHONE)
    if (s_priv->capture_cvbuffer) {
        CFRelease(s_priv->capture_cvbuffer);
        s_priv->capture_cvbuffer = NULL;
    }
    if (s_priv->capture_cvtexture) {
        CFRelease(s_priv->capture_cvtexture);
        s_priv->capture_cvtexture = NULL;
    }
#endif
    s_priv->capture_func = NULL;
}

static struct gctx *gl_create(const struct ngl_config *config)
{
    struct gctx_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    return (struct gctx *)s;
}

static void debug_message_callback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}

static int gl_init(struct gctx *s)
{
    int ret;
    const struct ngl_config *config = &s->config;
    struct gctx_gl *s_priv = (struct gctx_gl *)s;

    s_priv->glcontext = ngli_glcontext_new(config);
    if (!s_priv->glcontext)
        return NGL_ERROR_MEMORY;

#ifdef ENABLE_DEBUG
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debug_message_callback, NULL);
#endif

    struct glcontext *gl = s_priv->glcontext;
    s->features = gl->features;

    if (gl->offscreen) {
        ret = offscreen_rendertarget_init(s);
        if (ret < 0)
            return ret;
    }

    s->version = gl->version;
    s->features = gl->features;
    s->limits = gl->limits;
    s_priv->default_rendertarget_desc.nb_colors = 1;
    s_priv->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_R8G8B8A8_UNORM;
    s_priv->default_rendertarget_desc.colors[0].samples = gl->samples;
    s_priv->default_rendertarget_desc.colors[0].resolve = gl->samples > 1;
    s_priv->default_rendertarget_desc.depth_stencil.format = NGLI_FORMAT_D24_UNORM_S8_UINT;
    s_priv->default_rendertarget_desc.depth_stencil.samples = gl->samples;
    s_priv->default_rendertarget_desc.depth_stencil.resolve = gl->samples > 1;

    ngli_glstate_probe(gl, &s_priv->glstate);
    s_priv->default_graphicstate = NGLI_GRAPHICSTATE_DEFAULTS;

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

    return 0;
}

static int gl_resize(struct gctx *s, int width, int height, const int *viewport)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
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
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct ngl_config *config = &s->config;

    ngli_glstate_update(s, &s_priv->default_graphicstate);

    if (s_priv->capture_func)
        s_priv->capture_func(s);

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
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    offscreen_rendertarget_reset(s);
    ngli_glcontext_freep(&s_priv->glcontext);
}

static int gl_transform_cull_mode(struct gctx *s, int cull_mode)
{
    const struct ngl_config *config = &s->config;
    if (!config->offscreen)
        return cull_mode;
    static const int cull_mode_map[NGLI_CULL_MODE_NB] = {
        [NGLI_CULL_MODE_NONE]      = NGLI_CULL_MODE_NONE,
        [NGLI_CULL_MODE_FRONT_BIT] = NGLI_CULL_MODE_BACK_BIT,
        [NGLI_CULL_MODE_BACK_BIT]  = NGLI_CULL_MODE_FRONT_BIT,
    };
    return cull_mode_map[cull_mode];
}

static void gl_transform_projection_matrix(struct gctx *s, float *dst)
{
    const struct ngl_config *config = &s->config;
    if (!config->offscreen)
        return;
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    ngli_mat4_mul(dst, matrix, dst);
}

static void gl_get_rendertarget_uvcoord_matrix(struct gctx *s, float *dst)
{
    const struct ngl_config *config = &s->config;
    if (config->offscreen)
        return;
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    memcpy(dst, matrix, 4 * 4 * sizeof(float));
}

static void gl_bind_rendertarget(struct gctx *s, struct rendertarget *rt)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    if (rt == s_priv->rendertarget)
        return;

    struct rendertarget_gl *rt_gl = (struct rendertarget_gl *)rt;
    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    s_priv->rendertarget = rt;
}

static struct rendertarget *gl_get_rendertarget(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    return s_priv->rendertarget;
}

static const struct rendertarget_desc *gl_get_default_rendertarget_desc(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    return &s_priv->default_rendertarget_desc;
}

static void gl_set_viewport(struct gctx *s, const int *viewport)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
    memcpy(&s_priv->viewport, viewport, sizeof(s_priv->viewport));
}

static void gl_get_viewport(struct gctx *s, int *viewport)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(viewport, &s_priv->viewport, sizeof(s_priv->viewport));
}

static void gl_set_scissor(struct gctx *s, const int *scissor)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(&s_priv->scissor, scissor, sizeof(s_priv->scissor));
}

static void gl_get_scissor(struct gctx *s, int *scissor)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(scissor, &s_priv->scissor, sizeof(s_priv->scissor));
}

static void gl_set_clear_color(struct gctx *s, const float *color)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    memcpy(s_priv->clear_color, color, sizeof(s_priv->clear_color));
    ngli_glClearColor(gl, color[0], color[1], color[2], color[3]);
}

static void gl_get_clear_color(struct gctx *s, float *color)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    memcpy(color, &s_priv->clear_color, sizeof(s_priv->clear_color));
}

static void gl_clear_color(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct glstate *glstate = &s_priv->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

static void gl_clear_depth_stencil(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;
    struct glstate *glstate = &s_priv->glstate;

    const int scissor_test = glstate->scissor_test;
    ngli_glDisable(gl, GL_SCISSOR_TEST);

    ngli_glClear(gl, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (scissor_test)
        ngli_glEnable(gl, GL_SCISSOR_TEST);
}

static void gl_invalidate_depth_stencil(struct gctx *s)
{
    struct gctx_gl *s_priv = (struct gctx_gl *)s;
    struct glcontext *gl = s_priv->glcontext;

    if (!(gl->features & NGLI_FEATURE_INVALIDATE_SUBDATA))
        return;

    static const GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    ngli_glInvalidateFramebuffer(gl, GL_FRAMEBUFFER, NGLI_ARRAY_NB(attachments), attachments);
}

static int gl_get_preferred_depth_format(struct gctx *s)
{
    return NGLI_FORMAT_D16_UNORM;
}

static int gl_get_preferred_depth_stencil_format(struct gctx *s)
{
    return NGLI_FORMAT_D24_UNORM_S8_UINT;
}

static void gl_flush(struct gctx *s)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s;
    struct glcontext *gl = gctx_gl->glcontext;
    ngli_glFlush(gl);
}

static void gl_wait_idle(struct gctx *s)
{
}

const struct gctx_class ngli_gctx_gl = {
    .name         = "OpenGL",
    .create       = gl_create,
    .init         = gl_init,
    .resize       = gl_resize,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .wait_idle    = gl_wait_idle,
    .destroy      = gl_destroy,

    .transform_cull_mode              = gl_transform_cull_mode,
    .transform_projection_matrix      = gl_transform_projection_matrix,
    .get_rendertarget_uvcoord_matrix  = gl_get_rendertarget_uvcoord_matrix,

    .bind_rendertarget         = gl_bind_rendertarget,
    .get_rendertarget         = gl_get_rendertarget,
    .get_default_rendertarget_desc = gl_get_default_rendertarget_desc,
    .set_viewport             = gl_set_viewport,
    .get_viewport             = gl_get_viewport,
    .set_scissor              = gl_set_scissor,
    .get_scissor              = gl_get_scissor,
    .set_clear_color          = gl_set_clear_color,
    .get_clear_color          = gl_get_clear_color,
    .clear_color              = gl_clear_color,
    .clear_depth_stencil      = gl_clear_depth_stencil,
    .invalidate_depth_stencil = gl_invalidate_depth_stencil,
    .get_preferred_depth_format = gl_get_preferred_depth_format,
    .get_preferred_depth_stencil_format = gl_get_preferred_depth_stencil_format,
    .flush                    = gl_flush,

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
    .pipeline_bind_resources = ngli_pipeline_gl_bind_resources,
    .pipeline_update_attribute = ngli_pipeline_gl_update_attribute,
    .pipeline_update_uniform = ngli_pipeline_gl_update_uniform,
    .pipeline_update_texture = ngli_pipeline_gl_update_texture,
    .pipeline_draw           = ngli_pipeline_gl_draw,
    .pipeline_draw_indexed   = ngli_pipeline_gl_draw_indexed,
    .pipeline_dispatch       = ngli_pipeline_gl_dispatch,
    .pipeline_freep          = ngli_pipeline_gl_freep,

    .program_create = ngli_program_gl_create,
    .program_init   = ngli_program_gl_init,
    .program_freep  = ngli_program_gl_freep,

    .rendertarget_create      = ngli_rendertarget_gl_create,
    .rendertarget_init        = ngli_rendertarget_gl_init,
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
    .create       = gl_create,
    .init         = gl_init,
    .resize       = gl_resize,
    .pre_draw     = gl_pre_draw,
    .post_draw    = gl_post_draw,
    .wait_idle    = gl_wait_idle,
    .destroy      = gl_destroy,

    .transform_cull_mode              = gl_transform_cull_mode,
    .transform_projection_matrix      = gl_transform_projection_matrix,
    .get_rendertarget_uvcoord_matrix  = gl_get_rendertarget_uvcoord_matrix,

    .bind_rendertarget         = gl_bind_rendertarget,
    .get_rendertarget         = gl_get_rendertarget,
    .get_default_rendertarget_desc = gl_get_default_rendertarget_desc,
    .set_viewport             = gl_set_viewport,
    .get_viewport             = gl_get_viewport,
    .set_scissor              = gl_set_scissor,
    .get_scissor              = gl_get_scissor,
    .set_clear_color          = gl_set_clear_color,
    .get_clear_color          = gl_get_clear_color,
    .clear_color              = gl_clear_color,
    .clear_depth_stencil      = gl_clear_depth_stencil,
    .invalidate_depth_stencil = gl_invalidate_depth_stencil,
    .get_preferred_depth_format = gl_get_preferred_depth_format,
    .get_preferred_depth_stencil_format = gl_get_preferred_depth_stencil_format,
    .flush                    = gl_flush,

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
    .pipeline_update_attribute = ngli_pipeline_gl_update_attribute,
    .pipeline_update_uniform = ngli_pipeline_gl_update_uniform,
    .pipeline_update_texture = ngli_pipeline_gl_update_texture,
    .pipeline_draw           = ngli_pipeline_gl_draw,
    .pipeline_draw_indexed   = ngli_pipeline_gl_draw_indexed,
    .pipeline_dispatch       = ngli_pipeline_gl_dispatch,
    .pipeline_freep          = ngli_pipeline_gl_freep,

    .program_create = ngli_program_gl_create,
    .program_init   = ngli_program_gl_init,
    .program_freep  = ngli_program_gl_freep,

    .rendertarget_create      = ngli_rendertarget_gl_create,
    .rendertarget_init        = ngli_rendertarget_gl_init,
    .rendertarget_resolve     = ngli_rendertarget_gl_resolve,
    .rendertarget_read_pixels = ngli_rendertarget_gl_read_pixels,
    .rendertarget_freep       = ngli_rendertarget_gl_freep,

    .swapchain_create         = NULL,
    .swapchain_destroy        = NULL,
    .swapchain_acquire_image  = NULL,

    .texture_create           = ngli_texture_gl_create,
    .texture_init             = ngli_texture_gl_init,
    .texture_has_mipmap       = ngli_texture_gl_has_mipmap,
    .texture_match_dimensions = ngli_texture_gl_match_dimensions,
    .texture_upload           = ngli_texture_gl_upload,
    .texture_generate_mipmap  = ngli_texture_gl_generate_mipmap,
    .texture_freep            = ngli_texture_gl_freep,
};
