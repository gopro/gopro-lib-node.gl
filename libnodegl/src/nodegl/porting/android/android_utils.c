/*
 * Copyright 2015-2017 GoPro Inc.
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
#include <stdlib.h>

#include "android_utils.h"
#include "jni_utils.h"
#include "log.h"

jclass ngli_android_find_application_class(JNIEnv *env, const char *name)
{
    int ret;

    jobject application_context = NULL;
    jclass application_context_class = NULL;
    jmethodID get_class_loader_id = NULL;

    jobject class_loader = NULL;
    jclass class_loader_class = NULL;
    jmethodID find_class_id = NULL;

    jobject jname = NULL;
    jclass clazz = NULL;

    application_context = ngl_android_get_application_context();
    if (!application_context) {
        LOG(ERROR, "no application context has been registered");
        return NULL;
    }

    application_context_class = (*env)->GetObjectClass(env, application_context);
    if (!application_context_class) {
        return NULL;
    }

    get_class_loader_id = (*env)->GetMethodID(env, application_context_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if ((ret = ngli_jni_exception_check(env, 1)) < 0) {
        goto done;
    }

    class_loader = (*env)->CallObjectMethod(env, application_context, get_class_loader_id);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0) {
        goto done;
    }

    class_loader_class = (*env)->GetObjectClass(env, class_loader);
    if (!class_loader_class) {
        goto done;
    }

    find_class_id = (*env)->GetMethodID(env, class_loader_class, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if ((ret = ngli_jni_exception_check(env, 1)) < 0) {
        goto done;
    }

    jname = ngli_jni_utf_chars_to_jstring(env, name);
    if (!jname) {
        goto done;
    }

    clazz = (*env)->CallObjectMethod(env, class_loader, find_class_id, jname);
    if ((ret = ngli_jni_exception_check(env, 1)) < 0) {
        goto done;
    }
done:
    if (application_context_class) {
        (*env)->DeleteLocalRef(env, application_context_class);
    }

    if (class_loader) {
        (*env)->DeleteLocalRef(env, class_loader);
    }

    if (class_loader_class) {
        (*env)->DeleteLocalRef(env, class_loader_class);
    }

    if (jname) {
        (*env)->DeleteLocalRef(env, jname);
    }

    return clazz;
}
