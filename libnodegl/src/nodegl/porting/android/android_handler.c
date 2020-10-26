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

#include <jni.h>

#include "jni_utils.h"
#include "log.h"
#include "android_handler.h"
#include "memory.h"

struct jni_android_handler_fields {
    jclass handler_class;
    jmethodID init_id;
};

static const struct JniField android_handler_mapping[] = {
    {"android/os/Handler", NULL, NULL, NGLI_JNI_CLASS, offsetof(struct jni_android_handler_fields, handler_class), 1},
        {"android/os/Handler", "<init>", "()V", NGLI_JNI_METHOD, offsetof(struct jni_android_handler_fields, init_id), 1},
    {NULL}
};

struct android_handler {
    struct jni_android_handler_fields jfields;
    jobject handler;
};

struct android_handler *ngli_android_handler_new(void)
{
    jobject handler = NULL;

    struct android_handler *ret = ngli_calloc(1, sizeof(*ret));
    if (!ret)
        return NULL;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_free(ret);
        return NULL;
    }

    if (ngli_jni_init_jfields(env, &ret->jfields, android_handler_mapping, 1) < 0)
        goto fail;

    handler = (*env)->NewObject(env, ret->jfields.handler_class, ret->jfields.init_id);
    if (ngli_jni_exception_check(env, 1) < 0)
        goto fail;

    ret->handler = (*env)->NewGlobalRef(env, handler);
    if (!ret->handler)
        goto fail;

    (*env)->DeleteLocalRef(env, handler);

    return ret;
fail:
    (*env)->DeleteLocalRef(env, handler);

    ngli_android_handler_free(&ret);
    return NULL;
}

void *ngli_android_handler_get_native_handler(struct android_handler *handler)
{
    if (!handler)
        return NULL;

    return handler->handler;
}

void ngli_android_handler_free(struct android_handler **handler)
{
    if (!*handler)
        return;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_freep(handler);
        return;
    }

    (*env)->DeleteGlobalRef(env, (*handler)->handler);

    ngli_jni_reset_jfields(env, &(*handler)->jfields, android_handler_mapping, 1);

    ngli_freep(handler);
}
