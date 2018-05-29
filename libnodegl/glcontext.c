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
#include "log.h"
#include "nodegl.h"
#include "utils.h"
#include "glincludes.h"

#include "gldefinitions_data.h"
#include "glfeatures_data.h"

#ifdef HAVE_PLATFORM_GLX
extern const struct glcontext_class ngli_glcontext_x11_class;
#endif

#ifdef HAVE_PLATFORM_EGL
extern const struct glcontext_class ngli_glcontext_egl_class;
#endif

#ifdef HAVE_PLATFORM_NSGL
extern const struct glcontext_class ngli_glcontext_nsgl_class;
#endif

#ifdef HAVE_PLATFORM_EAGL
extern const struct glcontext_class ngli_glcontext_eagl_class;
#endif

#ifdef HAVE_PLATFORM_WGL
extern const struct glcontext_class ngli_glcontext_wgl_class;
#endif

static const struct glcontext_class *glcontext_class_map[] = {
#ifdef HAVE_PLATFORM_GLX
    [NGL_GLPLATFORM_GLX] = &ngli_glcontext_x11_class,
#endif
#ifdef HAVE_PLATFORM_EGL
    [NGL_GLPLATFORM_EGL] = &ngli_glcontext_egl_class,
#endif
#ifdef HAVE_PLATFORM_NSGL
    [NGL_GLPLATFORM_NSGL] = &ngli_glcontext_nsgl_class,
#endif
#ifdef HAVE_PLATFORM_EAGL
    [NGL_GLPLATFORM_EAGL] = &ngli_glcontext_eagl_class,
#endif
#ifdef HAVE_PLATFORM_WGL
    [NGL_GLPLATFORM_WGL] = &ngli_glcontext_wgl_class,
#endif
};

static int glcontext_choose_platform(int platform)
{
    if (platform != NGL_GLPLATFORM_AUTO)
        return platform;

#if defined(TARGET_LINUX)
    return NGL_GLPLATFORM_GLX;
#elif defined(TARGET_IPHONE)
    return NGL_GLPLATFORM_EAGL;
#elif defined(TARGET_DARWIN)
    return NGL_GLPLATFORM_NSGL;
#elif defined(TARGET_ANDROID)
    return NGL_GLPLATFORM_EGL;
#elif defined(TARGET_MINGW_W64)
    return NGL_GLPLATFORM_WGL;
#else
    LOG(ERROR, "Can not determine which GL platform to use");
    return -1;
#endif
}

static int glcontext_choose_api(int api)
{
    if (api != NGL_GLAPI_AUTO)
        return api;

#if defined(TARGET_IPHONE) || defined(TARGET_ANDROID)
    return NGL_GLAPI_OPENGLES;
#else
    return NGL_GLAPI_OPENGL;
#endif
}

struct glcontext *ngli_glcontext_new(struct ngl_config *config)
{
    struct glcontext *glcontext = NULL;

    int platform = glcontext_choose_platform(config->platform);
    if (platform < 0)
        return NULL;

    int api = glcontext_choose_api(config->api);
    if (api < 0)
        return NULL;

    if (platform < 0 || platform >= NGLI_ARRAY_NB(glcontext_class_map))
        return NULL;

    glcontext = calloc(1, sizeof(*glcontext));
    if (!glcontext)
        return NULL;

    glcontext->class = glcontext_class_map[platform];
    if (glcontext->class->priv_size) {
        glcontext->priv_data = calloc(1, glcontext->class->priv_size);
        if (!glcontext->priv_data) {
            goto fail;
        }
    }

    glcontext->platform = platform;
    glcontext->api = api;
    glcontext->wrapped = config->wrapped;

    if (glcontext->class->init) {
        void *handle = glcontext->wrapped ? config->handle : NULL;
        int ret = glcontext->class->init(glcontext, config->display, config->window, handle);
        if (ret < 0)
            goto fail;
    }

    if (!glcontext->wrapped) {
        if (glcontext->class->create) {
            int ret = glcontext->class->create(glcontext, config->handle);
            if (ret < 0)
                goto fail;
        }
    }

    return glcontext;
fail:
    ngli_glcontext_freep(&glcontext);
    return NULL;
}

static int glcontext_load_functions(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    for (int i = 0; i < NGLI_ARRAY_NB(gldefinitions); i++) {
        void *func;
        const struct gldefinition *gldefinition = &gldefinitions[i];

        func = ngli_glcontext_get_proc_address(glcontext, gldefinition->name);
        if ((gldefinition->flags & M) && !func) {
            LOG(ERROR, "could not find core function: %s", gldefinition->name);
            return -1;
        }

        *(void **)((uint8_t *)gl + gldefinition->offset) = func;
    }

    return 0;
}

static int glcontext_probe_version(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    if (glcontext->api == NGL_GLAPI_OPENGL) {
        ngli_glGetIntegerv(gl, GL_MAJOR_VERSION, &glcontext->major_version);
        ngli_glGetIntegerv(gl, GL_MINOR_VERSION, &glcontext->minor_version);

        if (glcontext->major_version < 3) {
            LOG(ERROR, "node.gl only supports OpenGL >= 3.0");
            return -1;
        }
    } else if (glcontext->api == NGL_GLAPI_OPENGLES) {
        glcontext->es = 1;

        const char *gl_version = (const char *)ngli_glGetString(gl, GL_VERSION);
        if (!gl_version) {
            LOG(ERROR, "could not get OpenGL ES version");
            return -1;
        }

        int ret = sscanf(gl_version,
                         "OpenGL ES %d.%d",
                         &glcontext->major_version,
                         &glcontext->minor_version);
        if (ret != 2) {
            LOG(ERROR, "could not parse OpenGL ES version (%s)", gl_version);
            return -1;
        }

        if (glcontext->major_version < 2) {
            LOG(ERROR, "node.gl only supports OpenGL ES >= 2.0");
            return -1;
        }
    } else {
        ngli_assert(0);
    }

    LOG(INFO, "OpenGL%s%d.%d",
        glcontext->api == NGL_GLAPI_OPENGLES ? " ES " : " ",
        glcontext->major_version,
        glcontext->minor_version);

    return 0;
}

static int glcontext_check_extension(const char *extension,
                                     const struct glfunctions *gl)
{
    GLint nb_extensions;
    ngli_glGetIntegerv(gl, GL_NUM_EXTENSIONS, &nb_extensions);

    for (GLint i = 0; i < nb_extensions; i++) {
        const char *tmp = (const char *)ngli_glGetStringi(gl, GL_EXTENSIONS, i);
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
    const struct glfunctions *gl = &glcontext->funcs;

    if (!extensions || !*extensions)
        return 0;

    if (glcontext->es) {
        const char *gl_extensions = (const char *)ngli_glGetString(gl, GL_EXTENSIONS);
        while (*extensions) {
            if (!ngli_glcontext_check_extension(*extensions, gl_extensions))
                return 0;

            extensions++;
        }
    } else {
        while (*extensions) {
            if (!glcontext_check_extension(*extensions, gl))
                return 0;

            extensions++;
        }
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
    const int es = glcontext->es;
    struct bstr *features_str = ngli_bstr_create();

    if (!features_str)
        return -1;

    for (int i = 0; i < NGLI_ARRAY_NB(glfeatures); i++) {
        const struct glfeature *glfeature = &glfeatures[i];

        int maj_version = es ? glfeature->maj_es_version : glfeature->maj_version;
        int min_version = es ? glfeature->min_es_version : glfeature->min_version;

        if (!(glcontext->major_version >= maj_version &&
              glcontext->minor_version >= min_version)) {
            const char **extensions = es ? glfeature->es_extensions : glfeature->extensions;
            if (!glcontext_check_extensions(glcontext, extensions))
                continue;
        }

        if (!glcontext_check_functions(glcontext, glfeature->funcs_offsets))
            continue;

        ngli_bstr_print(features_str, " %s", glfeature->name);
        glcontext->features |= glfeature->flag;
    }

    LOG(INFO, "OpenGL%s features:%s", es ? " ES" : "", ngli_bstr_strptr(features_str));
    ngli_bstr_freep(&features_str);

    return 0;
}

static int glcontext_probe_settings(struct glcontext *glcontext)
{
    const int es = glcontext->es;
    const struct glfunctions *gl = &glcontext->funcs;

    if (es && glcontext->major_version == 2 && glcontext->minor_version == 0) {
        glcontext->gl_1comp = GL_LUMINANCE;
        glcontext->gl_2comp = GL_LUMINANCE_ALPHA;
    } else {
        glcontext->gl_1comp = GL_RED;
        glcontext->gl_2comp = GL_RG;
    }

    ngli_glGetIntegerv(gl, GL_MAX_TEXTURE_IMAGE_UNITS, &glcontext->max_texture_image_units);

    if (glcontext->features & NGLI_FEATURE_COMPUTE_SHADER) {
        for (int i = 0; i < NGLI_ARRAY_NB(glcontext->max_compute_work_group_counts); i++) {
            ngli_glGetIntegeri_v(gl, GL_MAX_COMPUTE_WORK_GROUP_COUNT,
                                 i, &glcontext->max_compute_work_group_counts[i]);
        }
    }

    return 0;
}

int ngli_glcontext_load_extensions(struct glcontext *glcontext)
{
    if (glcontext->loaded)
        return 0;

    int ret = glcontext_load_functions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_version(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_extensions(glcontext);
    if (ret < 0)
        return ret;

    ret = glcontext_probe_settings(glcontext);
    if (ret < 0)
        return ret;

    glcontext->loaded = 1;

    return 0;
}

int ngli_glcontext_make_current(struct glcontext *glcontext, int current)
{
    if (glcontext->class->make_current)
        return glcontext->class->make_current(glcontext, current);

    return 0;
}

void ngli_glcontext_swap_buffers(struct glcontext *glcontext)
{
    if (glcontext->class->swap_buffers)
        glcontext->class->swap_buffers(glcontext);
}

void ngli_glcontext_freep(struct glcontext **glcontextp)
{
    struct glcontext *glcontext;

    if (!glcontextp || !*glcontextp)
        return;

    glcontext = *glcontextp;

    if (glcontext->class->uninit)
        glcontext->class->uninit(glcontext);

    free(glcontext->priv_data);
    free(glcontext);

    *glcontextp = NULL;
}

void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name)
{
    void *ptr = NULL;

    if (glcontext->class->get_proc_address)
        ptr = glcontext->class->get_proc_address(glcontext, name);

    return ptr;
}

void *ngli_glcontext_get_handle(struct glcontext *glcontext)
{
    void *handle = NULL;

    if (glcontext->class->get_handle)
        handle = glcontext->class->get_handle(glcontext);

    return handle;
}

void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext)
{
    void *texture_cache = NULL;

    if (glcontext->class->get_texture_cache)
        texture_cache = glcontext->class->get_texture_cache(glcontext);

    return texture_cache;
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

int ngli_glcontext_check_gl_error(struct glcontext *glcontext)
{
    const struct glfunctions *gl = &glcontext->funcs;

    const GLenum error = ngli_glGetError(gl);
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
        LOG(ERROR, "GL error: %s", errorstr);
    else
        LOG(ERROR, "GL error: %04x", error);

    return error;
}
