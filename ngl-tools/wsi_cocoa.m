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

#include <Cocoa/Cocoa.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <nodegl.h>

#include "wsi.h"

int wsi_set_ngl_config(struct ngl_config *config, SDL_Window *window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        fprintf(stderr, "Failed to get window WM information\n");
        return -1;
    }

    if (info.subsystem == SDL_SYSWM_COCOA) {
        NSWindow *nswindow = info.info.cocoa.window;
        NSView *view = [nswindow contentView];

        // HACK: should be in libnodegl?
        NSBundle *bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
        view.layer = [[bundle classNamed:@"CAMetalLayer"] layer];

        config->platform = NGL_PLATFORM_MACOS;
        config->window = (uintptr_t)view;
        return 0;
    }

    return -1;
}
