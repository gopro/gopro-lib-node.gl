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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenGL/OpenGL.h>
#include <CoreFoundation/CFBundle.h>
#include <Cocoa/Cocoa.h>

#include "format.h"
#include "glcontext.h"
#include "log.h"
#include "nodegl.h"

struct nsgl_priv {
    NSOpenGLPixelFormat *pixel_format;
    NSOpenGLContext *handle;
    NSView *view;
    CFBundleRef framework;
};

static int nsgl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    if (ctx->backend != NGL_BACKEND_OPENGL) {
        LOG(ERROR, "unsupported backend: %d, only OpenGL is supported by NSGL", ctx->backend);
        return -1;
    }

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
    if (!framework) {
        LOG(ERROR, "could not retrieve OpenGL framework");
        return -1;
    }

    nsgl->framework = (CFBundleRef)CFRetain(framework);
    if (!nsgl->framework) {
        LOG(ERROR, "could not retain OpenGL framework object");
        return -1;
    }

    const NSOpenGLPixelFormatAttribute pixelAttrs[] = {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAClosestPolicy,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        NSOpenGLPFASampleBuffers, ctx->offscreen ? 0 : (ctx->samples > 0),
        NSOpenGLPFASamples, ctx->offscreen ? 0 : ctx->samples,
        0,
    };

    nsgl->pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttrs];
    if (!nsgl->pixel_format) {
        LOG(ERROR, "could not allocate pixel format");
        return -1;
    }

    NSOpenGLContext *shared_context = other ? (NSOpenGLContext *)other : NULL;
    nsgl->handle = [[NSOpenGLContext alloc] initWithFormat:nsgl->pixel_format shareContext:shared_context];
    if (!nsgl->handle) {
        LOG(ERROR, "could not create NSGL context");
        return -1;
    }

    if (!ctx->offscreen) {
        if (![NSThread isMainThread]) {
            LOG(ERROR, "nsgl_init() must be called from the main thread");
            return -1;
        }

        NSObject *object = (NSObject *)window;
        if (!object) {
            LOG(ERROR, "no window specified");
            return -1;
        }

        if ([object isKindOfClass:[NSView class]]) {
            nsgl->view = (NSView *)object;
        } else if ([object isKindOfClass:[NSWindow class]]) {
            NSWindow *window = (NSWindow *)object;
            nsgl->view = [window contentView];
            if (!nsgl->view) {
                LOG(ERROR, "could not retrieve a NSView from the NSWindow");
                return -1;
            }
        } else {
            LOG(ERROR, "window must be either a NSView or a NSWindow");
            return -1;
        }

        [nsgl->handle setView:nsgl->view];
    }

    return 0;
}

static int nsgl_resize(struct glcontext *ctx, int width, int height)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    if (![NSThread isMainThread]) {
        LOG(ERROR, "nsgl_resize() must be called from the main thread");
        return -1;
    }

    [nsgl->handle update];

    NSRect bounds = [nsgl->view bounds];
    ctx->width = bounds.size.width;
    ctx->height = bounds.size.height;

    return 0;
}

static int nsgl_make_current(struct glcontext *ctx, int current)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    if (current) {
        [nsgl->handle makeCurrentContext];
    } else {
        [NSOpenGLContext clearCurrentContext];
    }

    return 0;
}

static void nsgl_swap_buffers(struct glcontext *ctx)
{
    struct nsgl_priv *nsgl = ctx->priv_data;
    [nsgl->handle flushBuffer];
}

static int nsgl_set_swap_interval(struct glcontext *ctx, int interval)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    [nsgl->handle setValues:&interval forParameter:NSOpenGLCPSwapInterval];

    return 0;
}

static void *nsgl_get_proc_address(struct glcontext *ctx, const char *name)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(nsgl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

static uintptr_t nsgl_get_handle(struct glcontext *ctx)
{
    struct nsgl_priv *nsgl = ctx->priv_data;
    return (uintptr_t)nsgl->handle;
}

static void nsgl_uninit(struct glcontext *ctx)
{
    struct nsgl_priv *nsgl = ctx->priv_data;

    if (nsgl->framework)
        CFRelease(nsgl->framework);

    if (nsgl->handle) {
        [nsgl->handle clearDrawable];
        CFRelease(nsgl->handle);
    }

    if (nsgl->pixel_format)
        CFRelease(nsgl->pixel_format);
}

const struct glcontext_class ngli_glcontext_nsgl_class = {
    .init = nsgl_init,
    .uninit = nsgl_uninit,
    .resize = nsgl_resize,
    .make_current = nsgl_make_current,
    .swap_buffers = nsgl_swap_buffers,
    .set_swap_interval = nsgl_set_swap_interval,
    .get_proc_address = nsgl_get_proc_address,
    .get_handle = nsgl_get_handle,
    .priv_size = sizeof(struct nsgl_priv),
};
