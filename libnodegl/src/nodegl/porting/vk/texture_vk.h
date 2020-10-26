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

#ifndef TEXTURE_VK_H
#define TEXTURE_VK_H

#include <vulkan/vulkan.h>

#include "nodegl/texture.h"
#include "nodegl/porting/vk/vkcontext.h"

struct texture_vk {
    struct texture parent;
    VkFormat format;
    int array_layers;
    int mipmap_levels;
    VkImage image;
    VkImageLayout image_layout;
    VkDeviceMemory image_memory;
    VkImageView image_view;
    VkSampler image_sampler;
    VkBuffer staging_buffer;
    VkDeviceSize staging_buffer_size;
    VkDeviceMemory staging_buffer_memory;
    int is_attachment;
};

struct texture *ngli_texture_vk_create(struct gctx *gctx);

int ngli_texture_vk_init(struct texture *s,
                         const struct texture_params *params);

int ngli_texture_vk_has_mipmap(const struct texture *s);
int ngli_texture_vk_match_dimensions(const struct texture *s, int width, int height, int depth);

int ngli_texture_vk_upload(struct texture *s, const uint8_t *data, int linesize);
int ngli_texture_vk_generate_mipmap(struct texture *s);

void ngli_texture_vk_freep(struct texture **sp);

int ngli_texture_vk_transition_layout(struct texture *s, VkImageLayout layout);

void ngli_vk_transition_image_layout(struct vkcontext *vk,
                                     VkCommandBuffer command_buffer,
                                     VkImage image,
                                     VkImageLayout old_layout,
                                     VkImageLayout new_layout,
                                     const VkImageSubresourceRange *subres_range);

VkFilter ngli_texture_get_vk_filter(int filter);

VkSamplerMipmapMode ngli_texture_get_vk_mipmap_mode(int mipmap_filter);

VkSamplerAddressMode ngli_texture_get_vk_wrap(int wrap);

int ngli_texture_vk_wrap(struct texture *s,
                         const struct texture_params *params,
                         VkImage image, VkImageLayout layout);

#endif
