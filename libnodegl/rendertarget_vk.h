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

#ifndef RENDERTARGET_VK_H
#define RENDERTARGET_VK_H

#include <vulkan/vulkan.h>
#include "rendertarget.h"

struct rendertarget_vk {
    struct rendertarget parent;
    int nb_attachments;
    VkImageView attachments[2*(NGLI_MAX_COLOR_ATTACHMENTS + 1)];
    VkFramebuffer framebuffer;
    VkRenderPass render_pass;
    VkRenderPass conservative_render_pass;
    struct texture *staging_texture;
};

struct rendertarget *ngli_rendertarget_vk_create(struct gctx *gctx);
int ngli_rendertarget_vk_init(struct rendertarget *s, const struct rendertarget_params *params);
void ngli_rendertarget_vk_blit(struct rendertarget *s, struct rendertarget *dst, int vflip);
void ngli_rendertarget_vk_resolve(struct rendertarget *s);
void ngli_rendertarget_vk_read_pixels(struct rendertarget *s, uint8_t *data);
void ngli_rendertarget_vk_freep(struct rendertarget **sp);

int ngli_vk_create_renderpass(struct gctx *s, const struct rendertarget_desc *desc, VkRenderPass *render_pass, int conservative);
VkSampleCountFlagBits ngli_vk_get_sample_count(int samples);


#endif
