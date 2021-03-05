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

#include "surface_util_ngfx.h"
#include "gctx_ngfx.h"
#include "debugutil_ngfx.h"
#include "ngfx/porting/metal/MTLGraphicsContext.h"
#include "ngfx/porting/metal/MTLSurface.h"
#import <MetalKit/MetalKit.h>
using namespace ngfx;

ngfx::Surface* surface_util_ngfx::create_surface_from_window_handle(ngfx::GraphicsContext *ctx, 
        int platform, uintptr_t display, uintptr_t window, uintptr_t width, uintptr_t height) {
    ngfx::MTLSurface *mtl_surface = new ngfx::MTLSurface();
    mtl_surface->w = width; mtl_surface->h = height; mtl_surface->offscreen = false;
    mtl_surface->view = (NSView*)window;
    CAMetalLayer *layer = (CAMetalLayer *)mtl_surface->view.layer;
    layer.device = mtl(ctx)->mtlDevice.v;
    return mtl_surface;
}
