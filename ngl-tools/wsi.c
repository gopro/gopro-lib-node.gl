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

#include <stdio.h>
#include <stdint.h>

#include <SDL.h>

#include "wsi.h"


int init_window(void)
{
#ifdef SDL_MAIN_HANDLED
    SDL_SetMainReady();
#endif
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Failed to initialize SDL");
        return -1;
    }

    return 0;
}

SDL_Window *get_window(const char *title, int width, int height)
{
    uint32_t flags = SDL_WINDOW_RESIZABLE;

    /*
     * Workaround an issue with the SDL Wayland video driver. If
     * SDL_WINDOW_VULKAN is not set, SDL automatically adds the
     * SDL_WINDOW_OPENGL flag internally, causing the Wayland backend to create
     * a Wayland EGL surface, an EGL context and expects the user to call
     * SDL_*_SwapWindow(). This conflicts with what we want to do (ie: manage
     * the underlying GPU buffers in node.gl). Adding the SDL_WINDOW_VULKAN
     * flag workarounds the issue and fixes resizing issues on Wayland.
     */
    const char *name = SDL_GetCurrentVideoDriver();
    if (name && !strcmp(name, "wayland"))
        flags |= SDL_WINDOW_VULKAN;

    SDL_Window *window = SDL_CreateWindow(title,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          width,
                                          height,
                                          flags);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        return NULL;
    }

    return window;
}
