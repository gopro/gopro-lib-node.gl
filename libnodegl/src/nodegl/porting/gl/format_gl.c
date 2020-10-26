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

#include "nodegl/porting/gl/format_gl.h"
#include "nodegl/core/utils.h"

static int get_gl_format_type(struct glcontext *gl, int data_format,
                              GLint *formatp, GLint *internal_formatp, GLenum *typep)
{
    static const struct entry {
        GLint format;
        GLint internal_format;
        GLenum type;
    } format_map[] = {
        [NGLI_FORMAT_UNDEFINED]            = {0,                  0,                     0},
        [NGLI_FORMAT_R8_UNORM]             = {GL_RED,             GL_R8,                 GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8_SNORM]             = {GL_RED,             GL_R8_SNORM,           GL_BYTE},
        [NGLI_FORMAT_R8_UINT]              = {GL_RED_INTEGER,     GL_R8UI,               GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8_SINT]              = {GL_RED_INTEGER,     GL_R8I,                GL_BYTE},
        [NGLI_FORMAT_R8G8_UNORM]           = {GL_RG,              GL_RG8,                GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8_SNORM]           = {GL_RG,              GL_RG8_SNORM,          GL_BYTE},
        [NGLI_FORMAT_R8G8_UINT]            = {GL_RG_INTEGER,      GL_RG8UI,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8_SINT]            = {GL_RG_INTEGER,      GL_RG8I,               GL_BYTE},
        [NGLI_FORMAT_R8G8B8_UNORM]         = {GL_RGB,             GL_RGB8,               GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8_SNORM]         = {GL_RGB,             GL_RGB8_SNORM,         GL_BYTE},
        [NGLI_FORMAT_R8G8B8_UINT]          = {GL_RGB_INTEGER,     GL_RGB8UI,             GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8_SINT]          = {GL_RGB_INTEGER,     GL_RGB8I,              GL_BYTE},
        [NGLI_FORMAT_R8G8B8_SRGB]          = {GL_RGB,             GL_SRGB8,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8A8_UNORM]       = {GL_RGBA,            GL_RGBA8,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8A8_SNORM]       = {GL_RGBA,            GL_RGBA8_SNORM,        GL_BYTE},
        [NGLI_FORMAT_R8G8B8A8_UINT]        = {GL_RGBA_INTEGER,    GL_RGBA8UI,            GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_R8G8B8A8_SINT]        = {GL_RGBA_INTEGER,    GL_RGBA8I,             GL_BYTE},
        [NGLI_FORMAT_R8G8B8A8_SRGB]        = {GL_RGBA,            GL_SRGB8_ALPHA8,       GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_B8G8R8A8_UNORM]       = {GL_BGRA,            GL_RGBA8,              GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_B8G8R8A8_SNORM]       = {GL_BGRA,            GL_RGBA8_SNORM,        GL_BYTE},
        [NGLI_FORMAT_B8G8R8A8_UINT]        = {GL_BGRA_INTEGER,    GL_RGBA8UI,            GL_UNSIGNED_BYTE},
        [NGLI_FORMAT_B8G8R8A8_SINT]        = {GL_BGRA_INTEGER,    GL_RGBA8I,             GL_BYTE},
        [NGLI_FORMAT_R16_UNORM]            = {GL_RED,             GL_R16,                GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16_SNORM]            = {GL_RED,             GL_R16_SNORM,          GL_SHORT},
        [NGLI_FORMAT_R16_UINT]             = {GL_RED_INTEGER,     GL_R16UI,              GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16_SINT]             = {GL_RED_INTEGER,     GL_R16I,               GL_SHORT},
        [NGLI_FORMAT_R16_SFLOAT]           = {GL_RED,             GL_R16F,               GL_HALF_FLOAT},
        [NGLI_FORMAT_R16G16_UNORM]         = {GL_RG,              GL_RG16,               GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16_SNORM]         = {GL_RG,              GL_RG16_SNORM,         GL_SHORT},
        [NGLI_FORMAT_R16G16_UINT]          = {GL_RG_INTEGER,      GL_RG16UI,             GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16_SINT]          = {GL_RG_INTEGER,      GL_RG16I,              GL_SHORT},
        [NGLI_FORMAT_R16G16_SFLOAT]        = {GL_RG,              GL_RG16F,              GL_HALF_FLOAT},
        [NGLI_FORMAT_R16G16B16_UNORM]      = {GL_RGB,             GL_RGB16,              GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16_SNORM]      = {GL_RGB,             GL_RGB16_SNORM,        GL_SHORT},
        [NGLI_FORMAT_R16G16B16_UINT]       = {GL_RGB_INTEGER,     GL_RGB16UI,            GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16_SINT]       = {GL_RGB_INTEGER,     GL_RGB16I,             GL_SHORT},
        [NGLI_FORMAT_R16G16B16_SFLOAT]     = {GL_RGB,             GL_RGB16F,             GL_HALF_FLOAT},
        [NGLI_FORMAT_R16G16B16A16_UNORM]   = {GL_RGBA,            GL_RGBA16,             GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16A16_SNORM]   = {GL_RGBA,            GL_RGBA16_SNORM,       GL_SHORT},
        [NGLI_FORMAT_R16G16B16A16_UINT]    = {GL_RGBA_INTEGER,    GL_RGBA16UI,           GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_R16G16B16A16_SINT]    = {GL_RGBA_INTEGER,    GL_RGBA16I,            GL_SHORT},
        [NGLI_FORMAT_R16G16B16A16_SFLOAT]  = {GL_RGBA,            GL_RGBA16F,            GL_HALF_FLOAT},
        [NGLI_FORMAT_R32_UINT]             = {GL_RED_INTEGER,     GL_R32UI,              GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32_SINT]             = {GL_RED_INTEGER,     GL_R32I,               GL_INT},
        [NGLI_FORMAT_R32_SFLOAT]           = {GL_RED,             GL_R32F,               GL_FLOAT},
        [NGLI_FORMAT_R32G32_UINT]          = {GL_RG_INTEGER,      GL_RG32UI,             GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32G32_SINT]          = {GL_RG_INTEGER,      GL_RG32I,              GL_INT},
        [NGLI_FORMAT_R32G32_SFLOAT]        = {GL_RG,              GL_RG32F,              GL_FLOAT},
        [NGLI_FORMAT_R32G32B32_UINT]       = {GL_RGB_INTEGER,     GL_RGB32UI,            GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32G32B32_SINT]       = {GL_RGB_INTEGER,     GL_RGB32I,             GL_INT},
        [NGLI_FORMAT_R32G32B32_SFLOAT]     = {GL_RGB,             GL_RGB32F,             GL_FLOAT},
        [NGLI_FORMAT_R32G32B32A32_UINT]    = {GL_RGBA_INTEGER,    GL_RGBA32UI,           GL_UNSIGNED_INT},
        [NGLI_FORMAT_R32G32B32A32_SINT]    = {GL_RGBA_INTEGER,    GL_RGBA32I,            GL_INT},
        [NGLI_FORMAT_R32G32B32A32_SFLOAT]  = {GL_RGBA,            GL_RGBA32F,            GL_FLOAT},
        [NGLI_FORMAT_D16_UNORM]            = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16,  GL_UNSIGNED_SHORT},
        [NGLI_FORMAT_X8_D24_UNORM_PACK32]  = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT24,  GL_UNSIGNED_INT},
        [NGLI_FORMAT_D32_SFLOAT]           = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT32F, GL_FLOAT},
        [NGLI_FORMAT_D24_UNORM_S8_UINT]    = {GL_DEPTH_STENCIL,   GL_DEPTH24_STENCIL8,   GL_UNSIGNED_INT_24_8},
        [NGLI_FORMAT_D32_SFLOAT_S8_UINT]   = {GL_DEPTH_STENCIL,   GL_DEPTH32F_STENCIL8,  GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
        [NGLI_FORMAT_S8_UINT]              = {GL_STENCIL_INDEX,   GL_STENCIL_INDEX8,     GL_UNSIGNED_BYTE},
    };

    ngli_assert(data_format >= 0 && data_format < NGLI_ARRAY_NB(format_map));
    const struct entry *entry = &format_map[data_format];

    ngli_assert(data_format == NGLI_FORMAT_UNDEFINED ||
               (entry->format && entry->internal_format && entry->type));

    if (formatp)
        *formatp = entry->format;
    if (internal_formatp)
        *internal_formatp = entry->internal_format;
    if (typep)
        *typep = entry->type;

    return 0;
}

int ngli_format_get_gl_texture_format(struct glcontext *gl, int data_format,
                                      GLint *formatp, GLint *internal_formatp, GLenum *typep)
{
    GLint format;
    GLint internal_format;
    GLenum type;

    int ret = get_gl_format_type(gl, data_format, &format, &internal_format, &type);
    if (ret < 0)
        return ret;

    if (gl->backend == NGL_BACKEND_OPENGLES && gl->version < 300) {
        if (format == GL_RED)
            format = GL_LUMINANCE;
        else if (format == GL_RG)
            format = GL_LUMINANCE_ALPHA;
        internal_format = format == GL_BGRA ? GL_RGBA : format;
    }

    if (formatp)
        *formatp = format;
    if (internal_formatp)
        *internal_formatp = internal_format;
    if (typep)
        *typep = type;

    return 0;
}

int ngli_format_get_gl_renderbuffer_format(struct glcontext *gl, int data_format, GLint *formatp)
{
    return get_gl_format_type(gl, data_format, NULL, formatp, NULL);
}
