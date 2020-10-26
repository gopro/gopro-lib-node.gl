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

#include <pthread.h>
#include <stdlib.h>

#include "android_looper.h"
#include "android_handler.h"
#include "android_handlerthread.h"
#include "memory.h"

struct android_handlerthread {
    pthread_t tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct android_looper *looper;
    struct android_handler *handler;
};

static void *run(void * data)
{
    int ret;
    struct android_handlerthread *thread = data;

    pthread_mutex_lock(&thread->lock);

    thread->looper = ngli_android_looper_new();
    if (!thread->looper)
        goto fail;

    ret = ngli_android_looper_prepare(thread->looper);
    if (ret < 0)
        goto fail;

    thread->handler = ngli_android_handler_new();
    if (!thread->handler)
        goto fail;

    pthread_cond_signal(&thread->cond);
    pthread_mutex_unlock(&thread->lock);

    ngli_android_looper_loop(thread->looper);

    ngli_android_handler_free(&thread->handler);
    ngli_android_looper_free(&thread->looper);

    return NULL;
fail:
    ngli_android_handler_free(&thread->handler);
    ngli_android_looper_free(&thread->looper);

    pthread_cond_signal(&thread->cond);
    pthread_mutex_unlock(&thread->lock);

    return NULL;
}

struct android_handlerthread *ngli_android_handlerthread_new(void)
{
    struct android_handlerthread *thread = ngli_calloc(1, sizeof(*thread));
    if (!thread)
        return NULL;

    pthread_mutex_init(&thread->lock, NULL);
    pthread_cond_init(&thread->cond, NULL);
    pthread_mutex_lock(&thread->lock);

    if (pthread_create(&thread->tid, NULL, run, thread)) {
        pthread_mutex_unlock(&thread->lock);
        pthread_mutex_destroy(&thread->lock);
        pthread_cond_destroy(&thread->cond);
        ngli_free(thread);
        return NULL;
    }

    pthread_cond_wait(&thread->cond, &thread->lock);
    pthread_mutex_unlock(&thread->lock);

    if (!thread->handler) {
        ngli_android_handlerthread_free(&thread);
        return NULL;
    }

    return thread;
}

void *ngli_android_handlerthread_get_native_handler(struct android_handlerthread *thread)
{
    if (!thread)
        return NULL;

    return ngli_android_handler_get_native_handler(thread->handler);
}

void ngli_android_handlerthread_free(struct android_handlerthread **threadp)
{
    struct android_handlerthread *thread = *threadp;
    if (!thread)
        return;

    ngli_android_looper_quit(thread->looper);
    pthread_join(thread->tid, NULL);
    pthread_mutex_destroy(&thread->lock);
    pthread_cond_destroy(&thread->cond);

    ngli_freep(threadp);
}
