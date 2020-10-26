/*
 * Copyright 2020 GoPro Inc.
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

#ifndef GCTX_NGFX_H
#define GCTX_NGFX_H

#include "nodegl/gctx.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Graphics.h"
#include "graphics/CommandBuffer.h"
#include <memory>

struct gctx_ngfx {
    struct gctx parent;
    struct rendertarget_desc default_rendertarget_desc;
    ngfx::GraphicsContext* graphics_context = nullptr;
    ngfx::Graphics* graphics = nullptr;
    ngfx::CommandBuffer* cur_command_buffer = nullptr;
    int viewport[4];
    int scissor[4];
    float clear_color[4];
    struct {
        texture *color_texture = nullptr, *depth_texture = nullptr;
        rendertarget *rt = nullptr;
    } offscreen_resources;
};

#endif
