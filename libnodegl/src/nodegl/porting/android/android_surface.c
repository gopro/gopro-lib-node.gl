/*
 * Copyright 2016-2017 GoPro Inc.
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

#include <jni.h>
#include <pthread.h>
#include <time.h>
#include <libavcodec/mediacodec.h>

#include "android_utils.h"
#include "jni_utils.h"
#include "log.h"
#include "memory.h"
#include "android_handlerthread.h"
#include "android_surface.h"


static void native_on_frame_available(JNIEnv *env, jobject object, jlong surface_)
{
    struct android_surface *surface = (struct android_surface *)surface_;

    ngli_android_surface_signal_frame(surface);
}

static jobject surface_listener_new(struct android_surface *surface)
{
    jobject listener = NULL;
    jclass listener_class = NULL;
    jmethodID init_id = NULL;
    jmethodID set_native_ptr_id = NULL;

    static const JNINativeMethod methods[] = {
        {"nativeOnFrameAvailable", "(J)V", (void *)&native_on_frame_available},
    };

    JNIEnv *env = ngli_jni_get_env();
    if (!env)
        return NULL;

    listener_class = ngli_android_find_application_class(env, "org/nodegl/OnFrameAvailableListener");
    if (!listener_class)
        goto done;

    (*env)->RegisterNatives(env, listener_class, methods, NGLI_ARRAY_NB(methods));
    if (ngli_jni_exception_check(env, 1) < 0)
        goto done;

    init_id = (*env)->GetMethodID(env, listener_class, "<init>", "()V");
    if (ngli_jni_exception_check(env, 1) < 0)
        goto done;

    set_native_ptr_id = (*env)->GetMethodID(env, listener_class, "setNativePtr", "(J)V");
    if (ngli_jni_exception_check(env, 1) < 0)
        goto done;

    listener = (*env)->NewObject(env, listener_class, init_id);
    if (ngli_jni_exception_check(env, 1) < 0)
        goto done;

    (*env)->CallVoidMethod(env, listener, set_native_ptr_id, (jlong)surface);
    if (ngli_jni_exception_check(env, 1) < 0)
        goto done;

done:
    (*env)->DeleteLocalRef(env, listener_class);

    return listener;
}

struct jni_android_surface_fields {

    jclass surface_class;
    jmethodID surface_init_id;
    jmethodID surface_release_id;

    jclass surface_texture_class;
    jmethodID surface_texture_init_id;
    jmethodID surface_texture_init2_id;
    jmethodID attach_to_gl_context_id;
    jmethodID detach_from_gl_context_id;
    jmethodID update_tex_image_id;
    jmethodID set_on_frame_available_listener_id;
    jmethodID set_on_frame_available_listener2_id;
    jmethodID get_transform_matrix_id;
    jmethodID set_default_buffer_size_id;
    jmethodID surface_texture_release_id;

};

static const struct JniField jfields_mapping[] = {
    {"android/view/Surface", NULL, NULL, NGLI_JNI_CLASS, offsetof(struct jni_android_surface_fields, surface_class), 1},
        {"android/view/Surface", "<init>", "(Landroid/graphics/SurfaceTexture;)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, surface_init_id), 1},
        {"android/view/Surface", "release", "()V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, surface_release_id), 1},

    {"android/graphics/SurfaceTexture", NULL, NULL, NGLI_JNI_CLASS, offsetof(struct jni_android_surface_fields, surface_texture_class), 1},
        {"android/graphics/SurfaceTexture", "<init>", "(I)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, surface_texture_init_id), 1},
        {"android/graphics/SurfaceTexture", "<init>", "(IZ)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, surface_texture_init2_id), 0},
        {"android/graphics/SurfaceTexture", "attachToGLContext", "(I)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, attach_to_gl_context_id), 1},
        {"android/graphics/SurfaceTexture", "detachFromGLContext", "()V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, detach_from_gl_context_id), 1},
        {"android/graphics/SurfaceTexture", "updateTexImage", "()V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, update_tex_image_id), 1},
        {"android/graphics/SurfaceTexture", "getTransformMatrix", "([F)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, get_transform_matrix_id), 1},
        {"android/graphics/SurfaceTexture", "setDefaultBufferSize", "(II)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, set_default_buffer_size_id), 1},
        {"android/graphics/SurfaceTexture", "setOnFrameAvailableListener", "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, set_on_frame_available_listener_id), 1},
        {"android/graphics/SurfaceTexture", "setOnFrameAvailableListener", "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;Landroid/os/Handler;)V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, set_on_frame_available_listener2_id), 0},
        {"android/graphics/SurfaceTexture", "release", "()V", NGLI_JNI_METHOD, offsetof(struct jni_android_surface_fields, surface_texture_release_id), 1},

    {NULL}
};

struct android_surface {
    struct jni_android_surface_fields jfields;
    jobject surface;
    jobject surface_texture;
    jobject listener;
    jfloatArray transformation_matrix;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int tex_id;
    int on_frame_available;
};

struct android_surface *ngli_android_surface_new(int tex_id, void *handler)
{
    jobject listener = NULL;
    jobject surface = NULL;
    jobject surface_texture = NULL;
    jfloatArray transformation_matrix = NULL;

    struct android_surface *ret = ngli_calloc(1, sizeof(*ret));
    if (!ret)
        return NULL;

    pthread_mutex_init(&ret->lock, NULL);
    pthread_cond_init(&ret->cond, NULL);

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_free(ret);
        return NULL;
    }

    if (ngli_jni_init_jfields(env, &ret->jfields, jfields_mapping, 1) < 0)
        goto fail;

    surface_texture = (*env)->NewObject(env,
                                        ret->jfields.surface_texture_class,
                                        ret->jfields.surface_texture_init_id,
                                        tex_id);
    if (!surface_texture) {
        goto fail;
    }

    ret->surface_texture = (*env)->NewGlobalRef(env, surface_texture);
    if (!ret->surface_texture)
        goto fail;
    ret->tex_id = tex_id;

    surface = (*env)->NewObject(env,
                                ret->jfields.surface_class,
                                ret->jfields.surface_init_id,
                                ret->surface_texture);
    if (!surface)
        goto fail;

    ret->surface = (*env)->NewGlobalRef(env, surface);
    if (!ret->surface)
        goto fail;

    listener = surface_listener_new(ret);
    if (listener) {
        ret->listener = (*env)->NewGlobalRef(env, listener);
        if (!ret->listener)
            goto fail;

        if (ret->jfields.set_on_frame_available_listener2_id)
            (*env)->CallVoidMethod(env,
                                   ret->surface_texture,
                                   ret->jfields.set_on_frame_available_listener2_id,
                                   listener,
                                   handler);
        else
            (*env)->CallVoidMethod(env,
                                   ret->surface_texture,
                                   ret->jfields.set_on_frame_available_listener_id,
                                   listener);

        if (ngli_jni_exception_check(env, 1) < 0)
            goto fail;
    }

    transformation_matrix = (*env)->NewFloatArray(env, 16);
    if (!transformation_matrix)
        goto fail;

    ret->transformation_matrix = (*env)->NewGlobalRef(env, transformation_matrix);
    if (!ret->transformation_matrix)
        goto fail;

    (*env)->DeleteLocalRef(env, surface);
    (*env)->DeleteLocalRef(env, surface_texture);
    (*env)->DeleteLocalRef(env, listener);
    (*env)->DeleteLocalRef(env, transformation_matrix);

    return ret;
fail:
    (*env)->DeleteLocalRef(env, surface);
    (*env)->DeleteLocalRef(env, surface_texture);
    (*env)->DeleteLocalRef(env, listener);
    (*env)->DeleteLocalRef(env, transformation_matrix);

    ngli_android_surface_free(&ret);

    return NULL;
}

void ngli_android_surface_free(struct android_surface **surface)
{
    if (!*surface)
        return;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_freep(surface);
        return;
    }

    if ((*surface)->surface) {
        (*env)->CallVoidMethod(env, (*surface)->surface, (*surface)->jfields.surface_release_id);
        if (ngli_jni_exception_check(env, 1) < 0) {
            goto fail;
        }
    }

    if ((*surface)->surface_texture) {
        (*env)->CallVoidMethod(env, (*surface)->surface_texture, (*surface)->jfields.surface_texture_release_id);
        if (ngli_jni_exception_check(env, 1) < 0) {
            goto fail;
        }
    }

fail:
    (*env)->DeleteGlobalRef(env, (*surface)->surface);
    (*env)->DeleteGlobalRef(env, (*surface)->surface_texture);
    (*env)->DeleteGlobalRef(env, (*surface)->listener);
    (*env)->DeleteGlobalRef(env, (*surface)->transformation_matrix);

    ngli_jni_reset_jfields(env, &(*surface)->jfields, jfields_mapping, 1);

    pthread_mutex_destroy(&(*surface)->lock);
    pthread_cond_destroy(&(*surface)->cond);
    ngli_freep(surface);
}

void *ngli_android_surface_get_surface(struct android_surface *surface)
{
    if (!surface)
        return NULL;

    return surface->surface;
}

int ngli_android_surface_attach_to_gl_context(struct android_surface *surface, int tex_id)
{
    int ret = 0;

    if (!surface)
        return 0;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_free(surface);
        return -1;
    }

    if (surface->tex_id != tex_id)
        ngli_android_surface_detach_from_gl_context(surface);

    (*env)->CallVoidMethod(env,
                           surface->surface_texture,
                           surface->jfields.attach_to_gl_context_id,
                           tex_id);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;
    surface->tex_id = tex_id;

fail:
    return ret;
}

int ngli_android_surface_detach_from_gl_context(struct android_surface *surface)
{
    int ret = 0;

    if (!surface || surface->tex_id < 0)
        return 0;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_free(surface);
        return -1;
    }

    (*env)->CallVoidMethod(env,
                           surface->surface_texture,
                           surface->jfields.detach_from_gl_context_id);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;
    surface->tex_id = -1;

fail:
    return ret;
}

int ngli_android_surface_render_buffer(struct android_surface *surface, AVMediaCodecBuffer *buffer, float *matrix)
{
    int ret = 0;

    if (!surface)
        return 0;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_free(surface);
        return -1;
    }

    pthread_mutex_lock(&surface->lock);
    surface->on_frame_available = 0;

    if (av_mediacodec_release_buffer(buffer, 1) < 0) {
        pthread_mutex_unlock(&surface->lock);
        ret = -1;
        goto fail;
    }

    if (surface->listener && !surface->on_frame_available) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&surface->cond, &surface->lock, &ts);
    }

    ret = surface->on_frame_available;
    pthread_mutex_unlock(&surface->lock);

    if (!ret)
        LOG(WARNING, "no frame available");

    (*env)->CallVoidMethod(env, surface->surface_texture, surface->jfields.update_tex_image_id);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;

    (*env)->CallVoidMethod(env,
                           surface->surface_texture,
                           surface->jfields.get_transform_matrix_id,
                           surface->transformation_matrix);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;

    (*env)->GetFloatArrayRegion(env, surface->transformation_matrix, 0, 16, matrix);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;

fail:

    return ret;
}

void ngli_android_surface_signal_frame(struct android_surface *surface)
{
    if (!surface)
        return;

    pthread_mutex_lock(&surface->lock);
    surface->on_frame_available = 1;
    pthread_cond_signal(&surface->cond);
    pthread_mutex_unlock(&surface->lock);
}
