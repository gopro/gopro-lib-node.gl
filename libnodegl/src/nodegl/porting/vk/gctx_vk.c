/*
 * Copyright 2018 GoPro Inc.
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

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>

#include "nodegl/core/math_utils.h"
#include "nodegl/core/nodes.h"
#include "nodegl/porting/vk/vkcontext.h"
#include "nodegl/porting/vk/gctx_vk.h"
#include "nodegl/core/log.h"
#include "nodegl/core/memory.h"
#include "nodegl/core/pgcache.h"

/* FIXME: missing includes probably */
#include "nodegl/porting/vk/buffer_vk.h"
#include "nodegl/porting/vk/texture_vk.h"
#include "nodegl/porting/vk/rendertarget_vk.h"
#include "nodegl/porting/vk/program_vk.h"
#include "nodegl/porting/vk/pipeline_vk.h"
#include "nodegl/porting/vk/gtimer_vk.h"
#include "nodegl/porting/vk/swapchain_vk.h"
#include "nodegl/porting/vk/util_vk.h"
#include <limits.h>
#ifdef ENABLE_RENDERDOC_CAPTURE
#include "renderdoc_utils.h"
static bool DEBUG_CAPTURE = false;
#endif

int ngli_gctx_vk_begin_transient_command(struct gctx *s, VkCommandBuffer *command_buffer)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = s_priv->transient_command_buffer_pool,
        .commandBufferCount = 1,
    };

    VkResult res = vkAllocateCommandBuffers(vk->device, &alloc_info, command_buffer);
    if (res != VK_SUCCESS)
        return -1;

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    res = vkBeginCommandBuffer(*command_buffer, &beginInfo);
    if (res != VK_SUCCESS) {
        vkFreeCommandBuffers(vk->device, s_priv->transient_command_buffer_pool, 1, command_buffer);
        return -1;
    }

    return 0;
}

int ngli_gctx_vk_execute_transient_command(struct gctx *s, VkCommandBuffer command_buffer)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    vkResetFences(vk->device, 1, &s_priv->transient_command_buffer_fence);

    VkResult res = vkQueueSubmit(vk->graphic_queue, 1, &submit_info, s_priv->transient_command_buffer_fence);
    if (res != VK_SUCCESS)
        goto done;

    res = vkWaitForFences(vk->device, 1, &s_priv->transient_command_buffer_fence, 1, UINT64_MAX);

done:
    vkFreeCommandBuffers(vk->device, s_priv->transient_command_buffer_pool, 1, &command_buffer);

    return res;
}

static int create_command_pool_and_buffers(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk->graphics_queue_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // FIXME
    };

    VkResult vkret = vkCreateCommandPool(vk->device, &command_pool_create_info, NULL, &s_priv->command_buffer_pool);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    s_priv->command_buffers = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(*s_priv->command_buffers));
    if (!s_priv->command_buffers)
        return NGL_ERROR_MEMORY;

    VkCommandBufferAllocateInfo command_buffers_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = s_priv->command_buffer_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = s_priv->nb_in_flight_frames,
    };

    VkResult ret = vkAllocateCommandBuffers(vk->device, &command_buffers_allocate_info, s_priv->command_buffers);
    if (ret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    return 0;
}

static void destroy_command_pool_and_buffers(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    if (s_priv->command_buffers) {
        vkFreeCommandBuffers(vk->device, s_priv->command_buffer_pool, s_priv->nb_in_flight_frames, s_priv->command_buffers);
        ngli_freep(&s_priv->command_buffers);
    }

    vkDestroyCommandPool(vk->device, s_priv->command_buffer_pool, NULL);
}

static VkResult create_semaphores(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    s_priv->sem_img_avail = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!s_priv->sem_img_avail)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    s_priv->sem_render_finished = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!s_priv->sem_render_finished)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    s_priv->fences = ngli_calloc(s_priv->nb_in_flight_frames, sizeof(VkFence));
    if (!s_priv->fences)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkResult ret;
    for (int i = 0; i < s_priv->nb_in_flight_frames; i++) {
        if ((ret = vkCreateSemaphore(vk->device, &semaphore_create_info, NULL,
                                     &s_priv->sem_img_avail[i])) != VK_SUCCESS ||
            (ret = vkCreateSemaphore(vk->device, &semaphore_create_info, NULL,
                                     &s_priv->sem_render_finished[i])) != VK_SUCCESS ||
            (ret = vkCreateFence(vk->device, &fence_create_info, NULL,
                                 &s_priv->fences[i])) != VK_SUCCESS) {
            return ret;
        }
    }
    return VK_SUCCESS;
}


static struct gctx *vk_create(const struct ngl_config *config)
{
    struct gctx_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    return (struct gctx *)s;
}

static VkResult create_offscreen_resources(struct gctx *s)
{
    const struct ngl_config *config = &s->config;
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++) {
        struct texture **ms_texture = ngli_darray_push(&s_priv->ms_textures, NULL);
        if (!ms_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *ms_texture = ngli_texture_create(s);
        if (!*ms_texture)
            return NGL_ERROR_MEMORY;

        struct texture_params texture_params = NGLI_TEXTURE_PARAM_DEFAULTS;
        texture_params.width = config->width;
        texture_params.height = config->height;
        texture_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
        texture_params.samples = config->samples;

        int ret = ngli_texture_init(*ms_texture, &texture_params);
        if (ret < 0)
            return ret;

        struct texture **depth_texture = ngli_darray_push(&s_priv->depth_textures, NULL);
        if (!depth_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *depth_texture = ngli_texture_create(s);
        if (!depth_texture)
            return NGL_ERROR_MEMORY;

        texture_params.width = config->width;
        texture_params.height = config->height;
        texture_params.format = vk->preferred_depth_stencil_format;
        texture_params.samples = config->samples;
        texture_params.usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
                NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        ret = ngli_texture_init(*depth_texture, &texture_params);
        if (ret < 0)
            return ret;

        struct rendertarget_params rt_params = {
            .width = config->width,
            .height = config->height,
            .nb_colors = 1,
            .colors[0].attachment = *ms_texture,
            .depth_stencil.attachment = *depth_texture,
            .readable = 1,
        };

        if (config->samples) {
            struct texture **resolve_texture = ngli_darray_push(&s_priv->resolve_textures, NULL);
            if (!resolve_texture)
                return VK_ERROR_OUT_OF_HOST_MEMORY;

            *resolve_texture = ngli_texture_create(s);
            if (!*resolve_texture)
                return NGL_ERROR_MEMORY;

            struct texture_params texture_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            texture_params.width = config->width;
            texture_params.height = config->height;
            texture_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
            texture_params.samples = 1;

            ret = ngli_texture_init(*resolve_texture, &texture_params);
            if (ret < 0)
                return ret;
            rt_params.colors[0].resolve_target = *resolve_texture;
        }

        struct rendertarget **rt = ngli_darray_push(&s_priv->rts, NULL);
        if (!rt)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *rt = ngli_rendertarget_create(s);
        if (!*rt)
            return NGL_ERROR_MEMORY;

        ret = ngli_rendertarget_init(*rt, &rt_params);
        if (ret < 0)
            return ret;
    }

    return VK_SUCCESS;
}

static int vk_create_swapchain_resources(struct gctx *s);

static int vk_init(struct gctx *s)
{
    const struct ngl_config *config = &s->config;
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
#ifdef ENABLE_RENDERDOC_CAPTURE
    DEBUG_CAPTURE = (getenv("DEBUG_CAPTURE") != NULL);
    if (DEBUG_CAPTURE) init_renderdoc();
#endif
    /* FIXME */
    s->features = -1;

    ngli_darray_init(&s_priv->wrapped_textures, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->ms_textures, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->resolve_textures, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->depth_textures, sizeof(struct texture *), 0);
    ngli_darray_init(&s_priv->rts, sizeof(struct rendertarget *), 0);

    ngli_darray_init(&s_priv->wait_semaphores, sizeof(VkSemaphore), 0);
    ngli_darray_init(&s_priv->wait_stages, sizeof(VkPipelineStageFlagBits), 0);
    ngli_darray_init(&s_priv->signal_semaphores, sizeof(VkSemaphore), 0);

    s_priv->vkcontext = ngli_vkcontext_create();
    if (!s_priv->vkcontext)
        return NGL_ERROR_MEMORY;

    int ret = ngli_vkcontext_init(s_priv->vkcontext, config);
    if (ret < 0) {
        ngli_vkcontext_freep(&s_priv->vkcontext);
        return ret;
    }
#ifdef ENABLE_RENDERDOC_CAPTURE
    if (DEBUG_CAPTURE) begin_renderdoc_capture();
#endif
    struct vkcontext *vk = s_priv->vkcontext;

    VkPhysicalDeviceLimits *limits = &vk->phy_device_props.limits;

    s->limits.max_color_attachments = limits->maxColorAttachments;
    s->limits.max_compute_work_group_counts[0] = limits->maxComputeWorkGroupCount[0];
    s->limits.max_compute_work_group_counts[1] = limits->maxComputeWorkGroupCount[1];
    s->limits.max_compute_work_group_counts[2] = limits->maxComputeWorkGroupCount[2];
    s->limits.max_draw_buffers = limits->maxColorAttachments;
    s->limits.max_samples = 4; // FIXME 
    s->limits.max_texture_image_units = 0; // FIXME
    s->limits.max_uniform_block_size = limits->maxUniformBufferRange;

    s_priv->spirv_compiler      = shaderc_compiler_initialize();
    s_priv->spirv_compiler_opts = shaderc_compile_options_initialize();
    if (!s_priv->spirv_compiler || !s_priv->spirv_compiler_opts)
        return -1;

    shaderc_env_version env_version = VK_API_VERSION_1_0;
    switch (vk->api_version) {
    case VK_API_VERSION_1_0: env_version = shaderc_env_version_vulkan_1_0; break;
    case VK_API_VERSION_1_1: env_version = shaderc_env_version_vulkan_1_1; break;
    case VK_API_VERSION_1_2: env_version = shaderc_env_version_vulkan_1_2; break;
    default:                 env_version = shaderc_env_version_vulkan_1_0;
    }

    shaderc_compile_options_set_target_env(s_priv->spirv_compiler_opts, shaderc_target_env_vulkan, env_version);
    shaderc_compile_options_set_invert_y(s_priv->spirv_compiler_opts, 1);
    shaderc_compile_options_set_optimization_level(s_priv->spirv_compiler_opts, shaderc_optimization_level_performance);

    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk->graphics_queue_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    /* FIXME: check return */
    vkCreateCommandPool(vk->device, &command_pool_create_info, NULL, &s_priv->transient_command_buffer_pool);

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    vkCreateFence(vk->device, &fence_create_info, NULL, &s_priv->transient_command_buffer_fence);

    s_priv->nb_in_flight_frames = 1;
    s_priv->width = config->width;
    s_priv->height = config->height;

    if (config->offscreen) {
        ret = create_offscreen_resources(s);
        if (ret != VK_SUCCESS)
            return -1;
    } else {
        ret = ngli_swapchain_create(s);
        if (ret != VK_SUCCESS)
            return -1;

        ret = vk_create_swapchain_resources(s);
        if (ret != VK_SUCCESS)
            return -1;
    }

    ret = create_semaphores(s);
    if (ret != VK_SUCCESS)
        return -1;

    create_command_pool_and_buffers(s);

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        ngli_gctx_set_viewport(s, viewport);
    } else {
        const int default_viewport[] = {0, 0, config->width, config->height};
        ngli_gctx_set_viewport(s, default_viewport);
    }

    const int scissor[] = {0, 0, config->width, config->height};
    ngli_gctx_set_scissor(s, scissor);

    ngli_gctx_set_clear_color(s, config->clear_color);


    if (config->offscreen) {
        s_priv->default_rendertarget_desc.nb_colors = 1;
        s_priv->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_R8G8B8A8_UNORM;
        s_priv->default_rendertarget_desc.colors[0].samples = config->samples;
        s_priv->default_rendertarget_desc.colors[0].resolve = config->samples > 0 ? 1 : 0;
        s_priv->default_rendertarget_desc.depth_stencil.format = vk->preferred_depth_stencil_format;
        s_priv->default_rendertarget_desc.depth_stencil.samples = config->samples;
        s_priv->default_rendertarget_desc.depth_stencil.resolve = 0;
    } else {
        s_priv->default_rendertarget_desc.nb_colors = 1;
        s_priv->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_B8G8R8A8_UNORM;
        s_priv->default_rendertarget_desc.colors[0].samples = config->samples;
        s_priv->default_rendertarget_desc.colors[0].resolve = config->samples > 0 ? 1 : 0;
        s_priv->default_rendertarget_desc.depth_stencil.format = vk->preferred_depth_stencil_format;
        s_priv->default_rendertarget_desc.depth_stencil.samples = config->samples;
    }

    struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
    s->default_rendertarget = *rts;

    return 0;
}

static int vk_resize(struct gctx *s, int width, int height, const int *viewport)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;

    s_priv->width = width;
    s_priv->height = height;

    if (viewport && viewport[2] > 0 && viewport[3] > 0) {
        ngli_gctx_set_viewport(s, viewport);
    } else {
        const int default_viewport[] = {0, 0, width, height};
        ngli_gctx_set_viewport(s, default_viewport);
    }

    const int scissor[] = {0, 0, width, height};
    ngli_gctx_set_scissor(s, scissor);

    return 0;
}

static int vk_pre_draw(struct gctx *s, double t)
{
    const struct ngl_config *config = &s->config;
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    vkWaitForFences(vk->device, 1, &s_priv->fences[s_priv->frame_index], VK_TRUE, UINT64_MAX);
    vkResetFences(vk->device, 1, &s_priv->fences[s_priv->frame_index]);

    if (!config->offscreen) {
        int ret = ngli_swapchain_acquire_image(s, &s_priv->image_index);
        if (ret < 0)
            return ret;

        VkPipelineStageFlagBits wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (!ngli_darray_push(&s_priv->wait_stages, &wait_stage))
            return NGL_ERROR_MEMORY;

        if (!ngli_darray_push(&s_priv->signal_semaphores, &s_priv->sem_render_finished[s_priv->frame_index]))
            return NGL_ERROR_MEMORY;

    }

    s_priv->cur_command_buffer = s_priv->command_buffers[s_priv->frame_index];
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    VkResult vkret = vkBeginCommandBuffer(s_priv->cur_command_buffer, &command_buffer_begin_info);
    if (vkret != VK_SUCCESS)
        return -1;
    s_priv->cur_command_buffer_state = 1;

    ngli_gctx_bind_rendertarget(s, NULL);

    return 0;
}

static int vk_swap_buffers(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = ngli_darray_count(&s_priv->signal_semaphores),
        .pWaitSemaphores = ngli_darray_data(&s_priv->signal_semaphores),
        .swapchainCount = 1,
        .pSwapchains = &s_priv->swapchain,
        .pImageIndices = &s_priv->image_index,
    };

    VkResult res = vkQueuePresentKHR(vk->present_queue, &present_info);
    ngli_darray_clear(&s_priv->signal_semaphores);
    switch (res) {
    case VK_SUCCESS:
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
        break;
    default:
        LOG(ERROR, "failed to present image %s", vk_res2str(res));
        return -1;
    }

    return 0;
}

static int vk_post_draw(struct gctx *s, double t)
{
    int ret = 0;
    const struct ngl_config *config = &s->config;
    struct gctx_vk *s_priv = (struct gctx_vk *)s;

    ngli_gctx_bind_rendertarget(s, NULL);

    if (config->offscreen) {
        if (config->capture_buffer) {
            struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
            ngli_rendertarget_read_pixels(rts[s_priv->frame_index], config->capture_buffer);
        }
        ngli_gctx_flush(s);
    } else {
        struct texture **wrapped_textures = ngli_darray_data(&s_priv->wrapped_textures);
        ret = ngli_texture_vk_transition_layout(wrapped_textures[s_priv->image_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        if (ret < 0)
            goto done;
        ngli_gctx_flush(s);
        ret = vk_swap_buffers(s);
    }

done:
    s_priv->frame_index = (s_priv->frame_index + 1) % s_priv->nb_in_flight_frames;

    /* Reset cur_command_buffer so updating resources will use a transient
     * command buffer */
    s_priv->cur_command_buffer = VK_NULL_HANDLE;

    return ret;
}

static void vk_destroy(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    /* FIXME */
    if (!vk)
        return;

    vkDeviceWaitIdle(vk->device);

    destroy_command_pool_and_buffers(s);

    if (s_priv->sem_render_finished) {
        for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++)
            vkDestroySemaphore(vk->device, s_priv->sem_render_finished[i], NULL);
        ngli_freep(&s_priv->sem_render_finished);
    }

    if (s_priv->sem_img_avail) {
        for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++)
            vkDestroySemaphore(vk->device, s_priv->sem_img_avail[i], NULL);
        ngli_freep(&s_priv->sem_img_avail);
    }

    if (s_priv->fences) {
        for (uint32_t i = 0; i < s_priv->nb_in_flight_frames; i++)
            vkDestroyFence(vk->device, s_priv->fences[i], NULL);
        ngli_freep(&s_priv->fences);
    }

    struct texture **wrapped_textures = ngli_darray_data(&s_priv->wrapped_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->wrapped_textures); i++)
        ngli_texture_freep(&wrapped_textures[i]);
    ngli_darray_reset(&s_priv->wrapped_textures);

    struct texture **ms_textures = ngli_darray_data(&s_priv->ms_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->ms_textures); i++)
        ngli_texture_freep(&ms_textures[i]);
    ngli_darray_reset(&s_priv->ms_textures);

    struct texture **resolve_textures = ngli_darray_data(&s_priv->resolve_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->resolve_textures); i++)
        ngli_texture_freep(&resolve_textures[i]);
    ngli_darray_reset(&s_priv->resolve_textures);

    struct texture **depth_textures = ngli_darray_data(&s_priv->depth_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->depth_textures); i++)
        ngli_texture_freep(&depth_textures[i]);
    ngli_darray_reset(&s_priv->depth_textures);

    struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
    for (int i = 0; i < ngli_darray_count(&s_priv->rts); i++)
        ngli_rendertarget_freep(&rts[i]);
    ngli_darray_reset(&s_priv->rts);

    if (s_priv->swapchain)
        vkDestroySwapchainKHR(vk->device, s_priv->swapchain, NULL);

    vkDestroyCommandPool(vk->device, s_priv->transient_command_buffer_pool, NULL);
    vkDestroyFence(vk->device, s_priv->transient_command_buffer_fence, NULL);

    ngli_freep(&s_priv->images);

    shaderc_compiler_release(s_priv->spirv_compiler);
    shaderc_compile_options_release(s_priv->spirv_compiler_opts);

    ngli_darray_reset(&s_priv->wait_semaphores);
    ngli_darray_reset(&s_priv->wait_stages);
    ngli_darray_reset(&s_priv->signal_semaphores);

    ngli_vkcontext_freep(&s_priv->vkcontext);
#ifdef ENABLE_RENDERDOC_CAPTURE
    if (DEBUG_CAPTURE) end_renderdoc_capture();
#endif
}

static void vk_wait_idle(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    vkDeviceWaitIdle(vk->device);
}

static int vk_transform_cull_mode(struct gctx *s, int cull_mode)
{
    static const int cull_mode_map[NGLI_CULL_MODE_NB] = {
        [NGLI_CULL_MODE_NONE]           = NGLI_CULL_MODE_NONE,
        [NGLI_CULL_MODE_FRONT_BIT]      = NGLI_CULL_MODE_BACK_BIT,
        [NGLI_CULL_MODE_BACK_BIT]       = NGLI_CULL_MODE_FRONT_BIT,
    };
    return cull_mode_map[cull_mode];
}

static void vk_transform_projection_matrix(struct gctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f,
    };
    ngli_mat4_mul(dst, matrix, dst);
}

static void vk_get_rendertarget_uvcoord_matrix(struct gctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    memcpy(dst, matrix, 4 * 4 * sizeof(float));
}

static void vk_bind_rendertarget(struct gctx *s, struct rendertarget *rt)
{
    if (s->cur_rendertarget != rt) {
        if (s->cur_rendertarget) ngli_rendertarget_end_pass(s->cur_rendertarget);
        if (rt) {
            ngli_rendertarget_begin_pass(rt);
            s->cur_rendertarget = rt;
            ngli_gctx_clear_color(s);
            ngli_gctx_clear_depth_stencil(s);
        }
    }
    s->cur_rendertarget = rt;
}

static struct rendertarget *vk_get_rendertarget(struct gctx *s)
{
    return s->cur_rendertarget;
}

static const struct rendertarget_desc *vk_get_default_rendertarget_desc(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    return &s_priv->default_rendertarget_desc;
}

static void vk_set_viewport(struct gctx *s, const int *viewport)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    memcpy(s_priv->viewport, viewport, sizeof(s_priv->viewport));
}

static void vk_get_viewport(struct gctx *s, int *viewport)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    memcpy(viewport, &s_priv->viewport, sizeof(s_priv->viewport));
}

static void vk_set_scissor(struct gctx *s, const int *scissor)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    memcpy(&s_priv->scissor, scissor, sizeof(s_priv->scissor));
}

static void vk_get_scissor(struct gctx *s, int *scissor)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    memcpy(scissor, &s_priv->scissor, sizeof(s_priv->scissor));
}

static void vk_set_clear_color(struct gctx *s, const float *color)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    memcpy(s_priv->clear_color, color, sizeof(s_priv->clear_color));
}

static void vk_get_clear_color(struct gctx *s, float *color)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    memcpy(color, &s_priv->clear_color, sizeof(s_priv->clear_color));
}

static void vk_clear_color(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct rendertarget *rt = s->cur_rendertarget;

    VkClearAttachment clear_attachments = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .colorAttachment = 0,
        .clearValue = {
            .color = {
                .float32 = {
                    s_priv->clear_color[0],
                    s_priv->clear_color[1],
                    s_priv->clear_color[2],
                    s_priv->clear_color[3],
                }
            },
        },
    };

    VkClearRect clear_rect = {
        .rect = {
            .offset = {0},
            .extent.width = rt->width,
            .extent.height = rt->height,
        },
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkCommandBuffer cmd_buf = s_priv->cur_command_buffer;
    vkCmdClearAttachments(cmd_buf, 1, &clear_attachments, 1, &clear_rect);
}

static void vk_clear_depth_stencil(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct rendertarget *rt = s->cur_rendertarget;

    if (rt) {
        struct rendertarget_params *params = &rt->params;
        if (!params->depth_stencil.attachment)
            return;
    }

    VkClearAttachment clear_attachments = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        .clearValue = {
            .depthStencil = {
                .depth = 1.0f,
                .stencil = 0,
            },
        },
    };

    VkClearRect clear_rect = {
        .rect = {
            .offset = {0},
            .extent.width = rt->width,
            .extent.height = rt->height,
        },
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkCommandBuffer cmd_buf = s_priv->cur_command_buffer;
    vkCmdClearAttachments(cmd_buf, 1, &clear_attachments, 1, &clear_rect);
}

static void vk_invalidate_depth_stencil(struct gctx *s)
{
    /* TODO: it needs change of API to implement this feature on Vulkan */
}

static void vk_flush(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    VkCommandBuffer cmd_buf = s_priv->cur_command_buffer;

    VkResult vkret = vkEndCommandBuffer(cmd_buf);
    if (vkret != VK_SUCCESS)
        return;
    s_priv->cur_command_buffer_state = 0;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = ngli_darray_count(&s_priv->wait_semaphores),
        .pWaitSemaphores = ngli_darray_data(&s_priv->wait_semaphores),
        .pWaitDstStageMask = ngli_darray_data(&s_priv->wait_stages),
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buf,
        .signalSemaphoreCount = ngli_darray_count(&s_priv->signal_semaphores),
        .pSignalSemaphores = ngli_darray_data(&s_priv->signal_semaphores),
    };

    VkResult ret = vkQueueSubmit(vk->graphic_queue, 1, &submit_info, s_priv->fences[s_priv->frame_index]);
    if (ret != VK_SUCCESS) {
        return;
    }

    ngli_darray_clear(&s_priv->wait_semaphores);
    ngli_darray_clear(&s_priv->wait_stages);
}

static int vk_get_preferred_depth_format(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    return vk->preferred_depth_format;
}

static int vk_get_preferred_depth_stencil_format(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    return vk->preferred_depth_stencil_format;
}

static int vk_create_swapchain_resources(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;
    struct ngl_config *config = &s->config;

    vkGetSwapchainImagesKHR(vk->device, s_priv->swapchain, &s_priv->nb_images, NULL);
    VkImage *imgs = ngli_realloc(s_priv->images, s_priv->nb_images * sizeof(*s_priv->images));
    if (!imgs)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    s_priv->images = imgs;
    vkGetSwapchainImagesKHR(vk->device, s_priv->swapchain, &s_priv->nb_images, s_priv->images);

    for (uint32_t i = 0; i < s_priv->nb_images; i++) {
        struct texture **wrapped_texture = ngli_darray_push(&s_priv->wrapped_textures, NULL);
        if (!wrapped_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *wrapped_texture = ngli_texture_create(s);
        if (!*wrapped_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        struct texture_params params = {
            .type = NGLI_TEXTURE_TYPE_2D,
            .format = NGLI_FORMAT_B8G8R8A8_UNORM,
            .width = s_priv->extent.width,
            .height = s_priv->extent.height,
            .external_storage = 1,
            .usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
                    NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
        };

        int ret = ngli_texture_vk_wrap(*wrapped_texture, &params, s_priv->images[i], VK_IMAGE_LAYOUT_UNDEFINED);
        if (ret < 0)
            return ret;

        struct texture **depth_texture = ngli_darray_push(&s_priv->depth_textures, NULL);
        if (!depth_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *depth_texture = ngli_texture_create(s);
        if (!*depth_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *depth_texture = ngli_texture_create(s);
        if (!*depth_texture)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        struct texture_params depth_params = {
            .type = NGLI_TEXTURE_TYPE_2D,
            .format = NGLI_FORMAT_D32_SFLOAT_S8_UINT,
            .width = s_priv->extent.width,
            .height = s_priv->extent.height,
            .samples = config->samples,
            .usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
                NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT |
                NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        };

        ret = ngli_texture_vk_init(*depth_texture, &depth_params);
        if (ret < 0)
            return ret;

        struct rendertarget_params rt_params = {
            .width = s_priv->extent.width,
            .height = s_priv->extent.height,
            .nb_colors = 1,
            .colors[0].attachment = *wrapped_texture,
            .depth_stencil.attachment = *depth_texture,
        };

        if (config->samples) {
            struct texture_params texture_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            texture_params.width = s_priv->extent.width;
            texture_params.height = s_priv->extent.height;
            texture_params.format = NGLI_FORMAT_B8G8R8A8_UNORM;
            texture_params.samples = config->samples;
            texture_params.usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT |
                    NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT |
                    NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

            struct texture **ms_texture = ngli_darray_push(&s_priv->ms_textures, NULL);
            if (!ms_texture)
                return VK_ERROR_OUT_OF_HOST_MEMORY;

            *ms_texture = ngli_texture_create(s);
            if (!*ms_texture)
                return NGL_ERROR_MEMORY;

            ret = ngli_texture_init(*ms_texture, &texture_params);
            if (ret < 0)
                return ret;
            rt_params.colors[0].attachment = *ms_texture;
            rt_params.colors[0].resolve_target = *wrapped_texture;
        }

        struct rendertarget **rt = ngli_darray_push(&s_priv->rts, NULL);
        if (!rt)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        *rt = ngli_rendertarget_create(s);
        if (!*rt)
            return NGL_ERROR_MEMORY;

        ret = ngli_rendertarget_init(*rt, &rt_params);
        if (ret < 0)
            return ret;
    }

    return VK_SUCCESS;
}

int on_swapchain_reset(struct gctx* s) {
    return vk_create_swapchain_resources(s);
}

const struct gctx_class ngli_gctx_vk = {
    .name         = "Vulkan",
    .create       = vk_create,
    .init         = vk_init,
    .resize       = vk_resize,
    .pre_draw     = vk_pre_draw,
    .post_draw    = vk_post_draw,
    .wait_idle    = vk_wait_idle,
    .destroy      = vk_destroy,

    .transform_cull_mode = vk_transform_cull_mode,
    .transform_projection_matrix = vk_transform_projection_matrix,
    .get_rendertarget_uvcoord_matrix = vk_get_rendertarget_uvcoord_matrix,

    .bind_rendertarget         = vk_bind_rendertarget,
    .get_rendertarget         = vk_get_rendertarget,
    .get_default_rendertarget_desc = vk_get_default_rendertarget_desc,
    .set_viewport             = vk_set_viewport,
    .get_viewport             = vk_get_viewport,
    .set_scissor              = vk_set_scissor,
    .get_scissor              = vk_get_scissor,
    .set_clear_color          = vk_set_clear_color,
    .get_clear_color          = vk_get_clear_color,
    .clear_color              = vk_clear_color,
    .clear_depth_stencil      = vk_clear_depth_stencil,
    .invalidate_depth_stencil = vk_invalidate_depth_stencil,
    .get_preferred_depth_format= vk_get_preferred_depth_format,
    .get_preferred_depth_stencil_format=vk_get_preferred_depth_stencil_format,
    .flush                    = vk_flush,

    .buffer_create = ngli_buffer_vk_create,
    .buffer_init   = ngli_buffer_vk_init,
    .buffer_upload = ngli_buffer_vk_upload,
    .buffer_download = ngli_buffer_vk_download,
    .buffer_map      = ngli_buffer_vk_map,
    .buffer_unmap    = ngli_buffer_vk_unmap,
    .buffer_freep  = ngli_buffer_vk_freep,

    .gtimer_create = ngli_gtimer_vk_create,
    .gtimer_init   = ngli_gtimer_vk_init,
    .gtimer_start  = ngli_gtimer_vk_start,
    .gtimer_stop   = ngli_gtimer_vk_stop,
    .gtimer_read   = ngli_gtimer_vk_read,
    .gtimer_freep  = ngli_gtimer_vk_freep,

    .pipeline_create         = ngli_pipeline_vk_create,
    .pipeline_init           = ngli_pipeline_vk_init,
    .pipeline_bind_resources = ngli_pipeline_vk_bind_resources,
    .pipeline_update_attribute = ngli_pipeline_vk_update_attribute,
    .pipeline_update_uniform = ngli_pipeline_vk_update_uniform,
    .pipeline_update_texture = ngli_pipeline_vk_update_texture,
    .pipeline_draw           = ngli_pipeline_vk_draw,
    .pipeline_draw_indexed   = ngli_pipeline_vk_draw_indexed,
    .pipeline_dispatch       = ngli_pipeline_vk_dispatch,
    .pipeline_freep          = ngli_pipeline_vk_freep,

    .program_create = ngli_program_vk_create,
    .program_init   = ngli_program_vk_init,
    .program_freep  = ngli_program_vk_freep,

    .rendertarget_create      = ngli_rendertarget_vk_create,
    .rendertarget_init        = ngli_rendertarget_vk_init,
    .rendertarget_resolve     = ngli_rendertarget_vk_resolve,
    .rendertarget_read_pixels = ngli_rendertarget_vk_read_pixels,
    .rendertarget_begin_pass  = ngli_rendertarget_vk_begin_pass,
    .rendertarget_end_pass    = ngli_rendertarget_vk_end_pass,
    .rendertarget_freep       = ngli_rendertarget_vk_freep,

    .swapchain_create         = ngli_swapchain_vk_create,
    .swapchain_destroy        = ngli_swapchain_vk_destroy,
    .swapchain_acquire_image  = ngli_swapchain_vk_acquire_image,

    .texture_create           = ngli_texture_vk_create,
    .texture_init             = ngli_texture_vk_init,
    .texture_has_mipmap       = ngli_texture_vk_has_mipmap,
    .texture_match_dimensions = ngli_texture_vk_match_dimensions,
    .texture_upload           = ngli_texture_vk_upload,
    .texture_generate_mipmap  = ngli_texture_vk_generate_mipmap,
    .texture_freep            = ngli_texture_vk_freep,
};
