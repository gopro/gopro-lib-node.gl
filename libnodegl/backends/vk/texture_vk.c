/*
 * Copyright 2019 GoPro Inc.
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
#include <stdint.h>

#include "log.h"
#include "utils.h"
#include "format.h"
#include "format_vk.h"
#include "memory.h"
#include "texture_vk.h"
#include "nodes.h"
#include "gpu_ctx_vk.h"
#include "vkutils.h"

static const VkFilter vk_filter_map[NGLI_NB_FILTER] = {
    [NGLI_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [NGLI_FILTER_LINEAR]  = VK_FILTER_LINEAR,
};

static VkFilter get_vk_filter(int filter)
{
    return vk_filter_map[filter];
}

static const VkSamplerMipmapMode vk_mipmap_mode_map[NGLI_NB_MIPMAP] = {
    [NGLI_MIPMAP_FILTER_NONE]    = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [NGLI_MIPMAP_FILTER_NEAREST] = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [NGLI_MIPMAP_FILTER_LINEAR]  = VK_SAMPLER_MIPMAP_MODE_LINEAR,
};

static VkSamplerMipmapMode get_vk_mipmap_mode(int mipmap_filter)
{
    return vk_mipmap_mode_map[mipmap_filter];
}

static const VkSamplerAddressMode vk_wrap_map[NGLI_NB_WRAP] = {
    [NGLI_WRAP_CLAMP_TO_EDGE]   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    [NGLI_WRAP_MIRRORED_REPEAT] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    [NGLI_WRAP_REPEAT]          = VK_SAMPLER_ADDRESS_MODE_REPEAT,
};

static VkSamplerAddressMode get_vk_wrap(int wrap)
{
    return vk_wrap_map[wrap];
}

static VkImageAspectFlags get_vk_image_aspect_flags(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

static const VkImageType image_type_map[NGLI_TEXTURE_TYPE_NB] = {
    [NGLI_TEXTURE_TYPE_2D]   = VK_IMAGE_TYPE_2D,
    [NGLI_TEXTURE_TYPE_3D]   = VK_IMAGE_TYPE_3D,
    [NGLI_TEXTURE_TYPE_CUBE] = VK_IMAGE_TYPE_2D,
};

static VkImageType get_vk_image_type(int type)
{
    return image_type_map[type];
}

static const VkImageViewType image_view_type_map[NGLI_TEXTURE_TYPE_NB] = {
    [NGLI_TEXTURE_TYPE_2D]   = VK_IMAGE_VIEW_TYPE_2D,
    [NGLI_TEXTURE_TYPE_3D]   = VK_IMAGE_VIEW_TYPE_3D,
    [NGLI_TEXTURE_TYPE_CUBE] = VK_IMAGE_VIEW_TYPE_CUBE,
};

static VkImageViewType get_vk_image_view_type(int type)
{
    return image_view_type_map[type];
}

static VkAccessFlagBits get_vk_access_mask_from_image_layout(VkImageLayout layout, int dst_mask)
{
    VkPipelineStageFlags access_mask = 0;
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        ngli_assert(!dst_mask);
        break;
    case VK_IMAGE_LAYOUT_GENERAL:
        access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        ngli_assert(dst_mask);
        access_mask = VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        /* TODO */
        break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
        /* TODO */
        break;
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        /* TODO */
        break;
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        access_mask = VK_ACCESS_MEMORY_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
        /* TODO */
        break;
    default:
        LOG(ERROR, "unexpected image layout: 0x%x", layout);
        ngli_assert(0);
    }

    return access_mask;
}

static void vk_transition_image_layout(VkCommandBuffer command_buffer,
                                       VkImage image,
                                       VkImageLayout old_layout,
                                       VkImageLayout new_layout,
                                       const VkImageSubresourceRange *subres_range)
{
    const VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    const VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = get_vk_access_mask_from_image_layout(old_layout, 0),
        .dstAccessMask       = get_vk_access_mask_from_image_layout(new_layout, 1),
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = *subres_range,
    };

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

static VkImageUsageFlags get_vk_image_usage(int usage)
{
    return (usage & NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT             ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT             ? VK_IMAGE_USAGE_TRANSFER_DST_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_SAMPLED_BIT                  ? VK_IMAGE_USAGE_SAMPLED_BIT                  : 0)
         | (usage & NGLI_TEXTURE_USAGE_STORAGE_BIT                  ? VK_IMAGE_USAGE_STORAGE_BIT                  : 0)
         | (usage & NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         : 0)
         | (usage & NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);
}

static VkFormatFeatureFlags get_vk_format_features(int usage)
{
    return (usage & NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT             ? VK_FORMAT_FEATURE_TRANSFER_SRC_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT             ? VK_FORMAT_FEATURE_TRANSFER_DST_BIT             : 0)
         | (usage & NGLI_TEXTURE_USAGE_SAMPLED_BIT                  ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT            : 0)
         | (usage & NGLI_TEXTURE_USAGE_STORAGE_BIT                  ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT            : 0)
         | (usage & NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT         : 0)
         | (usage & NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);
}

struct texture *ngli_texture_vk_create(struct gpu_ctx *gpu_ctx)
{
    struct texture_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gpu_ctx = gpu_ctx;
    return (struct texture *)s;
}

VkResult ngli_texture_vk_init(struct texture *s, const struct texture_params *params)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    s->params = *params;

    s_priv->format = ngli_format_get_vk_format(vk, s->params.format);

    const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    const VkImageUsageFlags usage = get_vk_image_usage(params->usage);
    const VkFormatFeatureFlags features = get_vk_format_features(params->usage);

    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(vk->phy_device, s_priv->format, &properties);

    VkFormatFeatureFlags supported_features;
    switch (tiling) {
    case VK_IMAGE_TILING_LINEAR:  supported_features = properties.linearTilingFeatures;  break;
    case VK_IMAGE_TILING_OPTIMAL: supported_features = properties.optimalTilingFeatures; break;
    default:
        ngli_assert(0);
    }

    if ((properties.optimalTilingFeatures & features) != features) {
        LOG(ERROR, "unsupported format %d, supported features: 0x%x, requested features: 0x%x",
            s_priv->format, supported_features, features);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    uint32_t depth = 1;
    if (params->type == NGLI_TEXTURE_TYPE_3D) {
        ngli_assert(params->depth);
        depth = params->depth;
    }
    s->params.depth = depth;

    s_priv->array_layers = 1;
    VkImageCreateFlags flags = 0;
    if (params->type == NGLI_TEXTURE_TYPE_CUBE) {
        s_priv->array_layers = 6;
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    s_priv->mipmap_levels = 1;
    if (params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE) {
        while ((params->width | params->height) >> s_priv->mipmap_levels)
            s_priv->mipmap_levels += 1;
    }

    const VkImageCreateInfo image_create_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = get_vk_image_type(params->type),
        .extent.width  = params->width,
        .extent.height = params->height,
        .extent.depth  = depth,
        .mipLevels     = s_priv->mipmap_levels,
        .arrayLayers   = s_priv->array_layers,
        .format        = s_priv->format,
        .tiling        = tiling,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage         = usage,
        .samples       = ngli_vk_get_sample_count(params->samples),
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .flags         = flags,
    };

    VkResult res = vkCreateImage(vk->device, &image_create_info, NULL, &s_priv->image);
    if (res != VK_SUCCESS)
        return res;

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(vk->device, s_priv->image, &requirements);

    const VkMemoryPropertyFlags memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    int32_t memory_index = ngli_vkcontext_find_memory_type(vk, requirements.memoryTypeBits, memory_property_flags);
    if (memory_index < 0)
        return VK_ERROR_UNKNOWN;

    const VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = requirements.size,
        .memoryTypeIndex = memory_index,
    };

    res = vkAllocateMemory(vk->device, &alloc_info, NULL, &s_priv->image_memory);
    if (res != VK_SUCCESS)
        return res;

    res = vkBindImageMemory(vk->device, s_priv->image, s_priv->image_memory, 0);
    if (res != VK_SUCCESS)
        return res;

    s_priv->image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    res = ngli_gpu_ctx_vk_begin_transient_command(s->gpu_ctx, &command_buffer);
    if (res != VK_SUCCESS)
        return res;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };

    vk_transition_image_layout(command_buffer, s_priv->image, s_priv->image_layout, VK_IMAGE_LAYOUT_GENERAL, &subres_range);

    res = ngli_gpu_ctx_vk_execute_transient_command(s->gpu_ctx, command_buffer);
    if (res != VK_SUCCESS)
        return res;

    s_priv->image_layout = VK_IMAGE_LAYOUT_GENERAL;

    const VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = s_priv->image,
        .viewType = get_vk_image_view_type(params->type),
        .format   = s_priv->format,
        .subresourceRange = {
            .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
            .baseMipLevel   = 0,
            .levelCount     = s_priv->mipmap_levels,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        }
    };

    res = vkCreateImageView(vk->device, &view_info, NULL, &s_priv->image_view);
    if (res != VK_SUCCESS)
        return res;

    const VkSamplerCreateInfo sampler_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = get_vk_filter(s->params.mag_filter),
        .minFilter               = get_vk_filter(s->params.min_filter),
        .addressModeU            = get_vk_wrap(s->params.wrap_s),
        .addressModeV            = get_vk_wrap(s->params.wrap_t),
        .addressModeW            = get_vk_wrap(s->params.wrap_r),
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .mipmapMode              = get_vk_mipmap_mode(s->params.mipmap_filter),
        .minLod                  = 0.0f,
        .maxLod                  = s_priv->mipmap_levels,
        .mipLodBias              = 0.0f,
    };

    return vkCreateSampler(vk->device, &sampler_info, NULL, &s_priv->image_sampler);
}

VkResult ngli_texture_vk_wrap(struct texture *s, const struct texture_params *params, VkImage image, VkImageLayout layout)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    ngli_assert(params->external_storage);

    s->params = *params;

    s_priv->format = ngli_format_get_vk_format(vk, s->params.format);

    s_priv->array_layers = 1;
    if (params->type == NGLI_TEXTURE_TYPE_CUBE)
        s_priv->array_layers = 6;

    /* XXX */
    s_priv->mipmap_levels = 1;

    s_priv->image = image;
    s_priv->image_layout = layout;

    const VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = s_priv->image,
        .viewType = get_vk_image_view_type(params->type),
        .format   = s_priv->format,
        .subresourceRange = {
            .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
            .baseMipLevel   = 0,
            .levelCount     = s_priv->mipmap_levels,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        }
    };

    VkResult res = vkCreateImageView(vk->device, &view_info, NULL, &s_priv->image_view);
    if (res != VK_SUCCESS)
        return res;

    const VkSamplerCreateInfo sampler_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = get_vk_filter(s->params.mag_filter),
        .minFilter               = get_vk_filter(s->params.min_filter),
        .addressModeU            = get_vk_wrap(s->params.wrap_s),
        .addressModeV            = get_vk_wrap(s->params.wrap_t),
        .addressModeW            = get_vk_wrap(s->params.wrap_r),
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .mipmapMode              = get_vk_mipmap_mode(s->params.mipmap_filter),
        .minLod                  = 0.0f,
        .maxLod                  = s_priv->mipmap_levels,
        .mipLodBias              = 0.0f,
    };

    return vkCreateSampler(vk->device, &sampler_info, NULL, &s_priv->image_sampler);
}

int ngli_texture_vk_transition_layout(struct texture *s, VkImageLayout layout)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    if (s_priv->image_layout == layout)
        return 0;

    VkCommandBuffer command_buffer = gpu_ctx_vk->cur_command_buffer;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    vk_transition_image_layout(command_buffer, s_priv->image, s_priv->image_layout, layout, &subres_range);

    s_priv->image_layout = layout;

    return 0;
}

VkResult ngli_texture_vk_upload(struct texture *s, const uint8_t *data, int linesize)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;
    const struct texture_params *params = &s->params;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    /* texture with external storage (including wrapped textures and render
     * buffers) cannot update their content with this function */
    ngli_assert(!s->external_storage);
    ngli_assert(params->usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT);

    if (!data)
        return VK_SUCCESS;

    if (!s_priv->staging_buffer || s_priv->staging_buffer_row_length != linesize) {
        const int32_t bytes_per_pixel = ngli_format_get_bytes_per_pixel(params->format);
        const int32_t width = linesize ? linesize : s->params.width;
        s_priv->staging_buffer_size = width * s->params.height * s->params.depth * bytes_per_pixel * s_priv->array_layers;

        vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
        s_priv->staging_buffer = VK_NULL_HANDLE;

        vkFreeMemory(vk->device, s_priv->staging_buffer_memory, NULL);
        s_priv->staging_buffer_memory = VK_NULL_HANDLE;

        const VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = s_priv->staging_buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res = vkCreateBuffer(vk->device, &buffer_create_info, NULL, &s_priv->staging_buffer);
        if (res != VK_SUCCESS)
            return res;

        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(vk->device, s_priv->staging_buffer, &requirements);

        const VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const int32_t memory_index = ngli_vkcontext_find_memory_type(vk, requirements.memoryTypeBits, props);
        if (memory_index < 0)
            return VK_ERROR_UNKNOWN;

        const VkMemoryAllocateInfo memory_allocate_info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = requirements.size,
            .memoryTypeIndex = memory_index,
        };

        res = vkAllocateMemory(vk->device, &memory_allocate_info, NULL, &s_priv->staging_buffer_memory);
        if (res != VK_SUCCESS)
            return res;

        res = vkBindBufferMemory(vk->device, s_priv->staging_buffer, s_priv->staging_buffer_memory, 0);
        if (res != VK_SUCCESS)
            return res;

        s_priv->staging_buffer_row_length = linesize;
    }

    void *mapped_data;
    VkResult res = vkMapMemory(vk->device, s_priv->staging_buffer_memory, 0, s_priv->staging_buffer_size, 0, &mapped_data);
    if (res != VK_SUCCESS)
        return res;

    memcpy(mapped_data, data, s_priv->staging_buffer_size);
    vkUnmapMemory(vk->device, s_priv->staging_buffer_memory);

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    res = ngli_gpu_ctx_vk_begin_transient_command(s->gpu_ctx, &command_buffer);
    if (res != VK_SUCCESS)
        return res;

    const VkImageSubresourceRange subres_range = {
        .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    vk_transition_image_layout(
                                    command_buffer,
                                    s_priv->image,
                                    s_priv->image_layout,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &subres_range);

    struct darray copy_regions;
    ngli_darray_init(&copy_regions, sizeof(VkBufferImageCopy), 0);

    int32_t layer_size = s->params.width * s->params.height * ngli_format_get_bytes_per_pixel(s->params.format);
    for (int32_t i = 0; i < s_priv->array_layers; i++) {
        const VkDeviceSize offset = i * layer_size;
        const VkBufferImageCopy region = {
            .bufferOffset      = offset,
            .bufferRowLength   = linesize,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask     = get_vk_image_aspect_flags(s_priv->format),
                .mipLevel       = 0,
                .baseArrayLayer = i,
                .layerCount     = 1,
            },
            .imageOffset = {
                0,
                0,
                0,
            },
            .imageExtent = {
                params->width,
                params->height,
                params->depth ? params->depth : 1,
            }
        };

        if (!ngli_darray_push(&copy_regions, &region)) {
            ngli_darray_reset(&copy_regions);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    vkCmdCopyBufferToImage(command_buffer,
                           s_priv->staging_buffer,
                           s_priv->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           ngli_darray_count(&copy_regions),
                           ngli_darray_data(&copy_regions));

    vk_transition_image_layout(command_buffer,
                               s_priv->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               s_priv->image_layout,
                               &subres_range);

    res = ngli_gpu_ctx_vk_execute_transient_command(s->gpu_ctx, command_buffer);
    if (res != VK_SUCCESS)
        return res;

    if (params->mipmap_filter != NGLI_MIPMAP_FILTER_NONE)
        ngli_texture_generate_mipmap(s);

    return VK_SUCCESS;
}

VkResult ngli_texture_vk_generate_mipmap(struct texture *s)
{
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    const struct texture_params *params = &s->params;
    struct texture_vk *s_priv = (struct texture_vk *)s;

    ngli_assert(params->usage & NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT);
    ngli_assert(params->usage & NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT);

    VkCommandBuffer command_buffer = gpu_ctx_vk->cur_command_buffer ? gpu_ctx_vk->cur_command_buffer : VK_NULL_HANDLE;
    if (!command_buffer) {
        VkResult res = ngli_gpu_ctx_vk_begin_transient_command(s->gpu_ctx, &command_buffer);
        if (res != VK_SUCCESS)
            return res;
    }

    const VkImageSubresourceRange subres_range = {
        .aspectMask = get_vk_image_aspect_flags(s_priv->format),
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    vk_transition_image_layout(
                                    command_buffer,
                                    s_priv->image,
                                    s_priv->image_layout,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &subres_range);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = s_priv->image,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseArrayLayer = 0,
            .layerCount     = params->type == NGLI_TEXTURE_TYPE_CUBE ? 6 : 1,
            .levelCount     = 1,
        }
    };

    int32_t mipmap_width  = s->params.width;
    int32_t mipmap_height = s->params.height;
    for (uint32_t i = 1; i < s_priv->mipmap_levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, NULL,
                             0, NULL,
                             1, &barrier);

        VkImageBlit blit = {
            .srcOffsets[0] = {
                .x = 0,
                .y = 0,
                .z = 0,
            },
            .srcOffsets[1] = {
                .x = mipmap_width,
                .y = mipmap_height,
                .z = 1,
            },
            .srcSubresource = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = i - 1,
                .baseArrayLayer = 0,
                .layerCount     = s_priv->array_layers,
            },
            .dstOffsets[0] = {
                .x = 0,
                .y = 0,
                .z = 0,
            },
            .dstOffsets[1] = {
                .x = NGLI_MAX(mipmap_width >> 1, 1),
                .y = NGLI_MAX(mipmap_height >> 1, 1),
                .z = 1,
            },
            .dstSubresource = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = i,
                .baseArrayLayer = 0,
                .layerCount     = params->type == NGLI_TEXTURE_TYPE_CUBE ? 6 : 1,
            }
        };

        vkCmdBlitImage(command_buffer,
                       s_priv->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       s_priv->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = s_priv->image_layout;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, NULL,
                             0, NULL,
                             1, &barrier);

        mipmap_width  = NGLI_MAX(mipmap_width >> 1, 1);
        mipmap_height = NGLI_MAX(mipmap_height >> 1, 1);
    }

    barrier.subresourceRange.baseMipLevel = s_priv->mipmap_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = s_priv->image_layout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &barrier);

    if (!gpu_ctx_vk->cur_command_buffer)
        return ngli_gpu_ctx_vk_execute_transient_command(s->gpu_ctx, command_buffer);

    return VK_SUCCESS;
}

void ngli_texture_vk_freep(struct texture **sp)
{
    if (!*sp)
        return;

    struct texture *s = *sp;
    struct texture_vk *s_priv = (struct texture_vk *)s;
    struct gpu_ctx_vk *gpu_ctx_vk = (struct gpu_ctx_vk *)s->gpu_ctx;
    struct vkcontext *vk = gpu_ctx_vk->vkcontext;

    vkDestroySampler(vk->device, s_priv->image_sampler, NULL);
    vkDestroyImageView(vk->device, s_priv->image_view, NULL);
    if (!s->params.external_storage)
        vkDestroyImage(vk->device, s_priv->image, NULL);
    vkDestroyBuffer(vk->device, s_priv->staging_buffer, NULL);
    vkFreeMemory(vk->device, s_priv->staging_buffer_memory, NULL);
    vkFreeMemory(vk->device, s_priv->image_memory, NULL);
    ngli_freep(sp);
}
