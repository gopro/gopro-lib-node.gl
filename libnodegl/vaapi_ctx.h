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

#ifndef VAAPI_H
#define VAAPI_H

#include "config.h"

#if defined(HAVE_VAAPI_X11)
#include <X11/Xlib.h>
#endif

#if defined(HAVE_VAAPI_WAYLAND)
#include <wayland-client.h>
#endif

#include <va/va.h>

struct gctx;

struct vaapi_ctx {
#if defined(HAVE_VAAPI_X11)
    Display *x11_display;
#endif
#if defined(HAVE_VAAPI_WAYLAND)
    struct wl_display *wl_display;
#endif
    VADisplay va_display;
    int va_version;
};

int ngli_vaapi_ctx_init(struct gctx *gctx, struct vaapi_ctx *s);
void ngli_vaapi_ctx_reset(struct vaapi_ctx *vaapi_ctx);

#endif
