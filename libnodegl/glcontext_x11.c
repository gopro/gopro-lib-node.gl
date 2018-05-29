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
#include <X11/Xlib.h>
#include <GL/glx.h>

#include "glcontext.h"
#include "nodegl.h"

struct glcontext_x11 {
    Display *display;
    Window window;
    GLXContext handle;
    GLXFBConfig *fbconfigs;
    int nb_fbconfigs;
};

typedef GLXContext (*glXCreateContextAttribsFunc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int glcontext_x11_init(struct glcontext *glcontext, void *display, void *window, void *handle)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        None
    };

    glcontext_x11->display = display ? *(Display **)display : glXGetCurrentDisplay();
    if (!glcontext_x11->display)
        return -1;
    /* XXX: yolo xopen ? */

    if (!glcontext->offscreen) {
        glcontext_x11->window  = window  ? *(Window *)window : glXGetCurrentDrawable();
        if (!glcontext_x11->window)
            return -1;
    }

    if (glcontext->wrapped) {
        glcontext_x11->handle  = handle  ? *(GLXContext *)handle : glXGetCurrentContext();
        if (!glcontext_x11->handle)
            return -1;
    }

    glcontext_x11->fbconfigs = glXChooseFBConfig(glcontext_x11->display,
                                                 DefaultScreen(glcontext_x11->display),
                                                 attribs,
                                                 &glcontext_x11->nb_fbconfigs);
    if (!glcontext_x11->fbconfigs)
        return -1;

    return 0;
}

static void glcontext_x11_uninit(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    if (glcontext_x11->fbconfigs)
        XFree(glcontext_x11->fbconfigs);

    if (!glcontext->wrapped)
        glXDestroyContext(glcontext_x11->display, glcontext_x11->handle);

    if (glcontext->offscreen)
        glXDestroyPbuffer(glcontext_x11->display, glcontext_x11->window);
}

static int glcontext_x11_create(struct glcontext *glcontext, void *other)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    glXCreateContextAttribsFunc glXCreateContextAttribs =
        (glXCreateContextAttribsFunc)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
    if (!glXCreateContextAttribs)
        return -1;

    Display *display = glcontext_x11->display;
    int screen = DefaultScreen(display);
    GLXFBConfig *fbconfigs = glcontext_x11->fbconfigs;

    const char *glx_extensions = glXQueryExtensionsString(display, screen);
    if (!ngli_glcontext_check_extension("GLX_ARB_create_context", glx_extensions))
        return -1;

    GLXContext shared_context = other ? *(GLXContext *)other : NULL;

    if (glcontext->api == NGL_GLAPI_OPENGLES) {
        if (!ngli_glcontext_check_extension("GLX_EXT_create_context_es2_profile", glx_extensions))
            return -1;

        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_ES2_PROFILE_BIT_EXT,
            GLX_CONTEXT_FLAGS_ARB, 0,
            None
        };

        glcontext_x11->handle = glXCreateContextAttribs(display,
                                                        fbconfigs[0],
                                                        shared_context,
                                                        1,
                                                        attribs);
    } else if (glcontext->api == NGL_GLAPI_OPENGL) {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };

        glcontext_x11->handle = glXCreateContextAttribs(display,
                                                        fbconfigs[0],
                                                        shared_context,
                                                        1,
                                                        attribs);
    }

    if (!glcontext_x11->handle)
        return -1;

    if (glcontext->offscreen) {
        int attribs[] = {
            GLX_PBUFFER_WIDTH, glcontext->offscreen_width,
            GLX_PBUFFER_HEIGHT, glcontext->offscreen_height,
            None
        };

        glcontext_x11->window = glXCreatePbuffer(display, fbconfigs[0], attribs);
        if (!glcontext_x11->window)
            return -1;
    }

    return 0;
}

static int glcontext_x11_make_current(struct glcontext *glcontext, int current)
{
    int ret;
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;

    if (current) {
        ret = glXMakeCurrent(glcontext_x11->display, glcontext_x11->window, glcontext_x11->handle);
    } else {
        ret = glXMakeCurrent(glcontext_x11->display, None, NULL);
    }

    return ret - 1;
}

static void glcontext_x11_swap_buffers(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;
    glXSwapBuffers(glcontext_x11->display, glcontext_x11->window);
}

static void *glcontext_x11_get_display(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;
    return &glcontext_x11->display;
}

static void *glcontext_x11_get_window(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;
    return &glcontext_x11->window;
}

static void *glcontext_x11_get_handle(struct glcontext *glcontext)
{
    struct glcontext_x11 *glcontext_x11 = glcontext->priv_data;
    return &glcontext_x11->handle;
}

static void *glcontext_x11_get_proc_address(struct glcontext *glcontext, const char *name)
{
    return glXGetProcAddress((const GLubyte *)name);
}

const struct glcontext_class ngli_glcontext_x11_class = {
    .init = glcontext_x11_init,
    .uninit = glcontext_x11_uninit,
    .create = glcontext_x11_create,
    .make_current = glcontext_x11_make_current,
    .swap_buffers = glcontext_x11_swap_buffers,
    .get_display = glcontext_x11_get_display,
    .get_window = glcontext_x11_get_window,
    .get_handle = glcontext_x11_get_handle,
    .get_proc_address = glcontext_x11_get_proc_address,
    .priv_size = sizeof(struct glcontext_x11),
};
