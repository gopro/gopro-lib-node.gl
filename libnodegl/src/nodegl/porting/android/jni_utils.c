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
#include <pthread.h>
#include <stdlib.h>

#include "bstr.h"
#include "jni_utils.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "utils.h"

static JavaVM *java_vm;
static pthread_key_t current_env;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void jni_detach_env(void *data)
{
    if (java_vm) {
        (*java_vm)->DetachCurrentThread(java_vm);
    }
}

static void jni_create_pthread_key(void)
{
    pthread_key_create(&current_env, jni_detach_env);
}

JNIEnv *ngli_jni_get_env(void)
{
    int ret = 0;
    JNIEnv *env = NULL;

    pthread_mutex_lock(&lock);
    if (java_vm == NULL) {
        java_vm = ngl_jni_get_java_vm();
    }

    if (!java_vm) {
        LOG(ERROR, "no Java virtual machine has been registered");
        goto done;
    }

    pthread_once(&once, jni_create_pthread_key);

    if ((env = pthread_getspecific(current_env)) != NULL) {
        goto done;
    }

    ret = (*java_vm)->GetEnv(java_vm, (void **)&env, JNI_VERSION_1_6);
    switch(ret) {
    case JNI_EDETACHED:
        if ((*java_vm)->AttachCurrentThread(java_vm, &env, NULL) != 0) {
            LOG(ERROR, "failed to attach the JNI environment to the current thread");
            env = NULL;
        } else {
            pthread_setspecific(current_env, env);
        }
        break;
    case JNI_OK:
        break;
    case JNI_EVERSION:
        LOG(ERROR, "the specified JNI version is not supported");
        break;
    default:
        LOG(ERROR, "failed to get the JNI environment attached to this thread");
        break;
    }

done:
    pthread_mutex_unlock(&lock);
    return env;
}

char *ngli_jni_jstring_to_utf_chars(JNIEnv *env, jstring string)
{
    char *ret = NULL;
    const char *utf_chars = NULL;

    jboolean copy = 0;

    if (!string) {
        return NULL;
    }

    utf_chars = (*env)->GetStringUTFChars(env, string, &copy);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "String.getStringUTFChars() threw an exception");
        return NULL;
    }

    ret = ngli_strdup(utf_chars);

    (*env)->ReleaseStringUTFChars(env, string, utf_chars);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "String.releaseStringUTFChars() threw an exception");
        return NULL;
    }

    return ret;
}

jstring ngli_jni_utf_chars_to_jstring(JNIEnv *env, const char *utf_chars)
{
    jstring ret;

    ret = (*env)->NewStringUTF(env, utf_chars);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "NewStringUTF() threw an exception");
        return NULL;
    }

    return ret;
}

int ngli_jni_exception_get_summary(JNIEnv *env, jthrowable exception, char **error)
{
    int ret = 0;

    struct bstr *bstr = NULL;

    char *name = NULL;
    char *message = NULL;

    jclass class_class = NULL;
    jmethodID get_name_id = NULL;

    jclass exception_class = NULL;
    jmethodID get_message_id = NULL;

    jstring string = NULL;

    bstr = ngli_bstr_create();

    exception_class = (*env)->GetObjectClass(env, exception);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "could not find Throwable class");
        ret = -1;
        goto done;
    }

    class_class = (*env)->GetObjectClass(env, exception_class);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "could not find Throwable class's class");
        ret = -1;
        goto done;
    }

    get_name_id = (*env)->GetMethodID(env, class_class, "getName", "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "could not find method Class.getName()");
        ret = -1;
        goto done;
    }

    string = (*env)->CallObjectMethod(env, exception_class, get_name_id);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "Class.getName() threw an exception");
        ret = -1;
        goto done;
    }

    if (string) {
        name = ngli_jni_jstring_to_utf_chars(env, string);
        (*env)->DeleteLocalRef(env, string);
        string = NULL;
    }

    get_message_id = (*env)->GetMethodID(env, exception_class, "getMessage", "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "could not find method java/lang/Throwable.getMessage()");
        ret = -1;
        goto done;
    }

    string = (*env)->CallObjectMethod(env, exception, get_message_id);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        LOG(ERROR, "Throwable.getMessage() threw an exception");
        ret = -1;
        goto done;
    }

    if (string) {
        message = ngli_jni_jstring_to_utf_chars(env, string);
        (*env)->DeleteLocalRef(env, string);
        string = NULL;
    }

    if (name && message) {
        ngli_bstr_printf(bstr, "%s: %s", name, message);
    } else if (name && !message) {
        ngli_bstr_printf(bstr, "%s occurred", name);
    } else if (!name && message) {
        ngli_bstr_printf(bstr, "Exception: %s", message);
    } else {
        LOG(WARNING, "could not retrieve exception name and message");
        ngli_bstr_print(bstr, "Exception occurred");
    }

    *error = ngli_bstr_strdup(bstr);
    ngli_bstr_freep(&bstr);
done:

    ngli_free(name);
    ngli_free(message);

    (*env)->DeleteLocalRef(env, class_class);
    (*env)->DeleteLocalRef(env, exception_class);
    (*env)->DeleteLocalRef(env, string);

    return ret;
}

int ngli_jni_exception_check(JNIEnv *env, int log)
{
    int ret;

    jthrowable exception;

    char *message = NULL;

    if (!(*(env))->ExceptionCheck((env))) {
        return 0;
    }

    if (!log) {
        (*(env))->ExceptionClear((env));
        return -1;
    }

    exception = (*env)->ExceptionOccurred(env);
    (*(env))->ExceptionClear((env));

    if ((ret = ngli_jni_exception_get_summary(env, exception, &message)) < 0) {
        (*env)->DeleteLocalRef(env, exception);
        return ret;
    }

    (*env)->DeleteLocalRef(env, exception);

    LOG(ERROR, "%s", message);
    ngli_free(message);

    return -1;
}

int ngli_jni_init_jfields(JNIEnv *env, void *jfields, const struct JniField *jfields_mapping, int global)
{
    int i, ret = 0;
    jclass last_clazz = NULL;

    for (i = 0; jfields_mapping[i].name; i++) {
        int mandatory = jfields_mapping[i].mandatory;
        enum JniFieldType type = jfields_mapping[i].type;

        if (type == NGLI_JNI_CLASS) {
            jclass clazz;

            last_clazz = NULL;

            clazz = (*env)->FindClass(env, jfields_mapping[i].name);
            if ((ret = ngli_jni_exception_check(env, mandatory)) < 0 && mandatory) {
                goto done;
            }

            last_clazz = *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset) =
                    global ? (*env)->NewGlobalRef(env, clazz) : clazz;

            if (global) {
                (*env)->DeleteLocalRef(env, clazz);
            }

        } else {

            if (!last_clazz) {
                ret = -1;
                break;
            }

            switch(type) {
            case NGLI_JNI_FIELD: {
                jfieldID field_id = (*env)->GetFieldID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = ngli_jni_exception_check(env, mandatory)) < 0 && mandatory) {
                    goto done;
                }

                *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = field_id;
                break;
            }
            case NGLI_JNI_STATIC_FIELD: {
                jfieldID field_id = (*env)->GetStaticFieldID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = ngli_jni_exception_check(env, mandatory)) < 0 && mandatory) {
                    goto done;
                }

                *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = field_id;
                break;
            }
            case NGLI_JNI_METHOD: {
                jmethodID method_id = (*env)->GetMethodID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = ngli_jni_exception_check(env, mandatory)) < 0 && mandatory) {
                    goto done;
                }

                *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = method_id;
                break;
            }
            case NGLI_JNI_STATIC_METHOD: {
                jmethodID method_id = (*env)->GetStaticMethodID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = ngli_jni_exception_check(env, mandatory)) < 0 && mandatory) {
                    goto done;
                }

                *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = method_id;
                break;
            }
            default:
                LOG(ERROR, "unknown JNI field type");
                ret = -1;
                goto done;
            }

            ret = 0;
        }
    }

done:
    if (ret < 0) {
        /* reset jfields in case of failure so it does not leak references */
        ngli_jni_reset_jfields(env, jfields, jfields_mapping, global);
    }

    return ret;
}

int ngli_jni_reset_jfields(JNIEnv *env, void *jfields, const struct JniField *jfields_mapping, int global)
{
    int i;

    for (i = 0; jfields_mapping[i].name; i++) {
        enum JniFieldType type = jfields_mapping[i].type;

        switch(type) {
        case NGLI_JNI_CLASS: {
            jclass clazz = *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset);
            if (!clazz)
                continue;

            if (global) {
                (*env)->DeleteGlobalRef(env, clazz);
            } else {
                (*env)->DeleteLocalRef(env, clazz);
            }

            *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case NGLI_JNI_FIELD: {
            *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case NGLI_JNI_STATIC_FIELD: {
            *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case NGLI_JNI_METHOD: {
            *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case NGLI_JNI_STATIC_METHOD: {
            *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        default:
            LOG(ERROR, "unknown JNI field type");
        }
    }

    return 0;
}
