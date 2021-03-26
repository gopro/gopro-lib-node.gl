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

#include <Metal/Metal.h>
#include "gpu_capture.h"
#include "config.h"
#include "log.h"
#include "memory.h"
#include "utils.h"

#if defined(BACKEND_VK)
#include <MoltenVK/vk_mvk_moltenvk.h>
#include "backends/vk/gctx_vk.h"
#endif

struct gpu_capture_ctx {
    struct gctx *gctx;
};

struct gpu_capture_ctx *gpu_capture_ctx_create(struct gctx *gctx)
{
    struct gpu_capture_ctx *s = ngli_calloc(1, sizeof(*s));
    s->gctx = gctx;
    return s;
}

int gpu_capture_init(struct gpu_capture_ctx *s)
{
    return 0;
}

int gpu_capture_begin(struct gpu_capture_ctx *s)
{
    NSError *error;
    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    MTLCaptureDescriptor *captureDescriptor = [[MTLCaptureDescriptor alloc] init];
#if defined(BACKEND_VK)
    if (s->gctx->config.backend == NGL_BACKEND_VULKAN) {
        struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
        struct vkcontext *vkcontext = gctx_vk->vkcontext;
        id<MTLDevice> mtl_device;
        vkGetMTLDeviceMVK(vkcontext->phy_device, &mtl_device);
        captureDescriptor.captureObject = mtl_device;
    }
#endif
    captureDescriptor.destination = MTLCaptureDestinationDeveloperTools;
    if (![captureManager startCaptureWithDescriptor: captureDescriptor error:&error]) {
        NSLog(@"Failed to start capture, error %@", error);
        [captureDescriptor release];
        return NGL_ERROR_EXTERNAL;
    }
    [captureDescriptor release];
    return 0;
}

int gpu_capture_end(struct gpu_capture_ctx *s)
{
    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    [captureManager stopCapture];
    return 0;
}

void gpu_capture_freep(struct gpu_capture_ctx **sp)
{
    if (!*sp)
        return;

    ngli_freep(sp);
}
