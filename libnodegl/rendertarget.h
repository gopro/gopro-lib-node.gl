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

enum {
    NGLI_LOAD_OP_LOAD,
    NGLI_LOAD_OP_CLEAR,
    NGLI_LOAD_OP_DONT_CARE,
};

enum {
    NGLI_STORE_OP_STORE,
    NGLI_STORE_OP_DONT_CARE,
};

struct attachment_desc {
    int format;
    int resolve;
};

struct rendertarget_desc {
    int samples;
    int nb_colors;
    struct attachment_desc colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct attachment_desc depth_stencil;
};

struct attachment {
    struct texture *attachment;
    int attachment_layer;
    struct texture *resolve_target;
    int resolve_target_layer;
    int load_op;
    float clear_value[4];
    int store_op;
};

struct rendertarget_params {
    int width;
    int height;
    int nb_colors;
    struct attachment colors[NGLI_MAX_COLOR_ATTACHMENTS];
    struct attachment depth_stencil;
    int readable;
};

struct rendertarget {
    struct gpu_ctx *gpu_ctx;
    struct rendertarget_params params;
    int width;
    int height;
};

struct rendertarget *ngli_rendertarget_create(struct gpu_ctx *gpu_ctx);
int ngli_rendertarget_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_read_pixels(struct rendertarget *s, uint8_t *data);
void ngli_rendertarget_freep(struct rendertarget **sp);

#endif
