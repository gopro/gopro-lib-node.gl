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
#include "ngfx/porting/d3d/D3DGraphicsContext.h"
#include "ngfx/porting/d3d/D3DDebugUtil.h"
#include <wrl.h>
#include <dxgi1_4.h>
using namespace ngfx;
using namespace std;
using Microsoft::WRL::ComPtr;
#define MAX_DESCRIPTORS 1024

void D3DGraphicsContext::create(const char* appName, bool enableDepthStencil, bool debug) {
    HRESULT hResult;
    this->debug = debug;
    this->enableDepthStencil = enableDepthStencil;
    depthFormat = PIXELFORMAT_D16_UNORM;
    UINT dxgiFactoryFlags = 0;
    if (debug) {
        ComPtr<ID3D12Debug> debugController;
        V(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
        // Enable additional debug layers.
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
    V(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&d3dFactory)));
    d3dDevice.create(this);
    V(d3dDevice.v->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dDrawCommandAllocator)));
    V(d3dDevice.v->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dCopyCommandAllocator)));
    V(d3dDevice.v->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dComputeCommandAllocator)));
    d3dCommandQueue.create(this);
    createDescriptorHeaps();
}

void D3DGraphicsContext::createDescriptorHeaps() {
    d3dRtvDescriptorHeap.create(d3dDevice.v.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 
        MAX_DESCRIPTORS, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    d3dCbvSrvUavDescriptorHeap.create(d3dDevice.v.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
        MAX_DESCRIPTORS * 3, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    d3dSamplerDescriptorHeap.create(d3dDevice.v.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 
        MAX_DESCRIPTORS, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    d3dDsvDescriptorHeap.create(d3dDevice.v.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        MAX_DESCRIPTORS, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
}

void D3DGraphicsContext::setSurface(Surface* surface) {
    defaultOffscreenSurfaceFormat = PIXELFORMAT_RGBA8_UNORM;
    if (surface && !surface->offscreen) {
        offscreen = false;
		d3dSwapchain.create(this, d3d(surface));
		surfaceFormat = PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
        numDrawCommandBuffers = d3dSwapchain.numImages;
    }
    else {
        offscreen = true;
        numDrawCommandBuffers = 1;
        surfaceFormat = defaultOffscreenSurfaceFormat;
    }
    d3dDrawCommandLists.resize(numDrawCommandBuffers);
    for (auto& cmdList : d3dDrawCommandLists) {
        cmdList.create(d3dDevice.v.Get(), d3dDrawCommandAllocator.Get());
    }
    d3dCopyCommandList.create(d3dDevice.v.Get(), d3dCopyCommandAllocator.Get());
    d3dComputeCommandList.create(d3dDevice.v.Get(), d3dComputeCommandAllocator.Get());
    if (surface && numSamples != 1) {
        TODO();
    }
    if (surface && enableDepthStencil) {
        d3dDepthStencilView.reset(
            (D3DTexture*)Texture::create(
                this, nullptr, nullptr, depthFormat, 
                surface->w * surface->h * 4, surface->w, surface->h, 1, 1, 
                IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            )
        );
        if (numSamples != 1) {
            TODO();
        }
    }
    if (surface && !surface->offscreen) d3dDefaultRenderPass = (D3DRenderPass*)getRenderPass({ 
        false, enableDepthStencil, false, numSamples, 1
    });
    defaultOffscreenSurfaceFormat = PIXELFORMAT_RGBA8_UNORM;
    d3dDefaultOffscreenRenderPass = (D3DRenderPass*)getRenderPass({ true, enableDepthStencil, false, numSamples, 1 });
    if (surface && !surface->offscreen) {
        createSwapchainFramebuffers(surface->w, surface->h);
    }
    //initSemaphores(vkDevice.v);
    createFences(d3dDevice.v.Get());
    createBindings();
}

RenderPass* D3DGraphicsContext::getRenderPass(RenderPassConfig config) {
    for (auto& r : d3dRenderPassCache) {
        if (r->config == config) return &r->d3dRenderPass;
    }
    auto renderPassData = make_unique<D3DRenderPassData>();
    if (config.numSamples != 1) {
        if (config.offscreen) createOffscreenRenderPassMSAA(config, renderPassData->d3dRenderPass);
        else createRenderPassMSAA(config, renderPassData->d3dRenderPass);
    }
    else {
        if (config.offscreen) createOffscreenRenderPass(config, renderPassData->d3dRenderPass);
        else createRenderPass(config, renderPassData->d3dRenderPass);
    }
    auto result = &renderPassData->d3dRenderPass;
    d3dRenderPassCache.emplace_back(std::move(renderPassData));
    return result;
}

void D3DGraphicsContext::createRenderPass(const RenderPassConfig& config, D3DRenderPass& renderPass) {
    renderPass.create(this, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
}
void D3DGraphicsContext::createOffscreenRenderPass(const RenderPassConfig& config, D3DRenderPass& renderPass) {
    renderPass.create(this, D3D12_RESOURCE_STATE_RENDER_TARGET, 
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
void D3DGraphicsContext::createRenderPassMSAA(const RenderPassConfig& config, D3DRenderPass& renderPass) {
    renderPass.create(this, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
}
void D3DGraphicsContext::createOffscreenRenderPassMSAA(const RenderPassConfig& config, D3DRenderPass& renderPass) {
    renderPass.create(this, D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
void D3DGraphicsContext::createFences(ID3D12Device* device) {
    d3dWaitFences.resize(numDrawCommandBuffers);
    for (auto& fence: d3dWaitFences) {
        fence.create(device, D3DFence::SIGNALED);
    }
    d3dComputeFence.create(device);
}

void D3DGraphicsContext::createSwapchainFramebuffers(int w, int h) {
    // Create frame buffers for every swap chain image
    d3dSwapchainFramebuffers.resize(d3dSwapchain.numImages);
    for (uint32_t i = 0; i < d3dSwapchainFramebuffers.size(); i++) {
        //TODO: add support for MSAA
        std::vector<D3DFramebuffer::D3DAttachment> attachments = {
            {
                d3dSwapchain.renderTargets[i].Get(),
                d3dSwapchain.renderTargetDescriptors[i].cpuHandle,
                0,
                IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                1,
                DXGI_FORMAT(surfaceFormat),
                nullptr
            }
        };
        if (enableDepthStencil) {
            attachments.push_back({
                d3dDepthStencilView->v.Get(), 
                d3dDepthStencilView->dsvDescriptor.cpuHandle,
                0,
                IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                1,
                DXGI_FORMAT(depthFormat),
                d3dDepthStencilView.get()
            });
        }
        d3dSwapchainFramebuffers[i].create(attachments, w, h);
    }
}

CommandBuffer* D3DGraphicsContext::drawCommandBuffer(int32_t index) {
    if (index == -1) index = currentImageIndex;
    return &d3dDrawCommandLists[index];
}

CommandBuffer* D3DGraphicsContext::copyCommandBuffer() {
    return &d3dCopyCommandList;
}

CommandBuffer* D3DGraphicsContext::computeCommandBuffer() {
    return &d3dComputeCommandList;
}

void D3DGraphicsContext::createBindings() {
    device = &d3dDevice;
    queue = &d3dCommandQueue;
    defaultRenderPass = offscreen ? d3dDefaultOffscreenRenderPass : d3dDefaultRenderPass;
    defaultOffscreenRenderPass = d3dDefaultOffscreenRenderPass;
    swapchain = &d3dSwapchain;
    frameFences.resize(d3dWaitFences.size());
    for (int j = 0; j < d3dWaitFences.size(); j++)
        frameFences[j] = &d3dWaitFences[j];
    swapchainFramebuffers.resize(d3dSwapchainFramebuffers.size());
    for (int j = 0; j < d3dSwapchainFramebuffers.size(); j++)
        swapchainFramebuffers[j] = &d3dSwapchainFramebuffers[j];
    pipelineCache = &d3dPipelineCache;
}

GraphicsContext* GraphicsContext::create(const char* appName, bool enableDepthStencil, bool debug) {
    LOG("debug: %s", (debug) ? "true" : "false");
    auto d3dGraphicsContext = new D3DGraphicsContext();
    d3dGraphicsContext->create(appName, enableDepthStencil, debug);
    return d3dGraphicsContext;
}
