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

#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#include <stdlib.h>

#include "nodegl/features.h"
#include "nodegl/porting/gl/glfunctions.h"
#include "nodegl/glimits.h"
#include "nodegl/nodegl.h"

struct glcontext_class;

struct glcontext {
    /* GL context */
    const struct glcontext_class *class;
    void *priv_data;

    /* User options */
    int platform;
    int backend;
    int offscreen;
    int width;
    int height;
    int samples;

    /* GL api */
    int version;

    /* GL features */
    int features;

    /* GL limits */
    struct limits limits;

    /* GL functions */
    struct glfunctions funcs;
};

struct glcontext_class {
    int (*init)(struct glcontext *glcontext, uintptr_t display, uintptr_t window, uintptr_t handle);
    int (*resize)(struct glcontext *glcontext, int width, int height);
    int (*make_current)(struct glcontext *glcontext, int current);
    void (*swap_buffers)(struct glcontext *glcontext);
    int (*set_swap_interval)(struct glcontext *glcontext, int interval);
    void (*set_surface_pts)(struct glcontext *glcontext, double t);
    void* (*get_texture_cache)(struct glcontext *glcontext);
    void* (*get_proc_address)(struct glcontext *glcontext, const char *name);
    uintptr_t (*get_display)(struct glcontext *glcontext);
    uintptr_t (*get_handle)(struct glcontext *glcontext);
    GLuint (*get_default_framebuffer)(struct glcontext *glcontext);
    void (*uninit)(struct glcontext *glcontext);
    size_t priv_size;
};

struct glcontext *ngli_glcontext_new(const struct ngl_config *config);
int ngli_glcontext_make_current(struct glcontext *glcontext, int current);
void ngli_glcontext_swap_buffers(struct glcontext *glcontext);
int ngli_glcontext_set_swap_interval(struct glcontext *glcontext, int interval);
void ngli_glcontext_set_surface_pts(struct glcontext *glcontext, double t);
int ngli_glcontext_resize(struct glcontext *glcontext, int width, int height);
void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name);
void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext);
uintptr_t ngli_glcontext_get_display(struct glcontext *glcontext);
uintptr_t ngli_glcontext_get_handle(struct glcontext *glcontext);
GLuint ngli_glcontext_get_default_framebuffer(struct glcontext *glcontext);
void ngli_glcontext_freep(struct glcontext **glcontext);
int ngli_glcontext_check_extension(const char *extension, const char *extensions);
int ngli_glcontext_check_gl_error(const struct glcontext *glcontext, const char *context);

#include "glwrappers.h"

#endif /* GLCONTEXT_H */
