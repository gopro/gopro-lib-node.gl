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
#include "glfunctions.h"
#include "nodegl.h"

#define NGLI_FEATURE_VERTEX_ARRAY_OBJECT          (1 << 0)
#define NGLI_FEATURE_TEXTURE_3D                   (1 << 1)
#define NGLI_FEATURE_TEXTURE_STORAGE              (1 << 2)
#define NGLI_FEATURE_COMPUTE_SHADER               (1 << 3)
#define NGLI_FEATURE_PROGRAM_INTERFACE_QUERY      (1 << 4)
#define NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE      (1 << 5)
#define NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT (1 << 6)
#define NGLI_FEATURE_FRAMEBUFFER_OBJECT           (1 << 7)
#define NGLI_FEATURE_INTERNALFORMAT_QUERY         (1 << 8)
#define NGLI_FEATURE_PACKED_DEPTH_STENCIL         (1 << 9)
#define NGLI_FEATURE_TIMER_QUERY                  (1 << 10)
#define NGLI_FEATURE_EXT_DISJOINT_TIMER_QUERY     (1 << 11)
#define NGLI_FEATURE_DRAW_INSTANCED               (1 << 12)
#define NGLI_FEATURE_INSTANCED_ARRAY              (1 << 13)
#define NGLI_FEATURE_UNIFORM_BUFFER_OBJECT        (1 << 14)
#define NGLI_FEATURE_INVALIDATE_SUBDATA           (1 << 15)
#define NGLI_FEATURE_OES_EGL_EXTERNAL_IMAGE       (1 << 16)
#define NGLI_FEATURE_DEPTH_TEXTURE                (1 << 17)
#define NGLI_FEATURE_RGB8_RGBA8                   (1 << 18)
#define NGLI_FEATURE_OES_EGL_IMAGE                (1 << 19)
#define NGLI_FEATURE_EGL_IMAGE_BASE_KHR           (1 << 20)
#define NGLI_FEATURE_EGL_EXT_IMAGE_DMA_BUF_IMPORT (1 << 21)
#define NGLI_FEATURE_SYNC                         (1 << 22)
#define NGLI_FEATURE_YUV_TARGET                   (1 << 23)
#define NGLI_FEATURE_TEXTURE_NPOT                 (1 << 24)
#define NGLI_FEATURE_TEXTURE_CUBE_MAP             (1 << 25)
#define NGLI_FEATURE_DRAW_BUFFERS                 (1 << 26)
#define NGLI_FEATURE_ROW_LENGTH                   (1 << 27)
#define NGLI_FEATURE_SOFTWARE                     (1 << 28)

#define NGLI_FEATURE_COMPUTE_SHADER_ALL (NGLI_FEATURE_COMPUTE_SHADER           | \
                                         NGLI_FEATURE_PROGRAM_INTERFACE_QUERY  | \
                                         NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE  | \
                                         NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)

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
    int max_texture_image_units;
    int max_compute_work_group_counts[3];
    int max_uniform_block_size;
    int max_samples;
    int max_color_attachments;
    int max_draw_buffers;

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
