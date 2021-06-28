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
#include "gpu_ctx_ngfx.h"
#include "debugutil_ngfx.h"
#include "ngfx/porting/vulkan/VKGraphicsContext.h"
#if defined(TARGET_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#elif defined(TARGET_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_win32.h>
#endif
using namespace ngfx;

ngfx::Surface* surface_util_ngfx::create_surface_from_window_handle(ngfx::GraphicsContext *ctx, 
        int platform, uintptr_t display, uintptr_t window, uintptr_t width, uintptr_t height) {
    ngfx::Surface *surface = nullptr;
#if defined(TARGET_LINUX)
    if (platform == NGL_PLATFORM_XLIB) {
        ngfx::VKGraphicsContext *vk_ctx = (ngfx::VKGraphicsContext *)ctx;
        ngfx::VKSurface *vk_surface = new ngfx::VKSurface();
        vk_surface->instance = vk_ctx->vkInstance.v;
        VkXlibSurfaceCreateInfoKHR surface_create_info = {
            VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, NULL, 0,
            (Display *)display, window,
        };
        vkCreateXlibSurfaceKHR(vk_ctx->vkInstance.v, &surface_create_info, NULL, &vk_surface->v);
        vk_surface->w = width;
        vk_surface->h = height;
        vk_surface->offscreen = false;
        surface = vk_surface;
    }
#elif defined(TARGET_WINDOWS)
    if (platform == NGL_PLATFORM_WINDOWS) {
        ngfx::VKGraphicsContext *vk_ctx = (ngfx::VKGraphicsContext *)ctx;
        ngfx::VKSurface *vk_surface = new ngfx::VKSurface();
        vk_surface->instance = vk_ctx->vkInstance.v;
        VkWin32SurfaceCreateInfoKHR surface_create_info = {
            VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, NULL, 0,
            GetModuleHandle(NULL), HWND(window),
        };
        vkCreateWin32SurfaceKHR(vk_ctx->vkInstance.v, &surface_create_info, NULL, &vk_surface->v);
        vk_surface->w = width;
        vk_surface->h = height;
        vk_surface->offscreen = false;
        surface = vk_surface;
    }
#endif
    return surface;
}
