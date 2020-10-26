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

#ifndef RENDERTARGET_NGFX_H
#define RENDERTARGET_NGFX_H

#include "nodegl/rendertarget.h"
#include "graphics/Framebuffer.h"

struct rendertarget_ngfx {
    struct rendertarget parent;
    ngfx::RenderPass *render_pass = nullptr;
    ngfx::Framebuffer *output_framebuffer = nullptr;
};

struct rendertarget *ngli_rendertarget_ngfx_create(struct gctx *gctx);
int ngli_rendertarget_ngfx_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_ngfx_resolve(struct rendertarget *s);
void ngli_rendertarget_ngfx_read_pixels(struct rendertarget *s, uint8_t *data);
void ngli_rendertarget_ngfx_begin_pass(struct rendertarget *s);
void ngli_rendertarget_ngfx_end_pass(struct rendertarget *s);
void ngli_rendertarget_ngfx_freep(struct rendertarget **sp);

#endif
