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

#include "nodegl/log.h"
#include "nodegl/utils.h"
#include "nodegl/porting/gl/format_gl.h"
#include "nodegl/porting/gl/gctx_gl.h"
#include "nodegl/porting/gl/glincludes.h"
#include "nodegl/porting/gl/glcontext.h"
#include "nodegl/memory.h"
#include "nodegl/nodes.h"
#include "nodegl/porting/gl/texture_gl.h"

static const GLint gl_filter_map[NGLI_NB_FILTER][NGLI_NB_MIPMAP] = {
    [NGLI_FILTER_NEAREST] = {
        [NGLI_MIPMAP_FILTER_NONE]    = GL_NEAREST,
        [NGLI_MIPMAP_FILTER_NEAREST] = GL_NEAREST_MIPMAP_NEAREST,
        [NGLI_MIPMAP_FILTER_LINEAR]  = GL_NEAREST_MIPMAP_LINEAR,
    },
    [NGLI_FILTER_LINEAR] = {
        [NGLI_MIPMAP_FILTER_NONE]    = GL_LINEAR,
        [NGLI_MIPMAP_FILTER_NEAREST] = GL_LINEAR_MIPMAP_NEAREST,
        [NGLI_MIPMAP_FILTER_LINEAR]  = GL_LINEAR_MIPMAP_LINEAR,
    },
};

GLint ngli_texture_get_gl_min_filter(int min_filter, int mipmap_filter)
{
    return gl_filter_map[min_filter][mipmap_filter];
}

GLint ngli_texture_get_gl_mag_filter(int mag_filter)
{
    return gl_filter_map[mag_filter][NGLI_MIPMAP_FILTER_NONE];
}

static const GLint gl_wrap_map[NGLI_NB_WRAP] = {
    [NGLI_WRAP_CLAMP_TO_EDGE]   = GL_CLAMP_TO_EDGE,
    [NGLI_WRAP_MIRRORED_REPEAT] = GL_MIRRORED_REPEAT,
    [NGLI_WRAP_REPEAT]          = GL_REPEAT,
};

GLint ngli_texture_get_gl_wrap(int wrap)
{
    return gl_wrap_map[wrap];
}

static const GLenum gl_access_map[NGLI_ACCESS_NB] = {
    [NGLI_ACCESS_READ_BIT]   = GL_READ_ONLY,
    [NGLI_ACCESS_WRITE_BIT]  = GL_WRITE_ONLY,
    [NGLI_ACCESS_READ_WRITE] = GL_READ_WRITE,
};

GLenum ngli_texture_get_gl_access(int access)
{
    return gl_access_map[access];
}

static void texture_set_image(struct texture *s, const uint8_t *data)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    switch (s_priv->target) {
    case GL_TEXTURE_2D:
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, s_priv->internal_format, params->width, params->height, 0, s_priv->format, s_priv->format_type, data);
        break;
    case GL_TEXTURE_3D:
        ngli_glTexImage3D(gl, GL_TEXTURE_3D, 0, s_priv->internal_format, params->width, params->height, params->depth, 0, s_priv->format, s_priv->format_type, data);
        break;
    case GL_TEXTURE_CUBE_MAP: {
        const int face_size = data ? s->bytes_per_pixel * params->width * params->height : 0;
        for (int face = 0; face < 6; face++) {
            ngli_glTexImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, s_priv->internal_format, params->width, params->height, 0, s_priv->format, s_priv->format_type, data);
            data += face_size;
        }
        break;
    }
    }
}

static void texture2d_set_sub_image(struct texture *s, const uint8_t *data, int linesize, int row_upload)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    if (row_upload) {
        for (int y = 0; y < params->height; y++) {
            ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, y, params->width, 1, s_priv->format, s_priv->format_type, data);
            data += linesize * s->bytes_per_pixel;
        }
        return;
    }
    ngli_glTexSubImage2D(gl, GL_TEXTURE_2D, 0, 0, 0, params->width, params->height, s_priv->format, s_priv->format_type, data);
}

static void texture3d_set_sub_image(struct texture *s, const uint8_t *data, int linesize, int row_upload)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    if (row_upload) {
        for (int z = 0; z < params->depth; z++) {
            for (int y = 0; y < params->height; y++) {
                ngli_glTexSubImage3D(gl, GL_TEXTURE_3D, 0, 0, y, z, params->width, 1, params->depth, s_priv->format, s_priv->format_type, data);
                data += linesize * s->bytes_per_pixel;
            }
        }
        return;
    }
    ngli_glTexSubImage3D(gl, GL_TEXTURE_3D, 0, 0, 0, 0, params->width, params->height, params->depth, s_priv->format, s_priv->format_type, data);
}

static void texturecube_set_sub_image(struct texture *s, const uint8_t *data, int linesize, int row_upload)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    if (row_upload) {
        for (int face = 0; face < 6; face++) {
            for (int y = 0; y < params->height; y++) {
                ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 0, y, params->width, 1, s_priv->format, s_priv->format_type, data);
                data += linesize * s->bytes_per_pixel;
            }
        }
        return;
    }
    const int face_size = data ? s->bytes_per_pixel * linesize * params->height : 0;
    for (int face = 0; face < 6; face++) {
        ngli_glTexSubImage2D(gl, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 0, 0, params->width, params->height, s_priv->format, s_priv->format_type, data);
        data += face_size;
    }
}

static void texture_set_sub_image(struct texture *s, const uint8_t *data, int linesize)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    if (!linesize)
        linesize = params->width;

    const int bytes_per_row = linesize * ngli_format_get_bytes_per_pixel(params->format);
    const int alignment = NGLI_MIN(bytes_per_row & ~(bytes_per_row - 1), 8);
    ngli_glPixelStorei(gl, GL_UNPACK_ALIGNMENT, alignment);

    int row_upload = 0;
    if (gl->features & NGLI_FEATURE_ROW_LENGTH)
        ngli_glPixelStorei(gl, GL_UNPACK_ROW_LENGTH, linesize);
    else if (params->width != linesize)
        row_upload = 1;

    switch (s_priv->target) {
    case GL_TEXTURE_2D:
        texture2d_set_sub_image(s, data, linesize, row_upload);
        break;
    case GL_TEXTURE_3D:
        texture3d_set_sub_image(s, data, linesize, row_upload);
        break;
    case GL_TEXTURE_CUBE_MAP:
        texturecube_set_sub_image(s, data, linesize, row_upload);
        break;
    }

    ngli_glPixelStorei(gl, GL_UNPACK_ALIGNMENT, 4);
    if (gl->features & NGLI_FEATURE_ROW_LENGTH)
        ngli_glPixelStorei(gl, GL_UNPACK_ROW_LENGTH, 0);
}

static void texture_set_storage(struct texture *s)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    switch (s_priv->target) {
    case GL_TEXTURE_2D: {
        int mipmap_levels = 1;
        if (ngli_texture_gl_has_mipmap(s))
            while ((params->width | params->height) >> mipmap_levels)
                mipmap_levels += 1;
        ngli_glTexStorage2D(gl, s_priv->target, mipmap_levels, s_priv->internal_format, params->width, params->height);
        break;
    }
    case GL_TEXTURE_3D:
        ngli_glTexStorage3D(gl, s_priv->target, 1, s_priv->internal_format, params->width, params->height, params->depth);
        break;
    case GL_TEXTURE_CUBE_MAP:
        /* glTexStorage2D automatically accomodates for 6 faces when using the cubemap target */
        ngli_glTexStorage2D(gl, s_priv->target, 1, s_priv->internal_format, params->width, params->height);
        break;
    }
}

static int renderbuffer_check_samples(struct texture *s)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct limits *limits = &gl->limits;
    const struct texture_params *params = &s->params;

    int max_samples = limits->max_samples;
    if (gl->features & NGLI_FEATURE_INTERNALFORMAT_QUERY)
        ngli_glGetInternalformativ(gl, GL_RENDERBUFFER, s_priv->format, GL_SAMPLES, 1, &max_samples);

    if (params->samples > max_samples) {
        LOG(WARNING, "renderbuffer format 0x%x does not support samples %d (maximum %d)",
            s_priv->format, params->samples, max_samples);
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

static void renderbuffer_set_storage(struct texture *s)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    if (params->samples > 0)
        ngli_glRenderbufferStorageMultisample(gl, GL_RENDERBUFFER, params->samples, s_priv->format, params->width, params->height);
    else
        ngli_glRenderbufferStorage(gl, GL_RENDERBUFFER, s_priv->format, params->width, params->height);
}

static int texture_init_fields(struct texture *s)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct texture_params *params = &s->params;

    s_priv->is_attachment = (params->usage & NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) ||
        (params->usage & NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if (s_priv->is_attachment) {
        s_priv->target = GL_RENDERBUFFER;
        int ret = ngli_format_get_gl_renderbuffer_format(gl, params->format, &s_priv->format);
        if (ret < 0)
            return ret;
        s_priv->internal_format = s_priv->format;

        ret = renderbuffer_check_samples(s);
        if (ret < 0)
            return ret;
        return 0;
    }

    /* TODO: add multisample support for textures */
    ngli_assert(!params->samples);

    if (params->type == NGLI_TEXTURE_TYPE_2D)
        s_priv->target = GL_TEXTURE_2D;
    else if (params->type == NGLI_TEXTURE_TYPE_3D)
        s_priv->target = GL_TEXTURE_3D;
    else if (params->type == NGLI_TEXTURE_TYPE_CUBE)
        s_priv->target = GL_TEXTURE_CUBE_MAP;
    else
        ngli_assert(0);

    if (params->external_oes) {
        ngli_assert(params->type == NGLI_TEXTURE_TYPE_2D);
        s_priv->target = GL_TEXTURE_EXTERNAL_OES;
    } else if (params->rectangle) {
        ngli_assert(params->type == NGLI_TEXTURE_TYPE_2D);
        s_priv->target = GL_TEXTURE_RECTANGLE;
    }

    int ret = ngli_format_get_gl_texture_format(gl,
                                                params->format,
                                                &s_priv->format,
                                                &s_priv->internal_format,
                                                &s_priv->format_type);
    if (ret < 0)
        return ret;

    s->bytes_per_pixel = ngli_format_get_bytes_per_pixel(params->format);

    if (params->external_storage || params->external_oes)
        s->external_storage = 1;

    return 0;
}

static int is_pow2(int x)
{
    return x && !(x & (x - 1));
}

struct texture *ngli_texture_gl_create(struct gctx *gctx)
{
    struct texture_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct texture *)s;
}

int ngli_texture_gl_init(struct texture *s, const struct texture_params *params)
{
    s->params = *params;

    int ret = texture_init_fields(s);
    if (ret < 0)
        return ret;

    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    if (s_priv->target == GL_RENDERBUFFER) {
        ngli_glGenRenderbuffers(gl, 1, &s_priv->id);
        ngli_glBindRenderbuffer(gl, s_priv->target, s_priv->id);
        renderbuffer_set_storage(s);
    } else {
        ngli_glGenTextures(gl, 1, &s_priv->id);
        ngli_glBindTexture(gl, s_priv->target, s_priv->id);
        if (s->params.mipmap_filter &&
            !(gl->features & NGLI_FEATURE_TEXTURE_NPOT) &&
            (!is_pow2(params->width) || !is_pow2(params->height))) {
            LOG(WARNING, "context does not support non-power of two textures, "
                "mipmapping will be disabled");
            s->params.mipmap_filter = NGLI_MIPMAP_FILTER_NONE;
        }
        const GLint min_filter = ngli_texture_get_gl_min_filter(params->min_filter, s->params.mipmap_filter);
        const GLint mag_filter = ngli_texture_get_gl_mag_filter(params->mag_filter);
        const GLint wrap_s = ngli_texture_get_gl_wrap(params->wrap_s);
        const GLint wrap_t = ngli_texture_get_gl_wrap(params->wrap_t);
        const GLint wrap_r = ngli_texture_get_gl_wrap(params->wrap_r);
        ngli_glTexParameteri(gl, s_priv->target, GL_TEXTURE_MIN_FILTER, min_filter);
        ngli_glTexParameteri(gl, s_priv->target, GL_TEXTURE_MAG_FILTER, mag_filter);
        ngli_glTexParameteri(gl, s_priv->target, GL_TEXTURE_WRAP_S, wrap_s);
        ngli_glTexParameteri(gl, s_priv->target, GL_TEXTURE_WRAP_T, wrap_t);
        if (s_priv->target == GL_TEXTURE_3D || s_priv->target == GL_TEXTURE_CUBE_MAP)
            ngli_glTexParameteri(gl, s_priv->target, GL_TEXTURE_WRAP_R, wrap_r);

        if (!s->external_storage) {
            if (!params->width || !params->height ||
                (params->type == NGLI_TEXTURE_TYPE_3D && !params->depth)) {
                LOG(ERROR, "invalid texture type %dx%dx%d",
                    params->width, params->height, params->depth);
                return NGL_ERROR_INVALID_ARG;
            }
            if (params->immutable) {
                texture_set_storage(s);
            } else {
                texture_set_image(s, NULL);
            }
        }
    }

    return 0;
}

int ngli_texture_gl_wrap(struct texture *s,
                         const struct texture_params *params,
                         GLuint texture)
{
    s->params = *params;

    int ret = texture_init_fields(s);
    if (ret < 0)
        return ret;

    struct texture_gl *s_priv = (struct texture_gl *)s;
    s_priv->id = texture;
    s->wrapped = 1;
    s->external_storage = 1;

    return 0;
}

void ngli_texture_gl_set_id(struct texture *s, GLuint id)
{
    /* only wrapped textures can update their id with this function */
    ngli_assert(s->wrapped);

    struct texture_gl *s_priv = (struct texture_gl *)s;
    s_priv->id = id;
}

void ngli_texture_gl_set_dimensions(struct texture *s, int width, int height, int depth)
{
    /* only textures with external storage can update their dimensions with this function */
    ngli_assert(s->external_storage);

    struct texture_params *params = &s->params;
    params->width = width;
    params->height = height;
    params->depth = depth;
}

int ngli_texture_gl_has_mipmap(const struct texture *s)
{
    return s->params.mipmap_filter != NGLI_MIPMAP_FILTER_NONE;
}

int ngli_texture_gl_match_dimensions(const struct texture *s, int width, int height, int depth)
{
    const struct texture_params *params = &s->params;
    return params->width == width && params->height == height && params->depth == depth;
}

int ngli_texture_gl_upload(struct texture *s, const uint8_t *data, int linesize)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    /* texture with external storage (including wrapped textures and render
     * buffers) cannot update their content with this function */
    ngli_assert(!s->external_storage && !s_priv->is_attachment);

    ngli_glBindTexture(gl, s_priv->target, s_priv->id);
    if (data) {
        texture_set_sub_image(s, data, linesize);
        if (ngli_texture_gl_has_mipmap(s))
            ngli_glGenerateMipmap(gl, s_priv->target);
    }
    ngli_glBindTexture(gl, s_priv->target, 0);

    return 0;
}

int ngli_texture_gl_generate_mipmap(struct texture *s)
{
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    ngli_assert(!s_priv->is_attachment);

    ngli_glBindTexture(gl, s_priv->target, s_priv->id);
    ngli_glGenerateMipmap(gl, s_priv->target);
    return 0;
}

void ngli_texture_gl_freep(struct texture **sp)
{
    if (!*sp)
        return;

    struct texture *s = *sp;
    struct texture_gl *s_priv = (struct texture_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    if (!s->wrapped) {
        if (s_priv->target == GL_RENDERBUFFER)
            ngli_glDeleteRenderbuffers(gl, 1, &s_priv->id);
        else
            ngli_glDeleteTextures(gl, 1, &s_priv->id);
    }

    ngli_freep(sp);
}
