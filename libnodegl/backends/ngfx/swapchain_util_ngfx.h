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
 
#ifndef SWAPCHAIN_UTIL_NGFX_H
#define SWAPCHAIN_UTIL_NGFX_H

#include <cstdint>

namespace ngfx { 
    class GraphicsContext;
    class CommandBuffer;
}

class swapchain_util_ngfx {
public:
    static swapchain_util_ngfx *create(ngfx::GraphicsContext *ctx, uintptr_t window);
    swapchain_util_ngfx(ngfx::GraphicsContext *ctx, uintptr_t window)
        : ctx(ctx), window(window) {}
    virtual ~swapchain_util_ngfx() {}
    virtual void acquire_image() = 0;
    virtual void present(ngfx::CommandBuffer *cmd_buffer) = 0;
    
    ngfx::GraphicsContext *ctx = nullptr;
    uintptr_t window = 0;
};

#endif
