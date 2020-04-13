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

#include <CoreVideo/CoreVideo.h>
#include <UIKit/UIKit.h>
#include <OpenGLES/EAGL.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

#include "glcontext.h"
#include "log.h"
#include "nodegl.h"

struct eagl_priv {
    EAGLContext *handle;
    UIView *view;
    CAEAGLLayer *layer;
    CFBundleRef framework;
    CVOpenGLESTextureCacheRef texture_cache;
    GLuint fbo;
    GLuint colorbuffer;
    GLuint depthbuffer;
    GLuint fbo_ms;
    GLuint colorbuffer_ms;
    int fb_initialized;
};

static int eagl_init_layer(struct glcontext *ctx)
{
    if (![NSThread isMainThread]) {
        LOG(ERROR, "eagl_init_layer() must be called from the UI thread");
        return -1;
    }

    struct eagl_priv *eagl = ctx->priv_data;

    eagl->layer = (CAEAGLLayer *)[eagl->view layer];
    if (!eagl->layer) {
        LOG(ERROR, "could not retrieve EAGL layer");
        return -1;
    }

    NSDictionary *properties = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
        nil
    ];

    [eagl->layer setOpaque:YES];
    [eagl->layer setDrawableProperties:properties];

    return 0;
}

static int eagl_init(struct glcontext *ctx, uintptr_t display, uintptr_t window, uintptr_t other)
{
    struct eagl_priv *eagl = ctx->priv_data;

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengles"));
    if (!framework) {
        LOG(ERROR, "could not retrieve OpenGLES framework");
        return -1;
    }

    eagl->framework = (CFBundleRef)CFRetain(framework);
    if (!eagl->framework) {
        LOG(ERROR, "could not retain OpenGL framework object");
        return -1;
    }

    if (!ctx->offscreen) {
        eagl->view = (UIView *)window;
        if (!eagl->view) {
            LOG(ERROR, "could not retrieve UI view");
            return -1;
        }

        int ret = eagl_init_layer(ctx);
        if (ret < 0)
            return ret;
    }

    EAGLSharegroup *share_group = nil;
    if (other) {
        EAGLContext *shared_context = (EAGLContext *)other;
        share_group = [shared_context sharegroup];
    }

    eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3 sharegroup:share_group];
    if (!eagl->handle) {
        eagl->handle = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:share_group];
        if (!eagl->handle) {
            LOG(ERROR, "could not create EAGL context");
            return -1;
        }
        if (ctx->samples > 0) {
            LOG(WARNING, "multisample anti-aliasing is not supported with OpenGLES 2.0 context");
            ctx->samples = 0;
        }
    }

    CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                                NULL,
                                                eagl->handle,
                                                NULL,
                                                &eagl->texture_cache);
    if (err != noErr) {
        LOG(ERROR, "could not create CoreVideo texture cache: 0x%x", err);
        return -1;
    }

    return 0;
}

static int eagl_init_framebuffer(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    eagl->fb_initialized = 1;
    ngli_glGenFramebuffers(ctx, 1, &eagl->fbo);
    ngli_glBindFramebuffer(ctx, GL_FRAMEBUFFER, eagl->fbo);

    if (!eagl->colorbuffer)
        ngli_glGenRenderbuffers(ctx, 1, &eagl->colorbuffer);
    ngli_glBindRenderbuffer (ctx, GL_RENDERBUFFER, eagl->colorbuffer);
    [eagl->handle renderbufferStorage:GL_RENDERBUFFER fromDrawable:eagl->layer];
    ngli_glFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, eagl->colorbuffer);
    ngli_glGetRenderbufferParameteriv(ctx, GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &ctx->width);
    ngli_glGetRenderbufferParameteriv(ctx, GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &ctx->height);

    if (!ctx->samples) {
        ngli_glGenRenderbuffers(ctx, 1, &eagl->depthbuffer);
        ngli_glBindRenderbuffer(ctx, GL_RENDERBUFFER, eagl->depthbuffer);
        ngli_glRenderbufferStorage(ctx, GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, ctx->width, ctx->height);
        ngli_glFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);
        ngli_glFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
       LOG(ERROR, "framebuffer is not complete: 0x%x", status);
       return -1;
    }

    if (ctx->samples > 0) {
        ngli_glGenFramebuffers(ctx, 1, &eagl->fbo_ms);
        ngli_glBindFramebuffer(ctx, GL_FRAMEBUFFER, eagl->fbo_ms);

        ngli_glGenRenderbuffers(ctx, 1, &eagl->colorbuffer_ms);
        ngli_glBindRenderbuffer(ctx, GL_RENDERBUFFER, eagl->colorbuffer_ms);
        ngli_glRenderbufferStorageMultisample(ctx, GL_RENDERBUFFER, ctx->samples, GL_RGBA8, ctx->width, ctx->height);
        ngli_glFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, eagl->colorbuffer_ms);

        ngli_glGenRenderbuffers(ctx, 1, &eagl->depthbuffer);
        ngli_glBindRenderbuffer(ctx, GL_RENDERBUFFER, eagl->depthbuffer);
        ngli_glRenderbufferStorageMultisample(ctx, GL_RENDERBUFFER, ctx->samples, GL_DEPTH24_STENCIL8_OES, ctx->width, ctx->height);
        ngli_glFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);
        ngli_glFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, eagl->depthbuffer);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG(ERROR, "framebuffer is not complete: 0x%x", status);
            return -1;
        }
    }

    ngli_glViewport(ctx, 0, 0, ctx->width, ctx->height);

    return 0;
}

static void eagl_reset_framebuffer(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    ngli_glDeleteFramebuffers(ctx, 1, &eagl->fbo);
    ngli_glDeleteFramebuffers(ctx, 1, &eagl->fbo_ms);
    ngli_glDeleteRenderbuffers(ctx, 1, &eagl->depthbuffer);
    ngli_glDeleteRenderbuffers(ctx, 1, &eagl->colorbuffer_ms);
}

static void eagl_uninit(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    if (eagl->fb_initialized) {
        eagl_reset_framebuffer(ctx);
        ngli_glDeleteRenderbuffers(ctx, 1, &eagl->colorbuffer);
        eagl->fb_initialized = 0;
    }

    if (eagl->framework)
        CFRelease(eagl->framework);

    if (eagl->texture_cache)
        CFRelease(eagl->texture_cache);

    if (eagl->handle)
        CFRelease(eagl->handle);
}

static int eagl_resize(struct glcontext *ctx, int width, int height)
{
    if (![NSThread isMainThread]) {
        LOG(ERROR, "eagl_resize() must be called from the UI thread");
        return -1;
    }

    eagl_reset_framebuffer(ctx);

    return eagl_init_framebuffer(ctx);
}

static int eagl_make_current(struct glcontext *ctx, int current)
{
    int ret;
    struct eagl_priv *eagl = ctx->priv_data;

    if (current) {
        ret = [EAGLContext setCurrentContext:eagl->handle];
    } else {
        ret = [EAGLContext setCurrentContext:nil];
    }

    return ret;
}

static void eagl_swap_buffers(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;

    if (ctx->offscreen)
        return;

    if (ctx->samples > 0) {
        ngli_glBindFramebuffer(ctx, GL_READ_FRAMEBUFFER, eagl->fbo_ms);
        ngli_glBindFramebuffer(ctx, GL_DRAW_FRAMEBUFFER, eagl->fbo);
        GLbitfield mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        ngli_glBlitFramebuffer(ctx, 0, 0, ctx->width, ctx->height, 0, 0, ctx->width, ctx->height, mask, GL_NEAREST);
        ngli_glBindFramebuffer(ctx, GL_DRAW_FRAMEBUFFER, eagl->fbo_ms);
    }

    ngli_glBindRenderbuffer(ctx, GL_RENDERBUFFER, eagl->colorbuffer);
    [eagl->handle presentRenderbuffer: GL_RENDERBUFFER];
}

static void *eagl_get_texture_cache(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;
    return &eagl->texture_cache;
}

static void *eagl_get_proc_address(struct glcontext *ctx, const char *name)
{
    struct eagl_priv *eagl = ctx->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(eagl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

static uintptr_t eagl_get_handle(struct glcontext *ctx)
{
    struct eagl_priv *eagl = ctx->priv_data;
    return (uintptr_t)eagl->handle;
}

static GLuint eagl_get_default_framebuffer(struct glcontext *ctx)
{
    const struct eagl_priv *eagl = ctx->priv_data;
    return ctx->samples > 0 ? eagl->fbo_ms : eagl->fbo;
}

const struct glcontext_class ngli_glcontext_eagl_class = {
    .init = eagl_init,
    .uninit = eagl_uninit,
    .resize = eagl_resize,
    .make_current = eagl_make_current,
    .swap_buffers = eagl_swap_buffers,
    .get_texture_cache = eagl_get_texture_cache,
    .get_proc_address = eagl_get_proc_address,
    .get_handle = eagl_get_handle,
    .get_default_framebuffer = eagl_get_default_framebuffer,
    .priv_size = sizeof(struct eagl_priv),
};
