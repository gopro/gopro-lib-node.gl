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

#ifndef GCTX_VK_H
#define GCTX_VK_H

#include "nodegl/gctx.h"
#include "nodegl/porting/vk/vkcontext.h"

struct gctx_vk {
    struct gctx parent;
    struct vkcontext *vkcontext;

    shaderc_compiler_t spirv_compiler;
    shaderc_compile_options_t spirv_compiler_opts;

    VkCommandPool transient_command_buffer_pool;
    VkFence transient_command_buffer_fence;

    VkCommandPool command_buffer_pool;
    VkCommandBuffer *command_buffers;
    VkCommandBuffer cur_command_buffer;
    int cur_command_buffer_state;

    VkSurfaceCapabilitiesKHR surface_caps;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkSwapchainKHR swapchain;

    int width;
    int height;
    VkExtent2D extent;

    int nb_in_flight_frames;
    int frame_index;

    VkImage *images;
    uint32_t nb_images;
    uint32_t image_index;

    struct darray wrapped_textures;
    struct darray ms_textures;
    struct darray resolve_textures;
    struct darray depth_textures;
    struct darray rts;

    VkSemaphore *sem_img_avail;
    VkSemaphore *sem_render_finished;
    VkFence *fences;

    struct darray wait_semaphores;
    struct darray wait_stages;
    struct darray signal_semaphores;

    struct rendertarget_desc default_rendertarget_desc;
    int viewport[4];
    int scissor[4];
    float clear_color[4];
};

int ngli_gctx_vk_begin_transient_command(struct gctx *s, VkCommandBuffer *command_buffer);
int ngli_gctx_vk_execute_transient_command(struct gctx *s, VkCommandBuffer command_buffer);

int on_swapchain_reset(struct gctx* s);
#endif
