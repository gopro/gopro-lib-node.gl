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

#ifndef GCTX_GL_H
#define GCTX_GL_H

#if defined(TARGET_IPHONE)
#include <CoreVideo/CoreVideo.h>
#endif

#include "nodegl/nodegl.h"
#include "nodegl/porting/gl/glstate.h"
#include "nodegl/graphicstate.h"
#include "nodegl/rendertarget.h"
#include "nodegl/pgcache.h"
#include "nodegl/pipeline.h"
#include "nodegl/gtimer.h"
#include "nodegl/gctx.h"

struct ngl_ctx;
struct rendertarget;

typedef void (*capture_func_type)(struct gctx *s);

struct gctx_gl {
    struct gctx parent;
    struct glcontext *glcontext;
    struct glstate glstate;
    struct rendertarget *rendertarget;
    struct graphicstate default_graphicstate;
    struct rendertarget_desc default_rendertarget_desc;
    int viewport[4];
    int scissor[4];
    float clear_color[4];
    int timer_active;
    /* Offscreen render target */
    struct rendertarget *rt;
    struct texture *color;
    struct texture *ms_color;
    struct texture *depth;
    /* Capture offscreen render target */
    capture_func_type capture_func;
#if defined(TARGET_IPHONE)
    CVPixelBufferRef capture_cvbuffer;
    CVOpenGLESTextureRef capture_cvtexture;
#endif
};

#endif
