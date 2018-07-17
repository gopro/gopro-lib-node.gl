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

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct rtt, x)
static const struct node_param rtt_params[] = {
    {"child",         PARAM_TYPE_NODE, OFFSET(child),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_texture` and optionally to `depth_texture`")},
    {"color_texture", PARAM_TYPE_NODE, OFFSET(color_texture),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
                      .desc=NGLI_DOCSTRING("destination color texture")},
    {"depth_texture", PARAM_TYPE_NODE, OFFSET(depth_texture),
                      .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
                      .desc=NGLI_DOCSTRING("destination depth (and potentially combined stencil) texture")},
    {"samples",       PARAM_TYPE_INT, OFFSET(samples),
                      .desc=NGLI_DOCSTRING("number of samples used for multisampling anti-aliasing")},
    {NULL}
};

#ifdef VULKAN_BACKEND
// TODO
#else
static GLenum get_depth_attachment(GLenum format)
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
    default:
        return GL_INVALID_ENUM;
    }
}

static int rtt_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct rtt *s = node->priv_data;
    struct texture *texture = s->color_texture->priv_data;
    struct texture *depth_texture = NULL;

    s->width = texture->width;
    s->height = texture->height;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) &&
        s->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (!(gl->features & NGLI_FEATURE_PACKED_DEPTH_STENCIL) &&
        s->samples > 0) {
        LOG(WARNING, "context does not support packed depth stencil feature, multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (s->depth_texture) {
        depth_texture = s->depth_texture->priv_data;
        if (s->width != depth_texture->width || s->height != depth_texture->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, depth_texture->width, depth_texture->height);
            return -1;
        }
    }

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

    ngli_glGenFramebuffers(gl, 1, &s->framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);

    LOG(VERBOSE, "init rtt with texture %d", texture->id);
    ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);

    GLenum depth_format = 0;
    GLenum depth_attachment = 0;
    int packed_depth_stencil = gl->features & NGLI_FEATURE_PACKED_DEPTH_STENCIL;

    if (depth_texture) {
        depth_format = depth_texture->internal_format;
        depth_attachment = get_depth_attachment(depth_format);
        if (!packed_depth_stencil && depth_attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            LOG(ERROR, "context does not support packed depth stencil feature");
            return -1;
        }
        ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, depth_attachment, GL_TEXTURE_2D, depth_texture->id, 0);
    } else {
        if (packed_depth_stencil) {
            depth_format = GL_DEPTH24_STENCIL8;
            depth_attachment = GL_DEPTH_STENCIL_ATTACHMENT;
            ngli_glGenRenderbuffers(gl, 1, &s->renderbuffer_id);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, s->renderbuffer_id);
            ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, depth_format, s->width, s->height);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
            ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, depth_attachment, GL_RENDERBUFFER, s->renderbuffer_id);
        } else {
            ngli_glGenRenderbuffers(gl, 1, &s->renderbuffer_id);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, s->renderbuffer_id);
            ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, s->width, s->height);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
            ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s->renderbuffer_id);

            ngli_glGenRenderbuffers(gl, 1, &s->stencilbuffer_id);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, s->stencilbuffer_id);
            ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, GL_STENCIL_INDEX8, s->width, s->height);
            ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
            ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s->stencilbuffer_id);
        }
    }

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", s->framebuffer_id);
        return -1;
    }

    if (s->samples > 0) {
        if (gl->features & NGLI_FEATURE_INTERNALFORMAT_QUERY) {
            GLint cbuffer_samples;
            ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, texture->internal_format, GL_SAMPLES, 1, &cbuffer_samples);
            GLint dbuffer_samples;
            ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, depth_format, GL_SAMPLES, 1, &dbuffer_samples);

            GLint samples = NGLI_MIN(cbuffer_samples, dbuffer_samples);
            if (s->samples > samples) {
                LOG(WARNING,
                    "requested samples (%d) exceed renderbuffer's maximum supported value (%d)",
                    s->samples,
                    samples);
                s->samples = samples;
            }
        }

        ngli_glGenFramebuffers(gl, 1, &s->framebuffer_ms_id);
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_ms_id);

        ngli_glGenRenderbuffers(gl, 1, &s->colorbuffer_ms_id);
        ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, s->colorbuffer_ms_id);
        ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, s->samples, texture->internal_format, s->width, s->height);
        ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
        ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s->colorbuffer_ms_id);

        ngli_glGenRenderbuffers(gl, 1, &s->depthbuffer_ms_id);
        ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, s->depthbuffer_ms_id);
        ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, s->samples, depth_format, s->width, s->height);
        ngli_glBindRenderbuffer(gl, GL_RENDERBUFFER, 0);
        ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, depth_attachment, GL_RENDERBUFFER, s->depthbuffer_ms_id);

        if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "multisampled framebuffer %u is not complete", s->framebuffer_id);
            return -1;
        }
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);

    /* flip vertically the color and depth textures so the coordinates match
     * how the uv coordinates system works */
    texture->coordinates_matrix[5] = -1.0f;
    texture->coordinates_matrix[13] = 1.0f;

    if (depth_texture) {
        depth_texture->coordinates_matrix[5] = -1.0f;
        depth_texture->coordinates_matrix[13] = 1.0f;
    }

    return 0;
}

static int rtt_update(struct ngl_node *node, double t)
{
    struct rtt *s = node->priv_data;
    int ret = ngli_node_update(s->child, t);
    if (ret < 0)
        return ret;
    return ngli_node_update(s->color_texture, t);
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    GLint viewport[4];
    struct rtt *s = node->priv_data;

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

    if (s->samples > 0)
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_ms_id);
    else
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);

    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, s->width, s->height);
    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ngli_node_draw(s->child);

    if (ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR, "framebuffer %u is not complete", s->framebuffer_id);
        return;
    }

    if (s->samples > 0) {
        ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, s->framebuffer_ms_id);
        ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, s->framebuffer_id);
        ngli_glBlitFramebuffer(gl, 0, 0, s->width, s->height, 0, 0, s->width, s->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);

    struct texture *texture = s->color_texture->priv_data;
    switch(texture->min_filter) {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_LINEAR:
        ngli_glBindTexture(gl, GL_TEXTURE_2D, texture->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
        break;
    }

    texture->coordinates_matrix[5] = -1.0f;
    texture->coordinates_matrix[13] = 1.0f;

    if (s->depth_texture) {
        struct texture *depth_texture = s->depth_texture->priv_data;
        depth_texture->coordinates_matrix[5] = -1.0f;
        depth_texture->coordinates_matrix[13] = 1.0f;
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct rtt *s = node->priv_data;

    GLuint framebuffer_id = 0;
    ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);
    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);
    ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);

    ngli_glDeleteRenderbuffers(gl, 1, &s->renderbuffer_id);
    ngli_glDeleteRenderbuffers(gl, 1, &s->stencilbuffer_id);
    ngli_glDeleteFramebuffers(gl, 1, &s->framebuffer_id);

    if (s->samples > 0) {
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_ms_id);
        ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
        ngli_glFramebufferRenderbuffer(gl, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);

        ngli_glDeleteFramebuffers(gl, 1, &s->framebuffer_ms_id);
        ngli_glDeleteRenderbuffers(gl, 1, &s->colorbuffer_ms_id);
        ngli_glDeleteRenderbuffers(gl, 1, &s->depthbuffer_ms_id);
    }

    ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
}
#endif

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RENDERTOTEXTURE,
    .name      = "RenderToTexture",
#ifndef VULKAN_BACKEND
    .prefetch  = rtt_prefetch,
    .update    = rtt_update,
    .draw      = rtt_draw,
    .release   = rtt_release,
#endif
    .priv_size = sizeof(struct rtt),
    .params    = rtt_params,
    .file      = __FILE__,
};
