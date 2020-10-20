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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "bstr.h"
#include "glcontext.h"
#include "limit.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "utils.h"
#include "glincludes.h"

#include "gldefinitions_data.h"
#include "glfeatures_data.h"

NGLI_STATIC_ASSERT(gfloat_size,  sizeof(GLfloat)  == sizeof(float));
NGLI_STATIC_ASSERT(gbyte_size,   sizeof(GLbyte)   == sizeof(char));
NGLI_STATIC_ASSERT(gshort_size,  sizeof(GLshort)  == sizeof(short));
NGLI_STATIC_ASSERT(gint_size,    sizeof(GLint)    == sizeof(int));
NGLI_STATIC_ASSERT(gubyte_size,  sizeof(GLubyte)  == sizeof(unsigned char));
NGLI_STATIC_ASSERT(gushort_size, sizeof(GLushort) == sizeof(unsigned short));
NGLI_STATIC_ASSERT(guint_size,   sizeof(GLuint)   == sizeof(unsigned int));
NGLI_STATIC_ASSERT(gl_bool,      GL_FALSE == 0 && GL_TRUE == 1);

enum {
    GLPLATFORM_EGL,
    GLPLATFORM_NSGL,
    GLPLATFORM_EAGL,
    GLPLATFORM_WGL,
};

#ifdef HAVE_GLPLATFORM_EGL
extern const struct glcontext_class ngli_glcontext_egl_class;
#endif

#ifdef HAVE_GLPLATFORM_NSGL
extern const struct glcontext_class ngli_glcontext_nsgl_class;
#endif

#ifdef HAVE_GLPLATFORM_EAGL
extern const struct glcontext_class ngli_glcontext_eagl_class;
#endif

#ifdef HAVE_GLPLATFORM_WGL
extern const struct glcontext_class ngli_glcontext_wgl_class;
#endif

static const struct glcontext_class *glcontext_class_map[] = {
#ifdef HAVE_GLPLATFORM_EGL
    [GLPLATFORM_EGL] = &ngli_glcontext_egl_class,
#endif
#ifdef HAVE_GLPLATFORM_NSGL
    [GLPLATFORM_NSGL] = &ngli_glcontext_nsgl_class,
#endif
#ifdef HAVE_GLPLATFORM_EAGL
    [GLPLATFORM_EAGL] = &ngli_glcontext_eagl_class,
#endif
#ifdef HAVE_GLPLATFORM_WGL
    [GLPLATFORM_WGL] = &ngli_glcontext_wgl_class,
#endif
};

static const int platform_to_glplatform[] = {
    [NGL_PLATFORM_XLIB]    = GLPLATFORM_EGL,
    [NGL_PLATFORM_ANDROID] = GLPLATFORM_EGL,
    [NGL_PLATFORM_MACOS]   = GLPLATFORM_NSGL,
    [NGL_PLATFORM_IOS]     = GLPLATFORM_EAGL,
    [NGL_PLATFORM_WINDOWS] = GLPLATFORM_WGL,
    [NGL_PLATFORM_WAYLAND] = GLPLATFORM_EGL,
};

static int glcontext_load_functions(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    for (int i = 0; i < NGLI_ARRAY_NB(gldefinitions); i++) {
        void *func;
        const struct gldefinition *gldefinition = &gldefinitions[i];

        func = ngli_glcontext_get_proc_address(glcontext, gldefinition->name);
        if ((gldefinition->flags & M) && !func) {
            LOG(ERROR, "could not find core function: %s", gldefinition->name);
            return NGL_ERROR_NOT_FOUND;
        }

        *(void **)((uint8_t *)gl + gldefinition->offset) = func;
    }

    return 0;
}

static int glcontext_probe_version(struct glcontext *glcontext)
{
    GLint major_version = 0;
    GLint minor_version = 0;

    if (glcontext->backend == NGL_BACKEND_OPENGL) {
        ngli_glGetIntegerv(glcontext, GL_MAJOR_VERSION, &major_version);
        ngli_glGetIntegerv(glcontext, GL_MINOR_VERSION, &minor_version);

        if (major_version < 3) {
            LOG(ERROR, "node.gl only supports OpenGL >= 3.0");
            return NGL_ERROR_UNSUPPORTED;
        }
    } else if (glcontext->backend == NGL_BACKEND_OPENGLES) {
        const char *gl_version = (const char *)ngli_glGetString(glcontext, GL_VERSION);
        if (!gl_version) {
            LOG(ERROR, "could not get OpenGL ES version");
            return NGL_ERROR_BUG;
        }

        int ret = sscanf(gl_version,
                         "OpenGL ES %d.%d",
                         &major_version,
                         &minor_version);
        if (ret != 2) {
            LOG(ERROR, "could not parse OpenGL ES version (%s)", gl_version);
            return NGL_ERROR_BUG;
        }

        if (major_version < 2) {
            LOG(ERROR, "node.gl only supports OpenGL ES >= 2.0");
            return NGL_ERROR_UNSUPPORTED;
        }
    } else {
        ngli_assert(0);
    }

    LOG(INFO, "OpenGL version: %d.%d %s",
        major_version,
        minor_version,
        glcontext->backend == NGL_BACKEND_OPENGLES ? "ES " : "");

    const char *renderer = (const char *)ngli_glGetString(glcontext, GL_RENDERER);
    LOG(INFO, "OpenGL renderer: %s", renderer ? renderer : "unknown");

    if (renderer && (
        strstr(renderer, "llvmpipe") || // Mesa llvmpipe
        strstr(renderer, "softpipe") || // Mesa softpipe
        strstr(renderer, "SWR"))) {     // Mesa swrast
        glcontext->features |= NGLI_FEATURE_SOFTWARE;
        LOG(INFO, "Software renderer detected");
    }

    glcontext->version = major_version * 100 + minor_version * 10;

    return 0;
}

static int glcontext_check_extension(const char *extension,
                                     const struct glcontext *glcontext)
{
    GLint nb_extensions;
    ngli_glGetIntegerv(glcontext, GL_NUM_EXTENSIONS, &nb_extensions);

    for (GLint i = 0; i < nb_extensions; i++) {
        const char *tmp = (const char *)ngli_glGetStringi(glcontext, GL_EXTENSIONS, i);
        if (!tmp)
            break;
        if (!strcmp(extension, tmp))
            return 1;
    }

    return 0;
}

static int glcontext_check_extensions(struct glcontext *glcontext,
                                      const char **extensions)
{
    if (!extensions || !*extensions)
        return 0;

    if (glcontext->backend == NGL_BACKEND_OPENGLES) {
        const char *gl_extensions = (const char *)ngli_glGetString(glcontext, GL_EXTENSIONS);
        while (*extensions) {
            if (!ngli_glcontext_check_extension(*extensions, gl_extensions))
                return 0;

            extensions++;
        }
    } else if (glcontext->backend == NGL_BACKEND_OPENGL) {
        while (*extensions) {
            if (!glcontext_check_extension(*extensions, glcontext))
                return 0;

            extensions++;
        }
    } else {
        ngli_assert(0);
    }

    return 1;
}

static int glcontext_check_functions(struct glcontext *glcontext,
                                     const size_t *funcs_offsets)
{
    const struct glfunctions *gl = &glcontext->funcs;

    if (!funcs_offsets)
        return 1;

    while (*funcs_offsets != -1) {
        void *func_ptr = *(void **)((uint8_t *)gl + *funcs_offsets);
        if (!func_ptr)
            return 0;
        funcs_offsets++;
    }

    return 1;
}

static int glcontext_probe_extensions(struct glcontext *glcontext)
{
    const int es = glcontext->backend == NGL_BACKEND_OPENGLES;
    struct bstr *features_str = ngli_bstr_create();

    if (!features_str)
        return NGL_ERROR_MEMORY;

    for (int i = 0; i < NGLI_ARRAY_NB(glfeatures); i++) {
        const struct glfeature *glfeature = &glfeatures[i];

        const char **extensions = es ? glfeature->es_extensions : glfeature->extensions;
        ngli_assert(!extensions || *extensions);

        int version = es ? glfeature->es_version : glfeature->version;
        if (!version && !extensions)
            continue;

        if (!version || glcontext->version < version) {
            if (!glcontext_check_extensions(glcontext, extensions))
                continue;
        }

        if (!glcontext_check_functions(glcontext, glfeature->funcs_offsets))
            continue;

        ngli_bstr_printf(features_str, " %s", glfeature->name);
        glcontext->features |= glfeature->flag;
    }

    LOG(INFO, "OpenGL%s features:%s", es ? " ES" : "", ngli_bstr_strptr(features_str));
    ngli_bstr_freep(&features_str);

    return 0;
}

static int glcontext_check_mandatory_extensions(struct glcontext *glcontext)
{
    if (glcontext->version >= 300)
        return 0;

    if (!(glcontext->features & (NGLI_FEATURE_RGB8_RGBA8    |
                                 NGLI_FEATURE_DEPTH_TEXTURE |
                                 NGLI_FEATURE_PACKED_DEPTH_STENCIL))) {
        LOG(ERROR,
            "OpenGLES 2.0 context does not support mandatory extensions: "
            "OES_rgb8_rgba8, OES_depth_texture, OES_packed_depth_stencil");
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

static int glcontext_probe_settings(struct glcontext *glcontext)
{
    struct limits *limits = &glcontext->limits;

    ngli_glGetIntegerv(glcontext, GL_MAX_TEXTURE_IMAGE_UNITS, &limits->max_texture_image_units);

    limits->max_color_attachments = 1;
    if (glcontext->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) {
        ngli_glGetIntegerv(glcontext, GL_MAX_SAMPLES, &limits->max_samples);
        ngli_glGetIntegerv(glcontext, GL_MAX_COLOR_ATTACHMENTS, &limits->max_color_attachments);
    }

    if (glcontext->features & NGLI_FEATURE_UNIFORM_BUFFER_OBJECT) {
        ngli_glGetIntegerv(glcontext, GL_MAX_UNIFORM_BLOCK_SIZE, &limits->max_uniform_block_size);
    }

    if (glcontext->features & NGLI_FEATURE_COMPUTE_SHADER) {
        for (int i = 0; i < NGLI_ARRAY_NB(limits->max_compute_work_group_counts); i++) {
            ngli_glGetIntegeri_v(glcontext, GL_MAX_COMPUTE_WORK_GROUP_COUNT,
                                 i, &limits->max_compute_work_group_counts[i]);
        }
    }

    limits->max_draw_buffers = 1;
    if (glcontext->features & NGLI_FEATURE_DRAW_BUFFERS) {
        ngli_glGetIntegerv(glcontext, GL_MAX_DRAW_BUFFERS, &limits->max_draw_buffers);
    }

    return 0;
}

static int glcontext_load_extensions(struct glcontext *glcontext)
{
    int ret = glcontext_load_functions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_version(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_extensions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_check_mandatory_extensions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_settings(glcontext);
    if (ret < 0)
        return ret;

    return 0;
}

struct glcontext *ngli_glcontext_new(const struct ngl_config *config)
{
    if (config->platform < 0 || config->platform >= NGLI_ARRAY_NB(platform_to_glplatform))
        return NULL;

    const int glplatform = platform_to_glplatform[config->platform];
    if (glplatform < 0 || glplatform >= NGLI_ARRAY_NB(glcontext_class_map))
        return NULL;

    struct glcontext *glcontext = ngli_calloc(1, sizeof(*glcontext));
    if (!glcontext)
        return NULL;
    glcontext->class = glcontext_class_map[glplatform];

    if (glcontext->class->priv_size) {
        glcontext->priv_data = ngli_calloc(1, glcontext->class->priv_size);
        if (!glcontext->priv_data) {
            ngli_free(glcontext);
            return NULL;
        }
    }

    glcontext->platform = config->platform;
    glcontext->backend = config->backend;
    glcontext->offscreen = config->offscreen;
    glcontext->width = config->width;
    glcontext->height = config->height;
    glcontext->samples = config->samples;

    if (glcontext->class->init) {
        int ret = glcontext->class->init(glcontext, config->display, config->window, config->handle);
        if (ret < 0)
            goto fail;
    }

    int ret = ngli_glcontext_make_current(glcontext, 1);
    if (ret < 0)
        goto fail;

    ret = glcontext_load_extensions(glcontext);
    if (ret < 0)
        goto fail;

    if (glcontext->backend == NGL_BACKEND_OPENGL &&
        (glcontext->features & NGLI_FEATURE_TEXTURE_CUBE_MAP))
        ngli_glEnable(glcontext, GL_TEXTURE_CUBE_MAP_SEAMLESS);

    if (!glcontext->offscreen) {
        int ret = ngli_glcontext_resize(glcontext, glcontext->width, glcontext->height);
        if (ret < 0)
            goto fail;
    }

    if (config->swap_interval >= 0)
        ngli_glcontext_set_swap_interval(glcontext, config->swap_interval);

    return glcontext;
fail:
    ngli_glcontext_freep(&glcontext);
    return NULL;
}

int ngli_glcontext_make_current(struct glcontext *glcontext, int current)
{
    if (glcontext->class->make_current)
        return glcontext->class->make_current(glcontext, current);

    return 0;
}

int ngli_glcontext_set_swap_interval(struct glcontext *glcontext, int interval)
{
    if (glcontext->class->set_swap_interval)
        return glcontext->class->set_swap_interval(glcontext, interval);

    return 0;
}

void ngli_glcontext_swap_buffers(struct glcontext *glcontext)
{
    if (glcontext->class->swap_buffers)
        glcontext->class->swap_buffers(glcontext);
}

void ngli_glcontext_set_surface_pts(struct glcontext *glcontext, double t)
{
    if (glcontext->class->set_surface_pts)
        glcontext->class->set_surface_pts(glcontext, t);
}

int ngli_glcontext_resize(struct glcontext *glcontext, int width, int height)
{
    if (glcontext->offscreen) {
        LOG(ERROR, "offscreen rendering does not support resize operation");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (glcontext->class->resize)
        return glcontext->class->resize(glcontext, width, height);

    return NGL_ERROR_UNSUPPORTED;
}

void ngli_glcontext_freep(struct glcontext **glcontextp)
{
    struct glcontext *glcontext;

    if (!glcontextp || !*glcontextp)
        return;

    glcontext = *glcontextp;

    if (glcontext->class->uninit)
        glcontext->class->uninit(glcontext);

    ngli_free(glcontext->priv_data);
    ngli_freep(glcontextp);
}

void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name)
{
    void *ptr = NULL;

    if (glcontext->class->get_proc_address)
        ptr = glcontext->class->get_proc_address(glcontext, name);

    return ptr;
}

void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext)
{
    void *texture_cache = NULL;

    if (glcontext->class->get_texture_cache)
        texture_cache = glcontext->class->get_texture_cache(glcontext);

    return texture_cache;
}

uintptr_t ngli_glcontext_get_display(struct glcontext *glcontext)
{
    uintptr_t handle = 0;

    if (glcontext->class->get_display)
        handle = glcontext->class->get_display(glcontext);

    return handle;
}

uintptr_t ngli_glcontext_get_handle(struct glcontext *glcontext)
{
    uintptr_t handle = 0;

    if (glcontext->class->get_handle)
        handle = glcontext->class->get_handle(glcontext);

    return handle;
}

GLuint ngli_glcontext_get_default_framebuffer(struct glcontext *glcontext)
{
    GLuint fbo_id = 0;

    if (glcontext->class->get_default_framebuffer)
        fbo_id = glcontext->class->get_default_framebuffer(glcontext);

    return fbo_id;
}

int ngli_glcontext_check_extension(const char *extension, const char *extensions)
{
    if (!extension || !extensions)
        return 0;

    size_t len = strlen(extension);
    const char *cur = extensions;
    const char *end = extensions + strlen(extensions);

    while (cur < end) {
        cur = strstr(extensions, extension);
        if (!cur)
            break;

        cur += len;
        if (cur[0] == ' ' || cur[0] == '\0')
            return 1;
    }

    return 0;
}

int ngli_glcontext_check_gl_error(const struct glcontext *glcontext, const char *context)
{
    const GLenum error = ngli_glGetError(glcontext);
    const char *errorstr = NULL;

    if (!error)
        return error;

    switch (error) {
    case GL_INVALID_ENUM:
        errorstr = "GL_INVALID_ENUM";
        break;
    case GL_INVALID_VALUE:
        errorstr = "GL_INVALID_VALUE";
        break;
    case GL_INVALID_OPERATION:
        errorstr = "GL_INVALID_OPERATION";
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        errorstr = "GL_INVALID_FRAMEBUFFER_OPERATION";
        break;
    case GL_OUT_OF_MEMORY:
        errorstr = "GL_OUT_OF_MEMORY";
        break;
    }

    if (errorstr)
        LOG(ERROR, "GL error in %s: %s", context, errorstr);
    else
        LOG(ERROR, "GL error in %s: %04x", context, error);

#ifdef DEBUG_GL
    ngli_assert(0);
#endif

    return error;
}
