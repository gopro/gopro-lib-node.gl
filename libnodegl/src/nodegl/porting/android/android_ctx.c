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

#include <dlfcn.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "android_ctx.h"
#include "gctx.h"
#include "log.h"
#include "nodes.h"

#define NDK_LOAD_FUNC(handle, name) do {       \
    s->name = dlsym(handle, #name);            \
    if (!s->name) {                            \
        LOG(INFO, "missing %s symbol", #name); \
        goto done;                             \
    }                                          \
} while (0)

static int load_media_api(struct android_ctx *s)
{
    s->libmediandk_handle = dlopen("libmediandk.so", RTLD_NOW);
    if (!s->libmediandk_handle) {
        LOG(ERROR, "could not open libmediandk.so");
        return NGL_ERROR_UNSUPPORTED;
    }

    NDK_LOAD_FUNC(s->libmediandk_handle, AImage_delete);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImage_getHardwareBuffer);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_new);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_getWindow);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_acquireNextImage);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_setImageListener);
    NDK_LOAD_FUNC(s->libmediandk_handle, AImageReader_delete);

    return 0;

done:
    dlclose(s->libmediandk_handle);
    s->libmediandk_handle = NULL;
    return NGL_ERROR_UNSUPPORTED;
}

static int load_window_api(struct android_ctx *s)
{
    s->libandroid_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!s->libandroid_handle) {
        LOG(ERROR, "could not open libandroid.so");
        return NGL_ERROR_UNSUPPORTED;
    }

    NDK_LOAD_FUNC(s->libandroid_handle, ANativeWindow_toSurface);

    return 0;

done:
    dlclose(s->libandroid_handle);
    s->libandroid_handle = NULL;
    return NGL_ERROR_UNSUPPORTED;
}

int ngli_android_ctx_init(struct gctx *gctx, struct android_ctx *s)
{
    memset(s, 0, sizeof(*s));

    int ret = load_media_api(s);
    if (ret < 0) {
        LOG(INFO, "could not load native media API");
        return ret;
    }

    ret = load_window_api(s);
    if (ret < 0) {
        LOG(INFO, "could not load native window API");
        return ret;
    }

    const struct ngl_config *config = &gctx->config;
    const int features = NGLI_FEATURE_OES_EGL_EXTERNAL_IMAGE |
                         NGLI_FEATURE_EGL_ANDROID_GET_IMAGE_NATIVE_CLIENT_BUFFER;
    if (config->backend == NGL_BACKEND_OPENGLES && (gctx->features & features) == features)
        s->has_native_imagereader_api = 1;

    return 0;
}

void ngli_android_ctx_reset(struct android_ctx *s)
{
    if (s->libmediandk_handle)
        dlclose(s->libmediandk_handle);
    if (s->libandroid_handle)
        dlclose(s->libandroid_handle);
    memset(s, 0, sizeof(*s));
}
