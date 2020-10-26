/*
 * Copyright 2020 GoPro Inc.
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
#include <jni.h>
#include <android/native_window.h>
#include <media/NdkImageReader.h>

#include "android_imagereader.h"
#include "jni_utils.h"
#include "log.h"
#include "memory.h"

struct android_image {
    struct android_ctx *android_ctx;
    AImage *image;
};

struct android_imagereader {
    struct android_ctx *android_ctx;
    AImageReader *reader;
    jobject window;

    pthread_mutex_t lock;
    pthread_cond_t cond;
    int buffer_available;
};

AHardwareBuffer *ngli_android_image_get_hardware_buffer(struct android_image *s)
{
    struct android_ctx *android_ctx = s->android_ctx;

    AHardwareBuffer *hardware_buffer;
    media_status_t status = android_ctx->AImage_getHardwareBuffer(s->image, &hardware_buffer);
    if (status != AMEDIA_OK)
        return NULL;
    return hardware_buffer;
}

void ngli_android_image_freep(struct android_image **sp)
{
    if (!*sp)
        return;

    struct android_image *s = *sp;
    struct android_ctx *android_ctx = s->android_ctx;
    android_ctx->AImage_delete(s->image);

    ngli_freep(sp);
}

static void on_buffer_available(void *context, AImageReader *reader)
{
    struct android_imagereader *s = context;
    pthread_mutex_lock(&s->lock);
    s->buffer_available = 1;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->lock);
}

struct android_imagereader *ngli_android_imagereader_create(struct android_ctx *android_ctx, int width, int height, int format, int max_images)
{
    if (!android_ctx->has_native_imagereader_api)
        return NULL;

    struct android_imagereader *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->android_ctx = android_ctx;
    if (pthread_mutex_init(&s->lock, NULL)) {
        ngli_freep(&s);
        return NULL;
    }

    if (pthread_cond_init(&s->cond, NULL)) {
        pthread_mutex_destroy(&s->lock);
        ngli_freep(&s);
        return NULL;
    }

    media_status_t status = android_ctx->AImageReader_new(width, height, format, max_images, &s->reader);
    if (status != AMEDIA_OK) {
        LOG(ERROR, "failed to allocate AImageReader");
        goto fail;
    }

    AImageReader_ImageListener listener = {
        .context = s,
        .onImageAvailable = on_buffer_available,
    };

    status = android_ctx->AImageReader_setImageListener(s->reader, &listener);
    if (status != AMEDIA_OK) {
        LOG(ERROR, "failed to set image listener");
        goto fail;
    }

    return s;
fail:

    pthread_mutex_destroy(&s->lock);
    pthread_cond_destroy(&s->cond);
    android_ctx->AImageReader_delete(s->reader);
    ngli_freep(&s);

    return NULL;
}

int ngli_android_imagereader_get_window(struct android_imagereader *s, void **window)
{
    struct android_ctx *android_ctx = s->android_ctx;

    if (s->window) {
        *window = s->window;
        return 0;
    }

    ANativeWindow *native_window;
    media_status_t status = android_ctx->AImageReader_getWindow(s->reader, &native_window);
    if (status != AMEDIA_OK)
        return NGL_ERROR_EXTERNAL;

    JNIEnv *env = ngli_jni_get_env();
    if (!env)
        return NGL_ERROR_EXTERNAL;

    jobject object = android_ctx->ANativeWindow_toSurface(env, native_window);
    s->window = (*env)->NewGlobalRef(env, object);
    (*env)->DeleteLocalRef(env, object);

    *window = s->window;
    return *window ? 0 : NGL_ERROR_EXTERNAL;
}

int ngli_android_imagereader_acquire_next_image(struct android_imagereader *s, struct android_image **imagep)
{
    struct android_ctx *android_ctx = s->android_ctx;

    AImage *android_image;
    media_status_t status;
    int nb_retries = 0;

    pthread_mutex_lock(&s->lock);
    for (;;) {
        status = android_ctx->AImageReader_acquireNextImage(s->reader, &android_image);
        if (!nb_retries++ && status == AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&s->cond, &s->lock, &ts);
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&s->lock);

    if (status != AMEDIA_OK)
        return NGL_ERROR_EXTERNAL;

    struct android_image *image = ngli_calloc(1, sizeof(*image));
    if (!image) {
        android_ctx->AImage_delete(android_image);
        return NGL_ERROR_MEMORY;
    }
    image->android_ctx = android_ctx;
    image->image = android_image;
    *imagep = image;

    return 0;
}

void ngli_android_imagereader_freep(struct android_imagereader **sp)
{
    if (!*sp)
        return;

    struct android_imagereader *s = *sp;
    struct android_ctx *android_ctx = s->android_ctx;
    android_ctx->AImageReader_delete(s->reader);

    pthread_mutex_destroy(&s->lock);
    pthread_cond_destroy(&s->cond);

    JNIEnv *env = ngli_jni_get_env();
    if (env)
        (*env)->DeleteGlobalRef(env, s->window);

    ngli_freep(sp);
}
