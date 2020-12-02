/*
 * Copyright 2017 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <Windows.h>

#include <GL/glcorearb.h>
#include <GL/wglext.h>

#include "config.h"
#include "glcontext.h"
#include "nodegl.h"
#include "log.h"

struct wgl_priv {
    HWND window;
    HDC device_context;
    HGLRC rendering_context;
    HMODULE module;
    PFNWGLCHOOSEPIXELFORMATARBPROC ChoosePixelFormatARB;
    PFNWGLCREATECONTEXTATTRIBSARBPROC CreateContextAttribsARB;
    PFNWGLGETEXTENSIONSSTRINGARBPROC GetExtensionsStringARB;
    PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT;
};

static int wgl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct wgl_priv *wgl = ctx->priv_data;

    wgl->module = LoadLibrary("opengl32.dll");
    if (!wgl->module) {
        LOG(ERROR, "could not load opengl32.dll (%lu)", GetLastError());
        return -1;
    }

    if (ctx->offscreen) {
        wgl->window = CreateWindowA("static", "node.gl", WS_DISABLED, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
        if (!wgl->window) {
            LOG(ERROR, "could not create offscreen window");
            return -1;
        }
    } else {
        wgl->window = (HWND)window;
        if (!wgl->window) {
            LOG(ERROR, "could not retrieve window");
            return -1;
        }
    }

    wgl->device_context = GetDC((HWND)wgl->window);
    if (!wgl->device_context) {
        LOG(ERROR, "could not retrieve device context");
        return -1;
    }

    // windows needs a dummy context to probe extensions
    PIXELFORMATDESCRIPTOR pixel_format_descriptor = {
        .nSize = sizeof(pixel_format_descriptor),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cRedBits = 8,
        .cGreenBits = 8,
        .cBlueBits = 8,
        .cAlphaBits = 8,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
    };
    int pixel_format = ChoosePixelFormat(wgl->device_context, &pixel_format_descriptor);
    if (SetPixelFormat(wgl->device_context, pixel_format, &pixel_format_descriptor) == FALSE) {
        LOG(ERROR, "could not apply default pixel format (%lu)", GetLastError());
        return -1;
    }

    wgl->rendering_context = wglCreateContext(wgl->device_context);
    if (!wgl->rendering_context) {
        LOG(ERROR, "could not create rendering context (%lu)", GetLastError());
        return -1;
    }

    if (wglMakeCurrent(wgl->device_context, wgl->rendering_context) == FALSE) {
        LOG(ERROR, "could not apply current rendering context (%lu)", GetLastError());
        return -1;
    }

    // probe all extensions potentially needed
    static const struct {
        const char *name;
        int offset;
    } extensions[] = {
        {"wglChoosePixelFormatARB", offsetof(struct wgl_priv, ChoosePixelFormatARB)},
        {"wglCreateContextAttribsARB", offsetof(struct wgl_priv, CreateContextAttribsARB)},
        {"wglGetExtensionsStringARB", offsetof(struct wgl_priv, GetExtensionsStringARB)},
    };

    for (int i = 0; i < NGLI_ARRAY_NB(extensions); i++) {
        const int offset = extensions[i].offset;
        const char *name = extensions[i].name;
        void *function_ptr = wglGetProcAddress(name);
        if (!function_ptr) {
            LOG(ERROR, "could not retrieve %s()", name);
            return -1;
        }

        memcpy(((uint8_t *)wgl) + offset, &function_ptr, sizeof(function_ptr));
    }

    wgl->SwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (!wgl->SwapIntervalEXT)
        LOG(WARNING, "context does not support any swap interval extension (%lu)", GetLastError());

    const int pixel_format_attributes[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB, ctx->offscreen ? 0 : (ctx->samples > 0),
        WGL_SAMPLES_ARB, ctx->offscreen ? 0 : ctx->samples,
        0
    };

    UINT pixel_format_count;
    if (wgl->ChoosePixelFormatARB(wgl->device_context, pixel_format_attributes, NULL, 1, &pixel_format, &pixel_format_count) == FALSE) {
        LOG(ERROR, "could not choose proper pixel format (%lu)", GetLastError());
        return -1;
    }
    DescribePixelFormat(wgl->device_context, pixel_format, sizeof(PIXELFORMATDESCRIPTOR), &pixel_format_descriptor);

    if (SetPixelFormat(wgl->device_context, pixel_format, &pixel_format_descriptor) == FALSE) {
        LOG(ERROR, "could not apply pixel format (%lu)", GetLastError());
        return -1;
    }

    if (wglDeleteContext(wgl->rendering_context) == FALSE)
        LOG(WARNING, "failed to delete dummy rendering context (%lu)", GetLastError());

    HGLRC shared_context = (HGLRC)other;

    if (ctx->backend == NGL_BACKEND_OPENGL) {
        static const int context_attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 1,
            WGL_CONTEXT_MINOR_VERSION_ARB, 0,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#if DEBUG_GL
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
            0
        };
        wgl->rendering_context = wgl->CreateContextAttribsARB(wgl->device_context, shared_context, context_attributes);
    } else if (ctx->backend == NGL_BACKEND_OPENGLES) {
        const char *extensions = wgl->GetExtensionsStringARB(wgl->device_context);
        if (!ngli_glcontext_check_extension("WGL_EXT_create_context_es2_profile", extensions) &&
            !ngli_glcontext_check_extension("WGL_EXT_create_context_es_profile", extensions)) {
            LOG(ERROR, "OpenGLES is not supported by this device");
            return -1;
        }
        static const int context_attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
            WGL_CONTEXT_MINOR_VERSION_ARB, 0,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_ES2_PROFILE_BIT_EXT,
            0
        };
        wgl->rendering_context = wgl->CreateContextAttribsARB(wgl->device_context, shared_context, context_attributes);
    } else {
        ngli_assert(0);
    }

    if (!wgl->rendering_context) {
        LOG(ERROR, "failed to create rendering context (%lu)", GetLastError());
        return -1;
    }

    if (wglMakeCurrent(wgl->device_context, wgl->rendering_context) == FALSE) {
        LOG(ERROR, "could not apply current rendering context (%lu)", GetLastError());
        return -1;
    }

    return 0;
}

static void wgl_uninit(struct glcontext *ctx)
{
    struct wgl_priv *wgl = ctx->priv_data;

    if (wgl->rendering_context)
        wglDeleteContext(wgl->rendering_context);

    if (ctx->offscreen && wgl->window)
        DestroyWindow(wgl->window);

    if (wgl->module)
        FreeLibrary(wgl->module);
}

static int wgl_resize(struct glcontext *ctx, int width, int height)
{
    struct wgl_priv *wgl = ctx->priv_data;

    RECT rect;
    if (GetWindowRect(wgl->window, &rect) == 0)
        return -1;

    ctx->width = rect.right-rect.left;
    ctx->height = rect.bottom-rect.top;

    return 0;
}

static int wgl_make_current(struct glcontext *ctx, int current)
{
    struct wgl_priv *wgl = ctx->priv_data;

    if (wglMakeCurrent(wgl->device_context, current ? wgl->rendering_context : NULL) == FALSE)
        return -1;

    return 0;
}

static void wgl_swap_buffers(struct glcontext *ctx)
{
    struct wgl_priv *wgl = ctx->priv_data;
    SwapBuffers(wgl->device_context);
}

static int wgl_set_swap_interval(struct glcontext *ctx, int interval)
{
    struct wgl_priv *wgl = ctx->priv_data;

    if (!wgl->SwapIntervalEXT) {
        LOG(WARNING, "context does not support swap interval operation");
        return -1;
    }

    if (wgl->SwapIntervalEXT(interval) == FALSE) {
        LOG(ERROR, "context failed to apply swap interval (%lu)", GetLastError());
        return -1;
    }

    return 0;
}

static void *wgl_get_proc_address(struct glcontext *ctx, const char *name)
{
    struct wgl_priv *wgl = ctx->priv_data;

    void *addr = GetProcAddress(wgl->module, name);
    if (!addr)
        addr = wglGetProcAddress((LPCSTR)name);

    return addr;
}

static uintptr_t wgl_get_handle(struct glcontext *ctx)
{
    struct wgl_priv *wgl = ctx->priv_data;
    return (uintptr_t)wgl->rendering_context;
}

const struct glcontext_class ngli_glcontext_wgl_class = {
    .init = wgl_init,
    .uninit = wgl_uninit,
    .resize = wgl_resize,
    .make_current = wgl_make_current,
    .swap_buffers = wgl_swap_buffers,
    .set_swap_interval = wgl_set_swap_interval,
    .get_proc_address = wgl_get_proc_address,
    .get_handle = wgl_get_handle,
    .priv_size = sizeof(struct wgl_priv),
};
