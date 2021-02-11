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

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "config.h"

#if defined(TARGET_ANDROID)
#include <jni.h>

#include "jni_utils.h"
#endif

#include "darray.h"
#include "gctx.h"
#include "graphicstate.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "pgcache.h"
#include "rnode.h"

#if defined(HAVE_VAAPI)
#include "vaapi.h"
#endif

#if defined(TARGET_DARWIN) || defined(TARGET_IPHONE)
#if defined(BACKEND_GL)
#include "backends/gl/gctx_gl.h"
#endif
#endif

#if defined(TARGET_IPHONE) || defined(TARGET_ANDROID)
# define DEFAULT_BACKEND NGL_BACKEND_OPENGLES
#else
# define DEFAULT_BACKEND NGL_BACKEND_OPENGL
#endif

static int get_default_platform(void)
{
#if defined(TARGET_LINUX)
    return NGL_PLATFORM_XLIB;
#elif defined(TARGET_IPHONE)
    return NGL_PLATFORM_IOS;
#elif defined(TARGET_DARWIN)
    return NGL_PLATFORM_MACOS;
#elif defined(TARGET_ANDROID)
    return NGL_PLATFORM_ANDROID;
#elif defined(TARGET_WINDOWS)
    return NGL_PLATFORM_WINDOWS;
#else
    return NGL_ERROR_UNSUPPORTED;
#endif
}

static int cmd_stop(struct ngl_ctx *s, void *arg)
{
#if defined(HAVE_VAAPI)
    ngli_vaapi_ctx_reset(&s->vaapi_ctx);
#endif
#if defined(TARGET_ANDROID)
    ngli_android_ctx_reset(&s->android_ctx);
#endif
    ngli_texture_freep(&s->font_atlas); // allocated by the first node text
    ngli_pgcache_reset(&s->pgcache);
    ngli_hud_freep(&s->hud);
    ngli_gctx_freep(&s->gctx);

    return 0;
}

static int cmd_configure(struct ngl_ctx *s, void *arg)
{
    struct ngl_config *config = arg;

    if (s->scene)
        ngli_node_detach_ctx(s->scene, s);
    ngli_rnode_clear(&s->rnode);

    cmd_stop(s, arg);

    if (config->backend == NGL_BACKEND_AUTO)
        config->backend = DEFAULT_BACKEND;

    if (config->platform == NGL_PLATFORM_AUTO)
        config->platform = get_default_platform();
    if (config->platform < 0) {
        LOG(ERROR, "can not determine which platform to use");
        return config->platform;
    }

    s->config = *config;

    s->gctx = ngli_gctx_create(config);
    if (!s->gctx)
        return NGL_ERROR_MEMORY;

    int ret = ngli_gctx_init(s->gctx);
    if (ret < 0) {
        LOG(ERROR, "unable to initialize gpu context");
        ngli_gctx_freep(&s->gctx);
        return ret;
    }

    s->rnode_pos->graphicstate = NGLI_GRAPHICSTATE_DEFAULTS;
    s->rnode_pos->rendertarget_desc = *ngli_gctx_get_default_rendertarget_desc(s->gctx);

    ret = ngli_pgcache_init(&s->pgcache, s->gctx);
    if (ret < 0)
        return ret;

#if defined(HAVE_VAAPI)
    ret = ngli_vaapi_ctx_init(s->gctx, &s->vaapi_ctx);
    if (ret < 0)
        LOG(WARNING, "could not initialize vaapi context");
#endif

#if defined(TARGET_ANDROID)
    struct android_ctx *android_ctx = &s->android_ctx;
    ret = ngli_android_ctx_init(s->gctx, android_ctx);
    if (ret < 0)
        LOG(WARNING, "could not initialize Android context");
#endif

    NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    ngli_gctx_transform_projection_matrix(s->gctx, matrix);
    ngli_darray_clear(&s->projection_matrix_stack);
    if (!ngli_darray_push(&s->projection_matrix_stack, matrix))
        return NGL_ERROR_MEMORY;

    if (s->scene) {
        ret = ngli_node_attach_ctx(s->scene, s);
        if (ret < 0) {
            ngli_node_detach_ctx(s->scene, s);
            ngl_node_unrefp(&s->scene);
            cmd_stop(s, arg);
            return ret;
        }
    }

    if (config->hud) {
        s->hud = ngli_hud_create(s);
        if (!s->hud)
            return NGL_ERROR_MEMORY;

        ret = ngli_hud_init(s->hud);
        if (ret < 0)
            return ret;
    }

    return 0;
}

struct resize_params {
    int width;
    int height;
    const int *viewport;
};

static int cmd_resize(struct ngl_ctx *s, void *arg)
{
    const struct resize_params *params = arg;
    return ngli_gctx_resize(s->gctx, params->width, params->height, params->viewport);
}

static int cmd_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer)
{
    struct ngl_config *config = &s->config;

    int ret = ngli_gctx_set_capture_buffer(s->gctx, capture_buffer);
    if (ret < 0) {
        if (s->scene) {
            ngli_node_detach_ctx(s->scene, s);
            ngl_node_unrefp(&s->scene);
        }
        cmd_stop(s, NULL);
        config->capture_buffer = NULL;
        return ret;
    }

    config->capture_buffer = capture_buffer;

    return 0;
}

static int cmd_set_scene(struct ngl_ctx *s, void *arg)
{
    if (s->scene) {
        ngli_node_detach_ctx(s->scene, s);
        ngl_node_unrefp(&s->scene);
    }
    ngli_rnode_clear(&s->rnode);

    s->rnode_pos->graphicstate = NGLI_GRAPHICSTATE_DEFAULTS;
    s->rnode_pos->rendertarget_desc = *ngli_gctx_get_default_rendertarget_desc(s->gctx);

    struct ngl_node *scene = arg;
    if (!scene)
        return 0;

    int ret = ngli_node_attach_ctx(scene, s);
    if (ret < 0) {
        ngli_node_detach_ctx(scene, s);
        return ret;
    }

    s->scene = ngl_node_ref(scene);

    const struct ngl_config *config = &s->config;
    if (config->hud) {
        ngli_hud_freep(&s->hud);

        s->hud = ngli_hud_create(s);
        if (!s->hud)
            return NGL_ERROR_MEMORY;

        ret = ngli_hud_init(s->hud);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int cmd_prepare_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;

    struct ngl_node *scene = s->scene;
    if (!scene) {
        return 0;
    }

    LOG(DEBUG, "prepare scene %s @ t=%f", scene->label, t);

    const int64_t start_time = s->hud ? ngli_gettime_relative() : 0;

    ngli_darray_clear(&s->activitycheck_nodes);
    int ret = ngli_node_visit(scene, 1, t);
    if (ret < 0)
        return ret;

    ret = ngli_node_honor_release_prefetch(&s->activitycheck_nodes);
    if (ret < 0)
        return ret;

    ret = ngli_node_update(scene, t);
    if (ret < 0)
        return ret;

    s->cpu_update_time = s->hud ? ngli_gettime_relative() - start_time : 0;

    return 0;
}

static int cmd_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;

    int ret = cmd_prepare_draw(s, arg);
    if (ret < 0)
        return ret;

    ret = ngli_gctx_begin_draw(s->gctx, t);
    if (ret < 0)
        goto end;

    const int64_t cpu_start_time = s->hud ? ngli_gettime_relative() : 0;

    struct rendertarget *rt = ngli_gctx_get_default_rendertarget(s->gctx);
    s->available_rendertargets[0] = rt;
    s->available_rendertargets[1] = rt;
    s->current_rendertarget = rt;
    s->begin_render_pass = 0;

    struct ngl_node *scene = s->scene;
    if (scene) {
        LOG(DEBUG, "draw scene %s @ t=%f", scene->label, t);
        ngli_node_draw(scene);
    }

    if (s->hud) {
        s->cpu_draw_time = ngli_gettime_relative() - cpu_start_time;

        if (!s->begin_render_pass) {
            ngli_gctx_end_render_pass(s->gctx);
            s->current_rendertarget = s->available_rendertargets[1];
            s->begin_render_pass = 1;
        }
        ngli_gctx_query_draw_time(s->gctx, &s->gpu_draw_time);

        ngli_hud_draw(s->hud);
    }

end:;
    int end_ret = ngli_gctx_end_draw(s->gctx, t);
    if (end_ret < 0)
        return end_ret;

    return ret;
}

static int dispatch_cmd(struct ngl_ctx *s, cmd_func_type cmd_func, void *arg)
{
    pthread_mutex_lock(&s->lock);
    s->cmd_func = cmd_func;
    s->cmd_arg = arg;
    pthread_cond_signal(&s->cond_wkr);
    while (s->cmd_func)
        pthread_cond_wait(&s->cond_ctl, &s->lock);
    pthread_mutex_unlock(&s->lock);

    return s->cmd_ret;
}

static void *worker_thread(void *arg)
{
    struct ngl_ctx *s = arg;

    ngli_thread_set_name("ngl-thread");

    pthread_mutex_lock(&s->lock);
    for (;;) {
        while (!s->cmd_func)
            pthread_cond_wait(&s->cond_wkr, &s->lock);
        s->cmd_ret = s->cmd_func(s, s->cmd_arg);
        int need_stop = s->cmd_func == cmd_stop;
        s->cmd_func = s->cmd_arg = NULL;
        pthread_cond_signal(&s->cond_ctl);

        if (need_stop)
            break;
    }
    pthread_mutex_unlock(&s->lock);

    return NULL;
}

#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
static int cmd_make_current(struct ngl_ctx *s, void *arg)
{
#if defined(BACKEND_GL)
    const struct ngl_config *config = &s->config;
    if (config->backend == NGL_BACKEND_OPENGL ||
        config->backend == NGL_BACKEND_OPENGLES) {
        const int current = *(int *)arg;
        struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
        ngli_glcontext_make_current(gctx_gl->glcontext, current);
    }
#endif
    return 0;
}

#define MAKE_CURRENT &(int[]){1}
#define DONE_CURRENT &(int[]){0}
static int configure_ios(struct ngl_ctx *s, struct ngl_config *config)
{
    int ret = cmd_configure(s, config);
    if (ret < 0)
        return ret;
    cmd_make_current(s, DONE_CURRENT);

    return dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
}

static int resize_ios(struct ngl_ctx *s, const struct resize_params *params)
{
    int ret = dispatch_cmd(s, cmd_make_current, DONE_CURRENT);
    if (ret < 0)
        return ret;

    cmd_make_current(s, MAKE_CURRENT);
    ret = cmd_resize(s, params);
    if (ret < 0)
        return ret;
    cmd_make_current(s, DONE_CURRENT);

    return dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
}
#endif

static void stop_thread(struct ngl_ctx *s)
{
    dispatch_cmd(s, cmd_stop, NULL);
    pthread_join(s->worker_tid, NULL);
    pthread_cond_destroy(&s->cond_ctl);
    pthread_cond_destroy(&s->cond_wkr);
    pthread_mutex_destroy(&s->lock);
}

static const char *get_cap_string_id(unsigned cap_id)
{
    switch (cap_id) {
    case NGL_CAP_BLOCK:                         return "block";
    case NGL_CAP_COMPUTE:                       return "compute";
    case NGL_CAP_INSTANCED_DRAW:                return "instanced_draw";
    case NGL_CAP_MAX_COLOR_ATTACHMENTS:         return "max_color_attachments";
    case NGL_CAP_MAX_COMPUTE_GROUP_COUNT_X:     return "max_compute_group_count_x";
    case NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Y:     return "max_compute_group_count_y";
    case NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Z:     return "max_compute_group_count_z";
    case NGL_CAP_MAX_COMPUTE_GROUP_INVOCATIONS: return "max_compute_group_invocations";
    case NGL_CAP_MAX_COMPUTE_GROUP_SIZE_X:      return "max_compute_group_size_x";
    case NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Y:      return "max_compute_group_size_y";
    case NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Z:      return "max_compute_group_size_z";
    case NGL_CAP_MAX_SAMPLES:                   return "max_samples";
    case NGL_CAP_NPOT_TEXTURE:                  return "npot_texture";
    case NGL_CAP_SHADER_TEXTURE_LOD:            return "shader_texture_lod";
    case NGL_CAP_TEXTURE_3D:                    return "texture_3d";
    case NGL_CAP_TEXTURE_CUBE:                  return "texture_cube";
    case NGL_CAP_UINT_UNIFORMS:                 return "uint_uniforms";
    }
    ngli_assert(0);
}

#define CAP(cap_id, value) {cap_id, get_cap_string_id(cap_id), value}
#define ALL_FEATURES(features, mask) ((features & (mask)) == mask)
#define ANY_FEATURES(features, mask) ((features & (mask)) != 0)

static int load_caps(struct ngl_backend *backend, const struct gctx *gctx)
{
    const int has_block          = ANY_FEATURES(gctx->features, NGLI_FEATURE_BUFFER_OBJECTS_ALL);
    const int has_compute        = ALL_FEATURES(gctx->features, NGLI_FEATURE_COMPUTE_SHADER_ALL);
    const int has_instanced_draw = ALL_FEATURES(gctx->features, NGLI_FEATURE_INSTANCED_ARRAY);
    const int has_npot_texture   = ALL_FEATURES(gctx->features, NGLI_FEATURE_TEXTURE_NPOT);
    const int has_shader_texture_lod = ALL_FEATURES(gctx->features, NGLI_FEATURE_SHADER_TEXTURE_LOD);
    const int has_texture_3d     = ALL_FEATURES(gctx->features, NGLI_FEATURE_TEXTURE_3D);
    const int has_texture_cube   = ALL_FEATURES(gctx->features, NGLI_FEATURE_TEXTURE_CUBE_MAP);
    const int has_uint_uniforms  = ALL_FEATURES(gctx->features, NGLI_FEATURE_UINT_UNIFORMS);

    const struct limits *limits = &gctx->limits;
    const struct ngl_cap caps[] = {
        CAP(NGL_CAP_BLOCK,                         has_block),
        CAP(NGL_CAP_COMPUTE,                       has_compute),
        CAP(NGL_CAP_INSTANCED_DRAW,                has_instanced_draw),
        CAP(NGL_CAP_MAX_COLOR_ATTACHMENTS,         limits->max_color_attachments),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_COUNT_X,     limits->max_compute_work_group_count[0]),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Y,     limits->max_compute_work_group_count[1]),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_COUNT_Z,     limits->max_compute_work_group_count[2]),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_INVOCATIONS, limits->max_compute_work_group_invocations),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_SIZE_X,      limits->max_compute_work_group_size[0]),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Y,      limits->max_compute_work_group_size[1]),
        CAP(NGL_CAP_MAX_COMPUTE_GROUP_SIZE_Z,      limits->max_compute_work_group_size[2]),
        CAP(NGL_CAP_MAX_SAMPLES,                   limits->max_samples),
        CAP(NGL_CAP_NPOT_TEXTURE,                  has_npot_texture),
        CAP(NGL_CAP_SHADER_TEXTURE_LOD,            has_shader_texture_lod),
        CAP(NGL_CAP_TEXTURE_3D,                    has_texture_3d),
        CAP(NGL_CAP_TEXTURE_CUBE,                  has_texture_cube),
        CAP(NGL_CAP_UINT_UNIFORMS,                 has_uint_uniforms),
    };

    backend->nb_caps = NGLI_ARRAY_NB(caps);
    backend->caps = ngli_calloc(backend->nb_caps, sizeof(*backend->caps));
    if (!backend->caps)
        return NGL_ERROR_MEMORY;
    memcpy(backend->caps, caps, sizeof(caps));

    return 0;
}

static int backend_probe(struct ngl_backend *backend, const struct ngl_config *config)
{
    struct gctx *gctx = ngli_gctx_create(config);
    if (!gctx)
        return NGL_ERROR_MEMORY;

    int ret = ngli_gctx_init(gctx);
    if (ret < 0)
        goto end;

    backend->id         = config->backend;
    backend->string_id  = gctx->backend_str;
    backend->name       = gctx->class->name;

    ret = load_caps(backend, gctx);
    if (ret < 0)
        goto end;

end:
    ngli_gctx_freep(&gctx);
    return ret;
}

static const int backend_ids[] = {
#ifdef BACKEND_GL
    NGL_BACKEND_OPENGL,
    NGL_BACKEND_OPENGLES,
#endif
};

int ngl_backends_probe(const struct ngl_config *user_config, int *nb_backendsp, struct ngl_backend **backendsp)
{
    static const struct ngl_config default_config = {
        .width     = 1,
        .height    = 1,
        .offscreen = 1,
    };

    if (!user_config)
        user_config = &default_config;

    const int platform = user_config->platform == NGL_PLATFORM_AUTO ? get_default_platform() : user_config->platform;

    struct ngl_backend *backends = ngli_calloc(NGLI_ARRAY_NB(backend_ids), sizeof(*backends));
    if (!backends)
        return NGL_ERROR_MEMORY;
    int nb_backends = 0;

    for (int i = 0; i < NGLI_ARRAY_NB(backend_ids); i++) {
        if (user_config->backend != NGL_BACKEND_AUTO && user_config->backend != backend_ids[i])
            continue;
        struct ngl_config config = *user_config;
        config.backend = backend_ids[i];
        config.platform = platform;

        int ret = backend_probe(&backends[nb_backends], &config);
        if (ret < 0)
            continue;
        backends[nb_backends].is_default = backend_ids[i] == DEFAULT_BACKEND;

        nb_backends++;
    }

    if (!nb_backends)
        ngl_backends_freep(&backends);

    *backendsp = backends;
    *nb_backendsp = nb_backends;
    return 0;
}

void ngl_backends_freep(struct ngl_backend **backendsp)
{
    struct ngl_backend *backends = *backendsp;
    for (int i = 0; i < NGLI_ARRAY_NB(backend_ids); i++)
        ngli_free(backends[i].caps);
    ngli_freep(backendsp);
}

struct ngl_ctx *ngl_create(void)
{
    struct ngl_ctx *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    if (pthread_mutex_init(&s->lock, NULL) ||
        pthread_cond_init(&s->cond_ctl, NULL) ||
        pthread_cond_init(&s->cond_wkr, NULL) ||
        pthread_create(&s->worker_tid, NULL, worker_thread, s)) {
        pthread_cond_destroy(&s->cond_ctl);
        pthread_cond_destroy(&s->cond_wkr);
        pthread_mutex_destroy(&s->lock);
        ngli_free(s);
        return NULL;
    }

    ngli_rnode_init(&s->rnode);
    s->rnode_pos = &s->rnode;

    ngli_darray_init(&s->modelview_matrix_stack, 4 * 4 * sizeof(float), 1);
    ngli_darray_init(&s->projection_matrix_stack, 4 * 4 * sizeof(float), 1);
    ngli_darray_init(&s->activitycheck_nodes, sizeof(struct ngl_node *), 0);

    static const NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;
    if (!ngli_darray_push(&s->modelview_matrix_stack, id_matrix) ||
        !ngli_darray_push(&s->projection_matrix_stack, id_matrix))
        goto fail;

    LOG(INFO, "context create in node.gl v%d.%d.%d",
        NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);

    return s;

fail:
    ngli_rnode_reset(&s->rnode);
    ngl_freep(&s);
    return NULL;
}

int ngl_configure(struct ngl_ctx *s, struct ngl_config *config)
{
    if (!config) {
        LOG(ERROR, "context configuration cannot be NULL");
        return NGL_ERROR_INVALID_ARG;
    }

    if (config->offscreen) {
        if (config->width <= 0 || config->height <= 0) {
            LOG(ERROR,
                "could not initialize offscreen rendering with invalid dimensions (%dx%d)",
                config->width,
                config->height);
            return NGL_ERROR_INVALID_ARG;
        }
    } else {
        if (config->capture_buffer) {
            LOG(ERROR, "capture_buffer is only supported with offscreen rendering");
            return NGL_ERROR_INVALID_ARG;
        }
    }

    s->configured = 0;
#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    int ret = configure_ios(s, config);
#else
    int ret = dispatch_cmd(s, cmd_configure, config);
#endif
    if (ret < 0)
        return ret;
    s->configured = 1;
    return 0;
}

int ngl_resize(struct ngl_ctx *s, int width, int height, const int *viewport)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before resizing rendering buffers");
        return NGL_ERROR_INVALID_USAGE;
    }

    const struct ngl_config *config = &s->config;
    if (config->offscreen) {
        LOG(ERROR, "offscreen context does not support resize operation");
        return NGL_ERROR_INVALID_USAGE;
    }

    struct resize_params params = {
        .width = width,
        .height = height,
        .viewport = viewport,
    };

#if defined(TARGET_IPHONE) || defined(TARGET_DARWIN)
    return resize_ios(s, &params);
#else
    return dispatch_cmd(s, cmd_resize, &params);
#endif
}

int ngl_set_capture_buffer(struct ngl_ctx *s, void *capture_buffer)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before setting a capture buffer");
        return NGL_ERROR_INVALID_USAGE;
    }

    const struct ngl_config *config = &s->config;
    if (!config->offscreen) {
        LOG(ERROR, "capture buffers are only supported with offscreen rendering");
        return NGL_ERROR_INVALID_USAGE;
    }

    int ret = dispatch_cmd(s, cmd_set_capture_buffer, capture_buffer);
    if (ret < 0) {
        s->configured = 0;
        return ret;
    }
    return ret;
}

int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before setting a scene");
        return NGL_ERROR_INVALID_USAGE;
    }

    return dispatch_cmd(s, cmd_set_scene, scene);
}

int ngli_prepare_draw(struct ngl_ctx *s, double t)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before updating");
        return NGL_ERROR_INVALID_USAGE;
    }

    return dispatch_cmd(s, cmd_prepare_draw, &t);
}

int ngl_draw(struct ngl_ctx *s, double t)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before drawing");
        return NGL_ERROR_INVALID_USAGE;
    }

    return dispatch_cmd(s, cmd_draw, &t);
}

void ngl_freep(struct ngl_ctx **ss)
{
    struct ngl_ctx *s = *ss;

    if (!s)
        return;

    if (s->configured)
        ngl_set_scene(s, NULL);

    stop_thread(s);
    ngli_rnode_reset(&s->rnode);
    ngli_darray_reset(&s->modelview_matrix_stack);
    ngli_darray_reset(&s->projection_matrix_stack);
    ngli_darray_reset(&s->activitycheck_nodes);
    ngli_freep(ss);
}

#if defined(TARGET_ANDROID)
static void *java_vm;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int ngl_jni_set_java_vm(void *vm)
{
    int ret = 0;

    pthread_mutex_lock(&lock);
    if (java_vm == NULL) {
        java_vm = vm;
    } else if (java_vm != vm) {
        ret = -1;
        LOG(ERROR, "a Java virtual machine has already been set");
    }
    pthread_mutex_unlock(&lock);

    return ret;
}

void *ngl_jni_get_java_vm(void)
{
    void *vm;

    pthread_mutex_lock(&lock);
    vm = java_vm;
    pthread_mutex_unlock(&lock);

    return vm;
}

static void *android_application_context;

int ngl_android_set_application_context(void *application_context)
{
    JNIEnv *env;

    env = ngli_jni_get_env();
    if (!env)
        return NGL_ERROR_EXTERNAL;

    pthread_mutex_lock(&lock);

    if (android_application_context) {
        (*env)->DeleteGlobalRef(env, android_application_context);
        android_application_context = NULL;
    }

    if (application_context)
        android_application_context = (*env)->NewGlobalRef(env, application_context);

    pthread_mutex_unlock(&lock);

    return 0;
}

void *ngl_android_get_application_context(void)
{
    void *context;

    pthread_mutex_lock(&lock);
    context = android_application_context;
    pthread_mutex_unlock(&lock);

    return context;
}

#else
int ngl_jni_set_java_vm(void *vm)
{
    return NGL_ERROR_UNSUPPORTED;
}

void *ngl_jni_get_java_vm(void)
{
    return NULL;
}

int ngl_android_set_application_context(void *application_context)
{
    return NGL_ERROR_UNSUPPORTED;
}

void *ngl_android_get_application_context(void)
{
    return NULL;
}
#endif
