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

#ifndef GCTX_H
#define GCTX_H

#include "nodegl.h"
#include "rendertarget.h"

void ngli_gctx_set_rendertarget(struct ngl_ctx *s, struct rendertarget *rt);
struct rendertarget *ngli_gctx_get_rendertarget(struct ngl_ctx *s);

void ngli_gctx_set_viewport(struct ngl_ctx *s, const int *viewport);
void ngli_gctx_get_viewport(struct ngl_ctx *s, int *viewport);
void ngli_gctx_set_scissor(struct ngl_ctx *s, const int *scissor);
void ngli_gctx_get_scissor(struct ngl_ctx *s, int *scissor);

void ngli_gctx_set_clear_color(struct ngl_ctx *s, const float *color);
void ngli_gctx_get_clear_color(struct ngl_ctx *s, float *color);

void ngli_gctx_clear_color(struct ngl_ctx *s);
void ngli_gctx_clear_depth_stencil(struct ngl_ctx *s);
void ngli_gctx_invalidate_depth_stencil(struct ngl_ctx *s);

#endif
