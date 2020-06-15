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

#ifndef TEXTURE_H
#define TEXTURE_H

#include "glincludes.h"
#include "utils.h"

struct gctx;

enum {
    NGLI_MIPMAP_FILTER_NONE,
    NGLI_MIPMAP_FILTER_NEAREST,
    NGLI_MIPMAP_FILTER_LINEAR,
    NGLI_NB_MIPMAP
};

enum {
    NGLI_FILTER_NEAREST,
    NGLI_FILTER_LINEAR,
    NGLI_NB_FILTER
};

GLint ngli_texture_get_gl_min_filter(int min_filter, int mipmap_filter);
GLint ngli_texture_get_gl_mag_filter(int mag_filter);

enum {
    NGLI_WRAP_CLAMP_TO_EDGE,
    NGLI_WRAP_MIRRORED_REPEAT,
    NGLI_WRAP_REPEAT,
    NGLI_NB_WRAP
};

GLint ngli_texture_get_gl_wrap(int wrap);

enum {
    NGLI_ACCESS_UNDEFINED,
    NGLI_ACCESS_READ_BIT,
    NGLI_ACCESS_WRITE_BIT,
    NGLI_ACCESS_READ_WRITE,
    NGLI_ACCESS_NB
};

NGLI_STATIC_ASSERT(texture_access, (NGLI_ACCESS_READ_BIT | NGLI_ACCESS_WRITE_BIT) == NGLI_ACCESS_READ_WRITE);

GLenum ngli_texture_get_gl_access(int access);

#define NGLI_TEXTURE_PARAM_DEFAULTS {          \
    .type = NGLI_TEXTURE_TYPE_2D,              \
    .format = NGLI_FORMAT_UNDEFINED,           \
    .min_filter = NGLI_FILTER_NEAREST,         \
    .mag_filter = NGLI_FILTER_NEAREST,         \
    .mipmap_filter = NGLI_MIPMAP_FILTER_NONE,  \
    .wrap_s = NGLI_WRAP_CLAMP_TO_EDGE,         \
    .wrap_t = NGLI_WRAP_CLAMP_TO_EDGE,         \
    .wrap_r = NGLI_WRAP_CLAMP_TO_EDGE,         \
    .access = NGLI_ACCESS_READ_WRITE           \
}

#define NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY (1 << 0)

enum texture_type {
    NGLI_TEXTURE_TYPE_2D,
    NGLI_TEXTURE_TYPE_3D,
    NGLI_TEXTURE_TYPE_CUBE,
};

struct texture_params {
    enum texture_type type;
    int format;
    int width;
    int height;
    int depth;
    int samples;
    int min_filter;
    int mag_filter;
    int mipmap_filter;
    int wrap_s;
    int wrap_t;
    int wrap_r;
    int access;
    int immutable;
    int usage;
    int external_storage;
    int external_oes;
    int rectangle;
};

struct texture {
    struct gctx *gctx;
    struct texture_params params;
    int wrapped;
    int external_storage;
    int bytes_per_pixel;

    GLenum target;
    GLuint id;
    GLint format;
    GLint internal_format;
    GLenum format_type;
};

struct texture *ngli_texture_create(struct gctx *gctx);

int ngli_texture_init(struct texture *s,
                      const struct texture_params *params);

int ngli_texture_wrap(struct texture *s,
                      const struct texture_params *params,
                      GLuint id);

void ngli_texture_set_id(struct texture *s, GLuint id);
void ngli_texture_set_dimensions(struct texture *s, int width, int height, int depth);

int ngli_texture_has_mipmap(const struct texture *s);
int ngli_texture_match_dimensions(const struct texture *s, int width, int height, int depth);

int ngli_texture_upload(struct texture *s, const uint8_t *data, int linesize);
int ngli_texture_generate_mipmap(struct texture *s);

void ngli_texture_freep(struct texture **sp);

#endif
