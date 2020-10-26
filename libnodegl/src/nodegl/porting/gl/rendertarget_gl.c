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

#include "nodegl/porting/gl/rendertarget_gl.h"
#include "nodegl/core/format.h"
#include "nodegl/porting/gl/gctx_gl.h"
#include "nodegl/porting/gl/glcontext.h"
#include "nodegl/porting/gl/glincludes.h"
#include "nodegl/core/log.h"
#include "nodegl/core/memory.h"
#include "nodegl/core/nodes.h"
#include "nodegl/porting/gl/texture_gl.h"
#include "nodegl/core/utils.h"

static GLenum get_gl_attachment_index(GLenum format)
{
    switch (format) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
        return GL_DEPTH_ATTACHMENT;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
        return GL_DEPTH_STENCIL_ATTACHMENT;
    case GL_STENCIL_INDEX:
    case GL_STENCIL_INDEX8:
        return GL_STENCIL_ATTACHMENT;
    default:
        return GL_COLOR_ATTACHMENT0;
    }
}

static void resolve_no_draw_buffers(struct rendertarget *s)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    const int flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    ngli_glBlitFramebuffer(gl, 0, 0, s->width, s->height, 0, 0, s->width, s->height, flags, GL_NEAREST);
}

static void resolve_draw_buffers(struct rendertarget *s)
{
    struct rendertarget_gl *s_priv = (struct rendertarget_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct rendertarget_params *params = &s->params;

    const GLenum *draw_buffers = s_priv->blit_draw_buffers;

    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        if (!attachment->resolve_target)
            continue;
        GLbitfield flags = GL_COLOR_BUFFER_BIT;
        if (i == 0)
            flags |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        ngli_glReadBuffer(gl, GL_COLOR_ATTACHMENT0 + i);
#if defined(TARGET_DARWIN)
        ngli_glDrawBuffer(gl, GL_COLOR_ATTACHMENT0 + i);
#else
        ngli_glDrawBuffers(gl, i + 1, draw_buffers);
#endif
        draw_buffers += i + 1;
        ngli_glBlitFramebuffer(gl, 0, 0, s->width, s->height, 0, 0, s->width, s->height, flags, GL_NEAREST);
    }
    ngli_glReadBuffer(gl, GL_COLOR_ATTACHMENT0);
    ngli_glDrawBuffers(gl, s->nb_color_attachments, s_priv->draw_buffers);
}

static int create_fbo(struct rendertarget *s, int resolve)
{
    int ret = -1;
    struct rendertarget_gl *s_priv = (struct rendertarget_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct limits *limits = &gl->limits;
    const struct rendertarget_params *params = &s->params;

    GLuint id = 0;
    int nb_color_attachments = 0;

    ngli_glGenFramebuffers(gl, 1, &id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, id);

    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        const struct texture *texture = resolve ? attachment->resolve_target : attachment->attachment;
        const int layer = resolve ? attachment->resolve_target_layer : attachment->attachment_layer;

        if (!texture)
            continue;

        const struct texture_gl *texture_gl = (const struct texture_gl *)texture;
        GLenum attachment_index = get_gl_attachment_index(texture_gl->format);
        ngli_assert(attachment_index == GL_COLOR_ATTACHMENT0);

        if (nb_color_attachments >= limits->max_color_attachments) {
            LOG(ERROR, "could not attach color buffer %d (maximum %d)",
                nb_color_attachments, limits->max_color_attachments);
            goto fail;
        }
        attachment_index = attachment_index + nb_color_attachments++;

        switch (texture_gl->target) {
        case GL_RENDERBUFFER:
            ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, texture_gl->id);
            break;
        case GL_TEXTURE_2D:
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, texture_gl->id, 0);
            break;
        case GL_TEXTURE_CUBE_MAP:
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index++, GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, texture_gl->id, 0);
            break;
        default:
            ngli_assert(0);
        }
    }

    const struct attachment *attachment = &params->depth_stencil;
    struct texture *texture = resolve ? attachment->resolve_target : attachment->attachment;
    if (texture) {
        const struct texture_gl *texture_gl = (const struct texture_gl *)texture;
        const GLenum attachment_index = get_gl_attachment_index(texture_gl->format);
        ngli_assert(attachment_index != GL_COLOR_ATTACHMENT0);

        switch (texture_gl->target) {
        case GL_RENDERBUFFER:
            if (gl->backend == NGL_BACKEND_OPENGLES && gl->version < 300 && attachment_index == GL_DEPTH_STENCIL_ATTACHMENT) {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, texture_gl->id);
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, texture_gl->id);
            } else {
                ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, attachment_index, GL_RENDERBUFFER, texture_gl->id);
            }
            break;
        case GL_TEXTURE_2D:
            ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, attachment_index, GL_TEXTURE_2D, texture_gl->id, 0);
            break;
        default:
            ngli_assert(0);
        }
    }

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", id);
        goto fail;
    }

    if (resolve) {
        s_priv->resolve_id = id;
        s->nb_resolve_color_attachments = nb_color_attachments;
    } else {
        s_priv->id = id;
        s->nb_color_attachments = nb_color_attachments;
    }

    return 0;

fail:
    ngli_glDeleteFramebuffers(gl, 1, &id);
    return ret;
}

static int require_resolve_fbo(struct rendertarget *s)
{
    const struct rendertarget_params *params = &s->params;
    for (int i = 0; i < params->nb_colors; i++) {
        const struct attachment *attachment = &params->colors[i];
        if (attachment->resolve_target)
            return 1;
    }
    return 0;
}

struct rendertarget *ngli_rendertarget_gl_create(struct gctx *gctx)
{
    struct rendertarget_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct rendertarget *)s;
}

int ngli_rendertarget_gl_init(struct rendertarget *s, const struct rendertarget_params *params)
{
    struct rendertarget_gl *s_priv = (struct rendertarget_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct limits *limits = &gl->limits;

    s->params = *params;
    s->width = params->width;
    s->height = params->height;

    int ret;
    if (require_resolve_fbo(s)) {
        ret = create_fbo(s, 1);
        if (ret < 0)
            goto done;
    }

    ret = create_fbo(s, 0);
    if (ret < 0)
        goto done;

    s_priv->resolve = resolve_no_draw_buffers;
    if (gl->features & NGLI_FEATURE_DRAW_BUFFERS) {
        if (s->nb_color_attachments > limits->max_draw_buffers) {
            LOG(ERROR, "draw buffer count (%d) exceeds driver limit (%d)",
                s->nb_color_attachments, limits->max_draw_buffers);
            ret = NGL_ERROR_UNSUPPORTED;
            goto done;
        }
        if (s->nb_color_attachments > 1) {
            for (int i = 0; i < s->nb_color_attachments; i++)
                s_priv->draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
            ngli_glDrawBuffers(gl, s->nb_color_attachments, s_priv->draw_buffers);

            GLenum *draw_buffers = s_priv->blit_draw_buffers;
            for (int i = 0; i < s->nb_color_attachments; i++) {
                draw_buffers += i + 1;
                draw_buffers[-1] = GL_COLOR_ATTACHMENT0 + i;
            }

            s_priv->resolve = resolve_draw_buffers;
        }
    }

done:;
    struct rendertarget *rt = gctx_gl->rendertarget;
    struct rendertarget_gl *rt_gl = (struct rendertarget_gl *)rt;
    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);

    return ret;
}

void ngli_rendertarget_gl_resolve(struct rendertarget *s)
{
    const struct rendertarget_gl *s_priv = (const struct rendertarget_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT))
        return;

    if (!s_priv->resolve_id)
        return;

    ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, s_priv->id);
    ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, s_priv->resolve_id);
    s_priv->resolve(s);

    struct rendertarget *rt = gctx_gl->rendertarget;
    struct rendertarget_gl *rt_gl = (struct rendertarget_gl *)rt;
    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);
}

void ngli_rendertarget_gl_read_pixels(struct rendertarget *s, uint8_t *data)
{
    const struct rendertarget_gl *s_priv = (struct rendertarget_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct rendertarget *rt = gctx_gl->rendertarget;
    struct rendertarget_gl *rt_gl = (struct rendertarget_gl *)rt;

    const GLuint fbo_id = rt_gl ? rt_gl->id : ngli_glcontext_get_default_framebuffer(gl);
    const GLuint id = s_priv->resolve_id ? s_priv->resolve_id : s_priv->id;
    if (id != fbo_id)
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, id);

    ngli_glReadPixels(gl, 0, 0, s->width, s->height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    if (id != fbo_id)
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, fbo_id);
}

void ngli_rendertarget_gl_freep(struct rendertarget **sp)
{
    if (!*sp)
        return;

    struct rendertarget *s = *sp;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct rendertarget_gl *s_priv = (struct rendertarget_gl *)s;
    ngli_glDeleteFramebuffers(gl, 1, &s_priv->id);
    ngli_glDeleteFramebuffers(gl, 1, &s_priv->resolve_id);

    ngli_freep(sp);
}
