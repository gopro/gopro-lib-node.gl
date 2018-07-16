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
#include <string.h>

#if defined(TARGET_ANDROID)
#include <jni.h>

#include "jni_utils.h"
#endif

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "glcontext.h"

struct ngl_ctx *ngl_create(void)
{
    struct ngl_ctx *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    LOG(INFO, "context create in node.gl v%d.%d.%d",
        NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    return s;
}

static int cmd_reconfigure(struct ngl_ctx *s, void *arg)
{
    if (s->glcontext->wrapped)
        return -1;

    const struct ngl_config *config = arg;

    int ret = ngli_glcontext_resize(s->glcontext, config->width, config->height);
    if (ret < 0)
        return ret;

    struct ngl_config *current_config = &s->config;
    current_config->width = config->width;
    current_config->height = config->height;

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);
        memcpy(current_config->viewport, config->viewport, sizeof(config->viewport));
    }

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);
    memcpy(current_config->clear_color, config->clear_color, sizeof(config->clear_color));

    return 0;
}

static int cmd_configure(struct ngl_ctx *s, void *arg)
{
    const struct ngl_config *config = arg;
    memcpy(&s->config, config, sizeof(s->config));

    s->glcontext = ngli_glcontext_new(&s->config);
    if (!s->glcontext)
        return -1;

    if (!s->glcontext->wrapped) {
        ngli_glcontext_make_current(s->glcontext, 1);
        if (s->config.swap_interval >= 0)
            ngli_glcontext_set_swap_interval(s->glcontext, s->config.swap_interval);
    }

    int ret = ngli_glcontext_load_extensions(s->glcontext);
    if (ret < 0)
        return ret;

    ngli_glstate_probe(s->glcontext, &s->glstate);

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0)
        ngli_glViewport(s->glcontext, viewport[0], viewport[1], viewport[2], viewport[3]);

    const float *rgba = config->clear_color;
    ngli_glClearColor(s->glcontext, rgba[0], rgba[1], rgba[2], rgba[3]);

    return 0;
}

#if TARGET_IPHONE
static int cmd_make_current(struct ngl_ctx *s, void *arg)
{
    const int current = *(int *)arg;
    ngli_glcontext_make_current(s->glcontext, current);
    return 0;
}
#endif

static int cmd_set_scene(struct ngl_ctx *s, void *arg)
{
    if (s->scene) {
        ngli_node_detach_ctx(s->scene);
        ngl_node_unrefp(&s->scene);
    }

    struct ngl_node *scene = arg;
    if (!scene)
        return 0;

    int ret = ngli_node_attach_ctx(scene, s);
    if (ret < 0)
        return ret;

    s->scene = ngl_node_ref(scene);
    return 0;
}

static int cmd_prepare_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;
    const struct glcontext *gl = s->glcontext;

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    struct ngl_node *scene = s->scene;
    if (!scene) {
        return 0;
    }

    LOG(DEBUG, "prepare scene %s @ t=%f", scene->name, t);
    int ret = ngli_node_visit(scene, 1, t);
    if (ret < 0)
        return ret;

    ret = ngli_node_honor_release_prefetch(scene, t);
    if (ret < 0)
        return ret;

    ret = ngli_node_update(scene, t);
    if (ret < 0)
        return ret;

    return 0;
}

static int cmd_draw(struct ngl_ctx *s, void *arg)
{
    const double t = *(double *)arg;
    struct glcontext *gl = s->glcontext;

    int ret = cmd_prepare_draw(s, arg);
    if (ret < 0)
        goto end;

    if (s->scene) {
        LOG(DEBUG, "draw scene %s @ t=%f", s->scene->name, t);
        ngli_node_draw(s->scene);
    }
end:
    if (ret == 0 && ngli_glcontext_check_gl_error(gl, __FUNCTION__))
        ret = -1;

    if (gl->set_surface_pts)
        ngli_glcontext_set_surface_pts(gl, t);

    if (!gl->wrapped)
        ngli_glcontext_swap_buffers(gl);

    return ret;
}

static int cmd_stop(struct ngl_ctx *s, void *arg)
{
    ngli_glcontext_freep(&s->glcontext);
    return 0;
}

static int dispatch_cmd(struct ngl_ctx *s, cmd_func_type cmd_func, void *arg)
{
    if (!s->has_thread)
        return cmd_func(s, arg);

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

#if TARGET_IPHONE
#define MAKE_CURRENT &(int[]){1}
#define DONE_CURRENT &(int[]){0}
static int reconfigure_ios(struct ngl_ctx *s, struct ngl_config *config)
{
    if (!s->has_thread)
        return cmd_reconfigure(s, config);

    int ret = dispatch_cmd(s, cmd_make_current, DONE_CURRENT);
    if (ret < 0)
        return ret;

    cmd_make_current(s, MAKE_CURRENT);
    ret = cmd_reconfigure(s, config);
    cmd_make_current(s, DONE_CURRENT);

    int ret_dispatch = dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
    return ret ? ret : ret_dispatch;
}

static int configure_ios(struct ngl_ctx *s, struct ngl_config *config)
{
    if (!s->has_thread)
        return cmd_configure(s, config);

    int ret = cmd_configure(s, config);
    if (ret < 0)
        return ret;
    cmd_make_current(s, DONE_CURRENT);

    return dispatch_cmd(s, cmd_make_current, MAKE_CURRENT);
}
#endif

int ngl_configure(struct ngl_ctx *s, struct ngl_config *config)
{
    if (!config) {
        LOG(ERROR, "context configuration cannot be NULL");
        return -1;
    }

    if (s->configured)
#if TARGET_IPHONE
        return reconfigure_ios(s, config);
#else
        return dispatch_cmd(s, cmd_reconfigure, config);
#endif

    s->has_thread = !config->wrapped;
    if (s->has_thread) {
        int ret;
        if ((ret = pthread_mutex_init(&s->lock, NULL)) ||
            (ret = pthread_cond_init(&s->cond_ctl, NULL)) ||
            (ret = pthread_cond_init(&s->cond_wkr, NULL)) ||
            (ret = pthread_create(&s->worker_tid, NULL, worker_thread, s))) {
            pthread_cond_destroy(&s->cond_ctl);
            pthread_cond_destroy(&s->cond_wkr);
            pthread_mutex_destroy(&s->lock);
            return ret;
        }
    }

#if TARGET_IPHONE
    int ret = configure_ios(s, config);
#else
    int ret = dispatch_cmd(s, cmd_configure, config);
#endif
    if (ret < 0)
        return ret;

    s->configured = 1;
    return 0;
}

int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before setting a scene");
        return -1;
    }

    return dispatch_cmd(s, cmd_set_scene, scene);
}

int ngli_prepare_draw(struct ngl_ctx *s, double t)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before updating");
        return -1;
    }

    return dispatch_cmd(s, cmd_prepare_draw, &t);
}

int ngl_draw(struct ngl_ctx *s, double t)
{
    if (!s->configured) {
        LOG(ERROR, "context must be configured before drawing");
        return -1;
    }

    return dispatch_cmd(s, cmd_draw, &t);
}

void ngl_free(struct ngl_ctx **ss)
{
    struct ngl_ctx *s = *ss;

    if (!s)
        return;

    if (s->configured) {
        ngl_set_scene(s, NULL);
        dispatch_cmd(s, cmd_stop, NULL);

        if (s->has_thread) {
            pthread_join(s->worker_tid, NULL);
            pthread_cond_destroy(&s->cond_ctl);
            pthread_cond_destroy(&s->cond_wkr);
            pthread_mutex_destroy(&s->lock);
        }
    }
    free(*ss);
    *ss = NULL;
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
        return -1;

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
    return -1;
}

void *ngl_jni_get_java_vm(void)
{
    return NULL;
}

int ngl_android_set_application_context(void *application_context)
{
    return -1;
}

void *ngl_android_get_application_context(void)
{
    return NULL;
}
#endif
