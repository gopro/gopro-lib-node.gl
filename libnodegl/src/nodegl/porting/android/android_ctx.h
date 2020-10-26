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

#ifndef ANDROID_CTX_H
#define ANDROID_CTX_H

#include <android/native_window_jni.h>
#include <media/NdkImageReader.h>

struct gctx;

struct android_ctx {
    void *libandroid_handle;
    void *libmediandk_handle;

    /* AImage */
    void (*AImage_delete)(AImage *image);
    media_status_t (*AImage_getHardwareBuffer)(const AImage *image, AHardwareBuffer **buffer);

    /* AImageReader */
    media_status_t (*AImageReader_new)(int32_t width, int32_t height, int32_t format, int32_t maxImages, AImageReader **reader);
    media_status_t (*AImageReader_setImageListener)(AImageReader *reader, AImageReader_ImageListener *listener);
    media_status_t (*AImageReader_getWindow)(AImageReader *reader, ANativeWindow **window);
    media_status_t (*AImageReader_acquireNextImage)(AImageReader *reader, AImage **image);
    void (*AImageReader_delete)(AImageReader *reader);

    /* ANativeWindow */
    jobject (*ANativeWindow_toSurface)(JNIEnv* env, ANativeWindow* window);

    int has_native_imagereader_api;
};

int ngli_android_ctx_init(struct gctx *gctx, struct android_ctx *s);
void ngli_android_ctx_reset(struct android_ctx *s);

#endif
