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

#include "jni_utils.h"
#include "log.h"
#include "android_looper.h"
#include "memory.h"

struct jni_android_looper_fields {
    jclass looper_class;
    jmethodID prepare_id;
    jmethodID my_looper_id;
    jmethodID get_main_looper_id;
    jmethodID loop_id;
    jmethodID quit_id;
};

static const struct JniField android_looper_mapping[] = {
    {"android/os/Looper", NULL, NULL, NGLI_JNI_CLASS, offsetof(struct jni_android_looper_fields, looper_class), 1},
        {"android/os/Looper", "prepare", "()V", NGLI_JNI_STATIC_METHOD, offsetof(struct jni_android_looper_fields, prepare_id), 1},
        {"android/os/Looper", "myLooper", "()Landroid/os/Looper;", NGLI_JNI_STATIC_METHOD, offsetof(struct jni_android_looper_fields, my_looper_id), 1},
        {"android/os/Looper", "getMainLooper", "()Landroid/os/Looper;", NGLI_JNI_STATIC_METHOD, offsetof(struct jni_android_looper_fields, get_main_looper_id), 1},
        {"android/os/Looper", "loop", "()V", NGLI_JNI_STATIC_METHOD, offsetof(struct jni_android_looper_fields, loop_id), 1},
        {"android/os/Looper", "quit", "()V", NGLI_JNI_METHOD, offsetof(struct jni_android_looper_fields, quit_id), 1},

    {NULL}
};

struct android_looper {
    struct jni_android_looper_fields jfields;
    jobject looper;
};

struct android_looper *ngli_android_looper_new(void)
{
    struct android_looper *ret = ngli_calloc(1, sizeof(*ret));
    if (!ret)
        return NULL;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_free(ret);
        return NULL;
    }

    if (ngli_jni_init_jfields(env, &ret->jfields, android_looper_mapping, 1) < 0) {
        ngli_free(ret);
        return NULL;
    }

    return ret;
}

int ngli_android_looper_prepare(struct android_looper *looper)
{
    int ret = 0;
    jobject *my_looper = NULL;

    if (!looper)
        return 0;

    JNIEnv *env = ngli_jni_get_env();
    if (!env)
        return -1;

    (*env)->CallStaticVoidMethod(env, looper->jfields.looper_class, looper->jfields.prepare_id);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;

    my_looper = (*env)->CallStaticObjectMethod(env, looper->jfields.looper_class, looper->jfields.my_looper_id);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0)
        goto fail;

    looper->looper = (*env)->NewGlobalRef(env, my_looper);
    if (!looper->looper) {
        ret = -1;
        goto fail;
    }

fail:
    (*env)->DeleteLocalRef(env, my_looper);

    return ret;
}

int ngli_android_looper_loop(struct android_looper *looper)
{
    if (!looper)
        return 0;

    JNIEnv *env = ngli_jni_get_env();
    if (!env)
        return -1;

    (*env)->CallStaticVoidMethod(env, looper->jfields.looper_class, looper->jfields.loop_id);
    return ngli_jni_exception_check(env, 1);
}

int ngli_android_looper_quit(struct android_looper *looper)
{
    if (!looper || !looper->looper)
        return 0;

    JNIEnv *env = ngli_jni_get_env();
    if (!env)
        return -1;

    (*env)->CallVoidMethod(env, looper->looper, looper->jfields.quit_id);
    return ngli_jni_exception_check(env, 1);
}

void ngli_android_looper_free(struct android_looper **looper)
{
    if (!*looper)
        return;

    JNIEnv *env = ngli_jni_get_env();
    if (!env) {
        ngli_freep(looper);
        return;
    }

    (*env)->DeleteGlobalRef(env, (*looper)->looper);

    ngli_jni_reset_jfields(env, &(*looper)->jfields, android_looper_mapping, 1);

    ngli_freep(looper);
}
