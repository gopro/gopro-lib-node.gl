/*
 * Copyright 2017 GoPro Inc.
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

#include "nodegl/porting/vk/swapchain_vk.h"
#include "nodegl/porting/vk/gctx_vk.h"
#include "nodegl/core/log.h"
#include "nodegl/core/format.h"
#include "nodegl/core/nodes.h"
#include "nodegl/core/memory.h"
#include "nodegl/porting/vk/texture_vk.h"
#include "nodegl/porting/vk/util_vk.h"

static uint32_t clip_u32(uint32_t x, uint32_t min, uint32_t max)
{
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}

static struct VkSurfaceFormatKHR select_swapchain_surface_format(
        const VkSurfaceFormatKHR *formats, uint32_t nb_formats) {
    VkSurfaceFormatKHR target_fmt = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    if (nb_formats == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        return target_fmt;
    for (uint32_t i = 0; i < nb_formats; i++)
        if (formats[i].format == target_fmt.format &&
            formats[i].colorSpace == target_fmt.colorSpace)
            return formats[i];
    return formats[0];
}

static VkPresentModeKHR select_swapchain_present_mode(const VkPresentModeKHR *present_modes,
                                                      uint32_t nb_present_modes)
{
    VkPresentModeKHR target_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (uint32_t i = 0; i < nb_present_modes; i++) {

        /* triple buffering, best mode possible */
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_modes[i];

        /* some drivers may not actually have VK_PRESENT_MODE_FIFO_KHR */
        if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            target_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    return target_mode;
}

static VkExtent2D select_swapchain_current_extent(struct gctx *s, VkSurfaceCapabilitiesKHR caps);

VkResult ngli_swapchain_vk_create(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->phy_device, vk->surface, &s_priv->surface_caps);

    s_priv->surface_format = select_swapchain_surface_format(vk->surface_formats, vk->nb_surface_formats);
    s_priv->present_mode = select_swapchain_present_mode(vk->present_modes, vk->nb_present_modes);
    s_priv->extent = select_swapchain_current_extent(s, s_priv->surface_caps);
    s_priv->width = s_priv->extent.width;
    s_priv->height = s_priv->extent.height;
    LOG(INFO, "current extent: %dx%d", s_priv->extent.width, s_priv->extent.height);

    uint32_t img_count = s_priv->surface_caps.minImageCount + 1;
    if (s_priv->surface_caps.maxImageCount && img_count > s_priv->surface_caps.maxImageCount)
        img_count = s_priv->surface_caps.maxImageCount;
    LOG(INFO, "swapchain image count: %d [%d-%d]", img_count,
           s_priv->surface_caps.minImageCount, s_priv->surface_caps.maxImageCount);

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surface,
        .minImageCount = img_count,
        .imageFormat = s_priv->surface_format.format,
        .imageColorSpace = s_priv->surface_format.colorSpace,
        .imageExtent = s_priv->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = s_priv->surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = s_priv->present_mode,
        .clipped = VK_TRUE,
    };

    const uint32_t queue_family_indices[2] = {
        vk->graphics_queue_index,
        vk->present_queue_index,
    };
    if (queue_family_indices[0] != queue_family_indices[1]) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = NGLI_ARRAY_NB(queue_family_indices);
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }

    VkResult res = vkCreateSwapchainKHR(vk->device, &swapchain_create_info, NULL, &s_priv->swapchain);
    if (res != VK_SUCCESS)
        return res;

    return res;
}

static VkExtent2D select_swapchain_current_extent(struct gctx *s,
                                                  VkSurfaceCapabilitiesKHR caps)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    // XXX: we want to rely on value passed by the user (set to vk->width, vk->height in vk_resize()
    // so the viewport and scissor matches the extent
#if 0
    if (caps.currentExtent.width != UINT32_MAX) {
        LOG(DEBUG, "current extent: %dx%d", caps.currentExtent.width, caps.currentExtent.height);
        return caps.currentExtent;
    }
#endif

    VkExtent2D ext = {
        .width  = clip_u32(s_priv->width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
        .height = clip_u32(s_priv->height, caps.minImageExtent.height, caps.maxImageExtent.height),
    };
    LOG(DEBUG, "swapchain extent %dx%d", ext.width, ext.height);
    return ext;
}

void ngli_swapchain_vk_destroy(struct gctx *s)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    struct texture **wrapped_textures = ngli_darray_data(&s_priv->wrapped_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->wrapped_textures); i++)
        ngli_texture_freep(&wrapped_textures[i]);
    ngli_darray_clear(&s_priv->wrapped_textures);

    struct texture **ms_textures = ngli_darray_data(&s_priv->ms_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->ms_textures); i++)
        ngli_texture_freep(&ms_textures[i]);
    ngli_darray_clear(&s_priv->ms_textures);

    struct texture **depth_textures = ngli_darray_data(&s_priv->depth_textures);
    for (int i = 0; i < ngli_darray_count(&s_priv->depth_textures); i++)
        ngli_texture_freep(&depth_textures[i]);
    ngli_darray_clear(&s_priv->depth_textures);

    struct rendertarget **rts = ngli_darray_data(&s_priv->rts);
    for (int i = 0; i < ngli_darray_count(&s_priv->rts); i++)
        ngli_rendertarget_freep(&rts[i]);
    ngli_darray_clear(&s_priv->rts);

    vkDestroySwapchainKHR(vk->device, s_priv->swapchain, NULL);
}

extern int on_swapchain_reset(struct gctx* s);

// XXX: window minimizing? (fb gets zero width or height)
static int reset_swapchain(struct gctx *gctx, struct vkcontext *vk)
{
    VkResult ret;

    vkDeviceWaitIdle(vk->device);
    ngli_swapchain_destroy(gctx);
    if ((ret = ngli_swapchain_create(gctx)) != VK_SUCCESS ||
        (ret = on_swapchain_reset(gctx)) != VK_SUCCESS)
        return -1;

    return 0;
}

int ngli_swapchain_vk_acquire_image(struct gctx *s, uint32_t *image_index)
{
    struct gctx_vk *s_priv = (struct gctx_vk *)s;
    struct vkcontext *vk = s_priv->vkcontext;

    VkSemaphore semaphore = s_priv->sem_img_avail[s_priv->frame_index];
    VkResult res = vkAcquireNextImageKHR(vk->device, s_priv->swapchain, UINT64_MAX, semaphore, NULL, image_index);
    switch (res) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        res = reset_swapchain(s, vk);
        if (res != VK_SUCCESS)
            return -1;
        res = vkAcquireNextImageKHR(vk->device, s_priv->swapchain, UINT64_MAX, semaphore, NULL, image_index);
        if (res != VK_SUCCESS)
            return -1;
        break;
    default:
        LOG(ERROR, "failed to acquire swapchain image: %s", vk_res2str(res));
        return -1;
    }

    if (!ngli_darray_push(&s_priv->wait_semaphores, &semaphore))
        return NGL_ERROR_MEMORY;

    return 0;
}
