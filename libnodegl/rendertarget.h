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

#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "darray.h"
#include "texture.h"

#define NGLI_MAX_COLOR_ATTACHMENTS 8

struct attachment_desc {
    int format;
    int samples;
    int resolve;
};

struct rendertarget_desc {
    int nb_colors;
    struct attachment_desc colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct attachment_desc depth_stencil;
};

struct attachment {
    struct texture *attachment;
    int attachment_layer;
    struct texture *resolve_target;
    int resolve_target_layer;
};

struct rendertarget_params {
    int width;
    int height;
    int samples;
    int nb_colors;
    struct attachment colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct attachment depth_stencil;
    int readable;
};

struct rendertarget {
    struct gctx *gctx;
    struct rendertarget_params params;
    int width;
    int height;
    int nb_color_attachments;
    int nb_resolve_color_attachments;
};

struct rendertarget *ngli_rendertarget_create(struct gctx *gctx);
int ngli_rendertarget_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_resolve(struct rendertarget *s);
void ngli_rendertarget_read_pixels(struct rendertarget *s, uint8_t *data);
void ngli_rendertarget_freep(struct rendertarget **sp);

#endif
