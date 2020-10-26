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

#ifndef JNI_UTILS_H
#define JNI_UTILS_H

#include <jni.h>
#include <sys/types.h>

/*
 * Attach permanently a JNI environment to the current thread and retrieve it.
 *
 * If successfully attached, the JNI environment will automatically be detached
 * at thread destruction.
 *
 * @param attached pointer to an integer that will be set to 1 if the
 * environment has been attached to the current thread or 0 if it is
 * already attached.
 * @return the JNI environment on success, NULL otherwise
 */
JNIEnv *ngli_jni_get_env(void);

/*
 * Convert a jstring to its utf characters equivalent.
 *
 * @param env JNI environment
 * @param string Java string to convert
 * @return a pointer to an array of unicode characters on success, NULL
 * otherwise
 */
char *ngli_jni_jstring_to_utf_chars(JNIEnv *env, jstring string);

/*
 * Convert utf chars to its jstring equivalent.
 *
 * @param env JNI environment
 * @param utf_chars a pointer to an array of unicode characters
 * @return a Java string object on success, NULL otherwise
 */
jstring ngli_jni_utf_chars_to_jstring(JNIEnv *env, const char *utf_chars);

/*
 * Extract the error summary from a jthrowable in the form of "className: errorMessage"
 *
 * @param env JNI environment
 * @param exception exception to get the summary from
 * @param error address pointing to the error, the value is updated if a
 * summary can be extracted
 * @return 0 on success, < 0 otherwise
 */
int ngli_jni_exception_get_summary(JNIEnv *env, jthrowable exception, char **error);

/*
 * Check if an exception has occurred, log it and clear it.
 *
 * @param env JNI environment
 * @param log value used to enable logging if an exception has occurred,
 * 0 disables logging, != 0 enables logging
 */
int ngli_jni_exception_check(JNIEnv *env, int log);

/*
 * Jni field type.
 */
enum JniFieldType {
    NGLI_JNI_CLASS,
    NGLI_JNI_FIELD,
    NGLI_JNI_STATIC_FIELD,
    NGLI_JNI_METHOD,
    NGLI_JNI_STATIC_METHOD,
};

/*
 * Jni field describing a class, a field or a method to be retrieved using
 * the ngli_jni_init_jfields method.
 */
struct JniField {
    const char *name;
    const char *method;
    const char *signature;
    enum JniFieldType type;
    size_t offset;
    int mandatory;
};

/*
 * Retrieve class references, field ids and method ids to an arbitrary structure.
 *
 * @param env JNI environment
 * @param jfields a pointer to an arbitrary structure where the different
 * fields are declared and where the FFJNIField mapping table offsets are
 * pointing to
 * @param jfields_mapping null terminated array of FFJNIFields describing
 * the class/field/method to be retrieved
 * @param global make the classes references global. It is the caller
 * responsibility to properly release global references.
 * @return 0 on success, < 0 otherwise
 */
int ngli_jni_init_jfields(JNIEnv *env, void *jfields, const struct JniField *jfields_mapping, int global);

/*
 * Delete class references, field ids and method ids of an arbitrary structure.
 *
 * @param env JNI environment
 * @param jfields a pointer to an arbitrary structure where the different
 * fields are declared and where the FFJNIField mapping table offsets are
 * pointing to
 * @param jfields_mapping null terminated array of FFJNIFields describing
 * the class/field/method to be deleted
 * @param global threat the classes references as global and delete them
 * accordingly
 * @return 0 on success, < 0 otherwise
 */
int ngli_jni_reset_jfields(JNIEnv *env, void *jfields, const struct JniField *jfields_mapping, int global);

#endif /* JNI_UTILS_H */
