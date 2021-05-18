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

#include <stdio.h>
#include <stdlib.h>
#include "gpu_capture.h"
#include "log.h"
#include "utils.h"
#include "memory.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include "renderdoc_app.h"

struct gpu_capture_ctx {
    RENDERDOC_API_1_4_0 *rdoc_api;
#ifdef _WIN32
    HMODULE mod;
#else
    void *mod;
#endif
};

struct gpu_capture_ctx *gpu_capture_ctx_create(struct gpu_ctx *gpu_ctx)
{
    struct gpu_capture_ctx *s = ngli_calloc(1, sizeof(*s));
    return s;
}

int gpu_capture_init(struct gpu_capture_ctx *s)
{
#ifdef _WIN32
    s->mod = LoadLibraryA("renderdoc.dll");
    if (!s->mod) {
        LOG(ERROR, "could not load renderdoc.dll");
        return NGL_ERROR_UNSUPPORTED;
    }
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)GetProcAddress(s->mod, "RENDERDOC_GetAPI");
#else
    s->mod = dlopen("librenderdoc.so", RTLD_LAZY);
    if (!s->mod) {
        LOG(ERROR, "could not load renderdoc.so: %s", dlerror());
        return NGL_ERROR_UNSUPPORTED;
    }
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        dlsym(s->mod, "RENDERDOC_GetAPI");
#endif
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_0, (void **)&s->rdoc_api);
    if (ret == 0) {
        LOG(ERROR, "could not initialize renderdoc");
        return NGL_ERROR_EXTERNAL;
    }
    LOG(INFO, "renderdoc capture path: %s", s->rdoc_api->GetCaptureFilePathTemplate());
    return 0;
}

int gpu_capture_begin(struct gpu_capture_ctx *s)
{
    s->rdoc_api->StartFrameCapture(NULL, NULL);
    return 0;
}

int gpu_capture_end(struct gpu_capture_ctx *s)
{
    int ret = s->rdoc_api->EndFrameCapture(NULL, NULL);
    if (ret == 0) {
        LOG(ERROR, "end frame capture failed");
        return NGL_ERROR_GENERIC;
    }
    return 0;
}

void gpu_capture_freep(struct gpu_capture_ctx **sp)
{
    if (!*sp)
        return;

    struct gpu_capture_ctx *s = *sp;
    if (s->mod)
#ifdef _WIN32
        FreeLibrary(s->mod);
#else
        dlclose(s->mod);
#endif
    ngli_freep(sp);
}

