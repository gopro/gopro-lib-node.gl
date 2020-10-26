/*
 * Copyright 2016 GoPro Inc.
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
#include <EGL/egl.h>
#include <EGL/eglext.h>

#if defined(TARGET_LINUX)
#include <X11/Xlib.h>
#endif

#if defined(HAVE_WAYLAND)
#include <wayland-client.h>
#include <wayland-egl.h>
#endif

#if defined(TARGET_ANDROID)
#include <android/native_window.h>
#include <android/hardware_buffer.h>
#endif

#include <egl.h>
#include "format.h"
#include "glcontext.h"
#include "log.h"
#include "nodegl.h"
#include "utils.h"

#define EGL_PLATFORM_X11 0x31D5
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_PLATFORM_WAYLAND 0x31D8
#define ENABLE_DEBUG
struct egl_priv {
    EGLNativeDisplayType native_display;
    int own_native_display;
    EGLNativeWindowType native_window;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext handle;
    EGLConfig config;
    const char *extensions;
    EGLBoolean (*PresentationTimeANDROID)(EGLDisplay dpy, EGLSurface sur, khronos_stime_nanoseconds_t time);
    EGLDisplay (*GetPlatformDisplay)(EGLenum platform, void *native_display, const EGLint *attrib_list);
    EGLAPIENTRY EGLImageKHR (*CreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
    EGLAPIENTRY EGLBoolean (*DestroyImageKHR)(EGLDisplay, EGLImageKHR);
#if defined(TARGET_ANDROID)
    EGLAPIENTRY EGLClientBuffer (*GetNativeClientBufferANDROID)(const struct AHardwareBuffer *);
#endif
    int has_platform_x11_ext;
    int has_platform_mesa_surfaceless_ext;
    int has_platform_wayland_ext;
    int has_surfaceless_context_ext;
#if defined(HAVE_WAYLAND)
    struct wl_egl_window *wl_egl_window;
#endif
};

EGLImageKHR ngli_eglCreateImageKHR(struct glcontext *gl, EGLConfig context, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
    struct egl_priv *egl = gl->priv_data;
    return egl->CreateImageKHR(egl->display, context, target, buffer, attrib_list);
}

EGLBoolean ngli_eglDestroyImageKHR(struct glcontext *gl, EGLImageKHR image)
{
    struct egl_priv *egl = gl->priv_data;
    return egl->DestroyImageKHR(egl->display, image);
}

#if defined(TARGET_ANDROID)
EGLClientBuffer ngli_eglGetNativeClientBufferANDROID(struct glcontext *gl, const struct AHardwareBuffer *buffer)
{
    struct egl_priv *egl = gl->priv_data;
    return egl->GetNativeClientBufferANDROID(buffer);
}
#endif

static int egl_probe_extensions(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

#if defined(TARGET_ANDROID)
    if (ngli_glcontext_check_extension("EGL_ANDROID_presentation_time", egl->extensions)) {
        egl->PresentationTimeANDROID = (void *)eglGetProcAddress("eglPresentationTimeANDROID");
        if (!egl->PresentationTimeANDROID) {
            LOG(ERROR, "could not retrieve eglPresentationTimeANDROID()");
            return -1;
        }
    }

    if (ngli_glcontext_check_extension("EGL_ANDROID_get_native_client_buffer", egl->extensions)) {
        egl->GetNativeClientBufferANDROID = (void *)eglGetProcAddress("eglGetNativeClientBufferANDROID");
        if (!egl->GetNativeClientBufferANDROID) {
            LOG(ERROR, "could not retrieve eglGetNativeClientBufferANDROID()");
            return -1;
        }
        ctx->features |= NGLI_FEATURE_EGL_ANDROID_GET_IMAGE_NATIVE_CLIENT_BUFFER;
    }
#endif

    if (ngli_glcontext_check_extension("EGL_KHR_image_base", egl->extensions)) {
        egl->CreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
        egl->DestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
        if (!egl->CreateImageKHR || !egl->DestroyImageKHR) {
            LOG(ERROR, "could not retrieve egl{Create,Destroy}ImageKHR()");
            return -1;
        }
        ctx->features |= NGLI_FEATURE_EGL_IMAGE_BASE_KHR;
    }

    if (ngli_glcontext_check_extension("EGL_EXT_image_dma_buf_import", egl->extensions)) {
        ctx->features |= NGLI_FEATURE_EGL_EXT_IMAGE_DMA_BUF_IMPORT;
    }

    if (ngli_glcontext_check_extension("EGL_KHR_surfaceless_context", egl->extensions)) {
        egl->has_surfaceless_context_ext = 1;
    }

    return 0;
}

#if defined(TARGET_LINUX)
static int egl_probe_client_extensions(struct egl_priv *egl)
{
    const char *client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!client_extensions) {
        LOG(ERROR, "could not retrieve EGL client extensions");
        return -1;
    }

    if (!ngli_glcontext_check_extension("EGL_EXT_platform_base", client_extensions)) {
        LOG(ERROR, "EGL_EXT_platform_base is not supported");
        return -1;
    }

    egl->GetPlatformDisplay = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!egl->GetPlatformDisplay) {
        LOG(ERROR, "could not retrieve eglGetPlatformDisplayEXT()");
        return -1;
    }

    if (ngli_glcontext_check_extension("EGL_KHR_platform_x11", client_extensions) ||
        ngli_glcontext_check_extension("EGL_EXT_platform_x11", client_extensions))
        egl->has_platform_x11_ext = 1;

    if (ngli_glcontext_check_extension("EGL_MESA_platform_surfaceless", client_extensions))
        egl->has_platform_mesa_surfaceless_ext = 1;

    if (ngli_glcontext_check_extension("EGL_KHR_platform_wayland", client_extensions) ||
        ngli_glcontext_check_extension("EGL_EXT_platform_wayland", client_extensions))
        egl->has_platform_wayland_ext = 1;

    return 0;
}
#endif

static EGLDisplay egl_get_display(struct glcontext *ctx, EGLNativeDisplayType native_display, int offscreen)
{
#if defined(TARGET_ANDROID)
    struct egl_priv *egl = ctx->priv_data;
    egl->native_display = native_display ? native_display : EGL_DEFAULT_DISPLAY;
    return eglGetDisplay(egl->native_display);
#elif defined(TARGET_LINUX)
    struct egl_priv *egl = ctx->priv_data;
    int ret = egl_probe_client_extensions(egl);
    if (ret < 0)
        return EGL_NO_DISPLAY;

    egl->native_display = native_display ? native_display : EGL_NO_DISPLAY;

    if (ctx->platform == NGL_PLATFORM_XLIB) {
        if (!egl->native_display) {
            egl->native_display = XOpenDisplay(NULL);
            if (!egl->native_display)
                LOG(WARNING, "could not retrieve X11 display");
            egl->own_native_display = egl->native_display ? 1 : 0;
        }

        if (egl->native_display) {
            if (!egl->has_platform_x11_ext) {
                LOG(ERROR, "EGL_EXT_platform_x11 is not supported");
                return EGL_NO_DISPLAY;
            }
            return egl->GetPlatformDisplay(EGL_PLATFORM_X11, egl->native_display, NULL);
        }
    } else if (ctx->platform == NGL_PLATFORM_WAYLAND) {
#if defined(HAVE_WAYLAND)
        if (!egl->native_display) {
            LOG(ERROR, "no Wayland display specified\n");
            return EGL_NO_DISPLAY;
        }

        if (!egl->has_platform_wayland_ext) {
            LOG(ERROR, "EGL_EXT_platform_wayland is not supported");
            return EGL_NO_DISPLAY;
        }
        return egl->GetPlatformDisplay(EGL_PLATFORM_WAYLAND, egl->native_display, NULL);
#else
        LOG(ERROR, "Wayland platform is not supported");
        return EGL_NO_DISPLAY;
#endif
    }

    if (egl->has_platform_mesa_surfaceless_ext && offscreen) {
        LOG(WARNING, "no display available, falling back on Mesa surfaceless platform");
        return egl->GetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    }
    return EGL_NO_DISPLAY;
#else
    return EGL_NO_DISPLAY;
#endif
}

static int egl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct egl_priv *egl = ctx->priv_data;

    egl->display = egl_get_display(ctx, (EGLNativeDisplayType)display, ctx->offscreen);
    if (!egl->display) {
        LOG(ERROR, "could not retrieve EGL display");
        return -1;
    }

    EGLint egl_minor;
    EGLint egl_major;
    int ret = eglInitialize(egl->display, &egl_major, &egl_minor);
    if (!ret) {
        LOG(ERROR, "could not initialize EGL: 0x%x", eglGetError());
        return -1;
    }

    egl->extensions = eglQueryString(egl->display, EGL_EXTENSIONS);
    if (!egl->extensions) {
        LOG(ERROR, "could not retrieve EGL extensions");
        return -1;
    }

    ret = egl_probe_extensions(ctx);
    if (ret < 0)
        return ret;

    if (egl_major < 1 || egl_minor < 4) {
        LOG(ERROR, "unsupported EGL version %d.%d, only 1.4+ is supported", egl_major, egl_minor);
        return -1;
    }

    int api = ctx->backend == NGL_BACKEND_OPENGL ? EGL_OPENGL_API : EGL_OPENGL_ES_API;
    if (!eglBindAPI(api)) {
        LOG(ERROR, "could not bind OpenGL%s API", ctx->backend == NGL_BACKEND_OPENGL ? "" : "ES");
        return -1;
    }

    const EGLint type = ctx->backend == NGL_BACKEND_OPENGL ? EGL_OPENGL_BIT : EGL_OPENGL_ES2_BIT;
    EGLint surface_type = ctx->offscreen ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT;

try_again:;

    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, type,
        EGL_SURFACE_TYPE, surface_type,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, ctx->offscreen ? 0 : (ctx->samples > 0),
        EGL_SAMPLES, ctx->offscreen ? 0 : ctx->samples,
        EGL_NONE
    };

    EGLint nb_configs;
    ret = eglChooseConfig(egl->display, config_attribs, &egl->config, 1, &nb_configs);
    if (ret && nb_configs == 0) {
        /* Fallback to EGL_WINDOW_BIT if the driver do not advertize any
         * pbuffer configurations. This happens on Wayland with Mesa. */
        if (surface_type == EGL_PBUFFER_BIT) {
            surface_type = EGL_WINDOW_BIT;
            goto try_again;
        }
    }

    if (!ret || !nb_configs) {
        LOG(ERROR, "could not choose a valid EGL configuration: 0x%x", eglGetError());
        return -1;
    }

    EGLContext shared_context = other ? (EGLContext)other : NULL;

    if (ctx->backend == NGL_BACKEND_OPENGL) {
        static const struct {
            int major;
            int minor;
        } gl_versions[] ={
            {4, 1}, // OpenGL 4.1
            {3, 3}, // OpenGL 3.3 (Mesa software renderers: llvmpipe, softpipe, swrast)
        };
        for (int i = 0; i < NGLI_ARRAY_NB(gl_versions); i++) {
            const EGLint ctx_attribs[] = {
                EGL_CONTEXT_MAJOR_VERSION_KHR, gl_versions[i].major,
                EGL_CONTEXT_MINOR_VERSION_KHR, gl_versions[i].minor,
                EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
#ifdef ENABLE_DEBUG
                EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
#endif
                EGL_NONE
            };

            if (i)
                LOG(WARNING, "falling back on OpenGL %d.%d", gl_versions[i].major, gl_versions[i].minor);

            egl->handle = eglCreateContext(egl->display, egl->config, shared_context, ctx_attribs);
            if (egl->handle)
                break;
        }
    } else {
        static const EGLint ctx_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        egl->handle = eglCreateContext(egl->display, egl->config, shared_context, ctx_attribs);
    }

    if (!egl->handle) {
        LOG(ERROR, "could not create EGL context: 0x%x", eglGetError());
        return -1;
    }

    if (ctx->offscreen) {
        if (egl->has_surfaceless_context_ext) {
            egl->surface = EGL_NO_SURFACE;
        } else {
            const EGLint attribs[] = {
                EGL_WIDTH, 1,
                EGL_HEIGHT, 1,
                EGL_NONE
            };

            egl->surface = eglCreatePbufferSurface(egl->display, egl->config, attribs);
            if (!egl->surface) {
                LOG(ERROR, "could not create EGL window surface: 0x%x", eglGetError());
                return -1;
            }
        }
    } else {
        if (ctx->platform == NGL_PLATFORM_XLIB ||
            ctx->platform == NGL_PLATFORM_ANDROID) {
            egl->native_window = (EGLNativeWindowType)window;
        } else if (ctx->platform == NGL_PLATFORM_WAYLAND) {
#if defined(HAVE_WAYLAND)
            struct wl_surface *wl_surface = (struct wl_surface *)window;
            if (!wl_surface) {
                LOG(ERROR, "no Wayland display specified");
                return -1;
            }
            egl->wl_egl_window = wl_egl_window_create(wl_surface, ctx->width, ctx->height);
            if (!egl->wl_egl_window) {
                LOG(ERROR, "could not create Wayland EGL window");
                return -1;
            }
            egl->native_window = (uintptr_t)egl->wl_egl_window;
#endif
        }
        if (!egl->native_window) {
            LOG(ERROR, "could not retrieve EGL native window");
            return -1;
        }
        egl->surface = eglCreateWindowSurface(egl->display, egl->config, egl->native_window, NULL);
        if (!egl->surface) {
            LOG(ERROR, "could not create EGL window surface: 0x%x", eglGetError());
            return -1;
        }
    }

    return 0;
}

static void egl_uninit(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

    ngli_glcontext_make_current(ctx, 0);

    if (egl->surface)
        eglDestroySurface(egl->display, egl->surface);

    if (egl->handle)
        eglDestroyContext(egl->display, egl->handle);

    if (egl->display)
        eglTerminate(egl->display);

#if defined(TARGET_LINUX)
    if (ctx->platform == NGL_PLATFORM_XLIB) {
        if (egl->own_native_display)
            XCloseDisplay(egl->native_display);
    } else if (ctx->platform == NGL_PLATFORM_WAYLAND) {
#if defined(HAVE_WAYLAND)
        if (egl->wl_egl_window)
            wl_egl_window_destroy(egl->wl_egl_window);
#endif
    }
#endif
}

static int egl_resize(struct glcontext *ctx, int width, int height)
{
    struct egl_priv *egl = ctx->priv_data;

#if defined(TARGET_ANDROID)
    const int w_width = ANativeWindow_getWidth(egl->native_window);
    const int w_height = ANativeWindow_getHeight(egl->native_window);
    GLint format;
    eglGetConfigAttrib(egl->display, egl->config, EGL_NATIVE_VISUAL_ID, &format);
    /* Resize the native window buffers to the native window size. This
     * ensures that the current EGL buffer will be of the same size as the
     * native window after a resize. See:
     * https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglSwapBuffers.xhtml */
    int ret = ANativeWindow_setBuffersGeometry(egl->native_window, w_width, w_height, format);
    if (ret < 0)
        return -1;
#endif

#if defined(HAVE_WAYLAND)
    if (ctx->platform == NGL_PLATFORM_WAYLAND)
        wl_egl_window_resize(egl->wl_egl_window, width, height, 0, 0);
#endif

    if (!eglQuerySurface(egl->display, egl->surface, EGL_WIDTH, &ctx->width) ||
        !eglQuerySurface(egl->display, egl->surface, EGL_HEIGHT, &ctx->height)) {
        LOG(ERROR, "could not query surface dimensions: 0x%x", eglGetError());
        return -1;
    }

    return 0;
}

static int egl_make_current(struct glcontext *ctx, int current)
{
    int ret;
    struct egl_priv *egl = ctx->priv_data;

    if (current) {
        ret = eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->handle);
    } else {
        ret = eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    return ret - 1;
}

static void egl_swap_buffers(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;

    if (!ctx->offscreen)
        eglSwapBuffers(egl->display, egl->surface);
}

static int egl_set_swap_interval(struct glcontext *ctx, int interval)
{
    struct egl_priv *egl = ctx->priv_data;

    if (!ctx->offscreen)
        eglSwapInterval(egl->display, interval);

    return 0;
}

static void egl_set_surface_pts(struct glcontext *ctx, double t)
{
#if defined(TARGET_ANDROID)
    if (ctx->offscreen) {
        LOG(WARNING, "setting surface pts is not supported with offscreen rendering");
        return;
    }

    struct egl_priv *egl = ctx->priv_data;

    if (egl->PresentationTimeANDROID) {
        EGLnsecsANDROID pts = t * 1000000000LL;
        egl->PresentationTimeANDROID(egl->display, egl->surface, pts);
    }
#endif
}

static void *egl_get_proc_address(struct glcontext *ctx, const char *name)
{
    return eglGetProcAddress(name);
}

static uintptr_t get_display(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;
    return (uintptr_t)egl->native_display;
}

static uintptr_t egl_get_handle(struct glcontext *ctx)
{
    struct egl_priv *egl = ctx->priv_data;
    return (uintptr_t)egl->handle;
}

const struct glcontext_class ngli_glcontext_egl_class = {
    .init = egl_init,
    .uninit = egl_uninit,
    .resize = egl_resize,
    .make_current = egl_make_current,
    .swap_buffers = egl_swap_buffers,
    .set_swap_interval = egl_set_swap_interval,
    .set_surface_pts = egl_set_surface_pts,
    .get_proc_address = egl_get_proc_address,
    .get_handle = egl_get_handle,
    .get_display = get_display,
    .priv_size = sizeof(struct egl_priv),
};
