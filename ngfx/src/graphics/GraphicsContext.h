/*
 * Copyright 2016 GoPro Inc.
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
#pragma once
#include "graphics/CommandBuffer.h"
#include "graphics/Graphics.h"
#include "graphics/Framebuffer.h"
#include "graphics/PipelineCache.h"
#include "graphics/Queue.h"
#include "compute/ComputePass.h"
#include "graphics/RenderPass.h"
#include "graphics/Swapchain.h"
#include "graphics/Device.h"
#include "graphics/Surface.h"
#include <vector>

namespace ngfx {
    class GraphicsContext {
    public:
        static GraphicsContext* create(const char* appName, bool enableDepthStencil = false, bool debug = true);
        virtual ~GraphicsContext() {}
        virtual void setSurface(Surface* surface) = 0;
        virtual void beginRenderPass(CommandBuffer* commandBuffer, Graphics* graphics) {
            auto framebuffer = swapchainFramebuffers[currentImageIndex];
            uint32_t w = framebuffer->w, h = framebuffer->h;
            graphics->beginRenderPass(commandBuffer, defaultRenderPass, framebuffer, clearColor);
            graphics->setViewport(commandBuffer, { 0, 0, w, h });
            graphics->setScissor(commandBuffer, { 0, 0, w, h });
        }
        virtual void beginOffscreenRenderPass(CommandBuffer* commandBuffer, Graphics* graphics, Framebuffer* outputFramebuffer) {
            graphics->beginRenderPass(commandBuffer, defaultOffscreenRenderPass, outputFramebuffer, clearColor);
            graphics->setViewport(commandBuffer, { 0, 0, outputFramebuffer->w, outputFramebuffer->h });
            graphics->setScissor(commandBuffer, { 0, 0, outputFramebuffer->w, outputFramebuffer->h });
        }
        virtual void endRenderPass(CommandBuffer* commandBuffer, Graphics* graphics) {
            graphics->endRenderPass(commandBuffer);
        }
        virtual void endOffscreenRenderPass(CommandBuffer* commandBuffer, Graphics* graphics) {
            graphics->endRenderPass(commandBuffer);
        }
        virtual void submit(CommandBuffer* commandBuffer) {
            queue->submit(commandBuffer);
        }
        Device* device;
        uint32_t numDrawCommandBuffers = 0;
        virtual CommandBuffer* drawCommandBuffer(int32_t index = -1) = 0;
        virtual CommandBuffer* copyCommandBuffer() = 0;
        virtual CommandBuffer* computeCommandBuffer() = 0;
        struct RenderPassConfig {
            inline bool operator==(const RenderPassConfig& rhs) {
                return offscreen == rhs.offscreen && enableDepthStencil == rhs.enableDepthStencil
                    && numSamples == rhs.numSamples
                    && numColorAttachments == rhs.numColorAttachments;
            }
            bool offscreen = false;
            bool enableDepthStencil = false, enableDepthStencilResolve = false;
            uint32_t numSamples = 1;
            uint32_t numColorAttachments = 1;
        };
        virtual RenderPass* getRenderPass(RenderPassConfig config) = 0;
        std::vector<Framebuffer*> swapchainFramebuffers;
        Queue* queue = nullptr;
        RenderPass *defaultRenderPass, *defaultOffscreenRenderPass;
        Swapchain* swapchain;
        uint32_t currentImageIndex = 0;
        std::vector<Fence*> frameFences;
        Fence* computeFence;
        Semaphore *presentCompleteSemaphore = nullptr, *renderCompleteSemaphore = nullptr;
        PipelineCache *pipelineCache;
        PixelFormat surfaceFormat = PIXELFORMAT_UNDEFINED, defaultOffscreenSurfaceFormat = PIXELFORMAT_UNDEFINED,
            depthFormat = PIXELFORMAT_UNDEFINED;
        glm::vec4 clearColor = glm::vec4(0.0f);
    protected:
        bool debug, enableDepthStencil = false;
    };
};
