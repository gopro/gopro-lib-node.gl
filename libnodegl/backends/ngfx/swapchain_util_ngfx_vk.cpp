/*
 * Copyright 2021 GoPro Inc.
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

#include "swapchain_util_ngfx_vk.h"
#include "ngfx/graphics/GraphicsContext.h"
using namespace ngfx;

swapchain_util_ngfx *swapchain_util_ngfx::create(ngfx::GraphicsContext *ctx, uintptr_t window) {
    return new swapchain_util_ngfx_vk(ctx, window);
}

void swapchain_util_ngfx_vk::acquire_image() {
    ctx->swapchain->acquireNextImage();
}

void swapchain_util_ngfx_vk::present(CommandBuffer *cmd_buffer) {
    ctx->submit(cmd_buffer);
    ctx->queue->present();
}
