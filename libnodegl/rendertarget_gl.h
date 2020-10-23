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

#ifndef RENDERTARGET_GL_H
#define RENDERTARGET_GL_H

#include "glincludes.h"
#include "rendertarget.h"

struct rendertarget_gl {
    struct rendertarget parent;
    int wrapped;
    GLuint id;
    GLuint resolve_id;
    GLenum draw_buffers[NGLI_MAX_COLOR_ATTACHMENTS];
    GLenum clear_flags;
    GLenum invalidate_attachments[NGLI_MAX_COLOR_ATTACHMENTS + 2]; // max color attachments + depth and stencil attachments
    int nb_invalidate_attachments;
    void (*clear)(struct rendertarget *s);
    void (*invalidate)(struct rendertarget *s);
    void (*resolve)(struct rendertarget *s);
};

struct rendertarget *ngli_rendertarget_gl_create(struct gctx *gctx);
int ngli_rendertarget_gl_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_gl_resolve(struct rendertarget *s);
void ngli_rendertarget_gl_clear(struct rendertarget *s);
void ngli_rendertarget_gl_invalidate(struct rendertarget *s);
void ngli_rendertarget_gl_read_pixels(struct rendertarget *s, uint8_t *data);
void ngli_rendertarget_gl_freep(struct rendertarget **sp);

int ngli_default_rendertarget_gl_init(struct rendertarget *s, const struct rendertarget_params *params);

#endif
