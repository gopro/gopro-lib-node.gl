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

#include "nodegl/porting/vk/buffer_vk.h"
#include "nodegl/core/log.h"
#include "nodegl/core/nodes.h"
#include "nodegl/core/memory.h"
#include "nodegl/porting/vk/gctx_vk.h"
#include "nodegl/porting/vk/vkcontext.h"

#define USAGE (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  | \
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT   | \
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | \
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)  \

struct buffer *ngli_buffer_vk_create(struct gctx *gctx)
{
    struct buffer_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct buffer *)s;
}

int ngli_buffer_vk_init(struct buffer *s, int size, int usage)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    s->size = size;
    s->usage = usage;

    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = USAGE,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkResult res = vkCreateBuffer(vk->device, &buffer_create_info, NULL, &s_priv->buffer);
    if (res != VK_SUCCESS)
        return NGL_ERROR_GENERIC;

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(vk->device, s_priv->buffer, &requirements);

    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int32_t memory_type_index = ngli_vkcontext_find_memory_type(vk, requirements.memoryTypeBits, props);
    if (memory_type_index < 0)
        return NGL_ERROR_EXTERNAL;

    VkMemoryAllocateInfo memory_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_type_index,
    };
    res = vkAllocateMemory(vk->device, &memory_allocate_info, NULL, &s_priv->memory);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "can not allocate memory");
        return NGL_ERROR_MEMORY;
    }

    res = vkBindBufferMemory(vk->device, s_priv->buffer, s_priv->memory, 0);
    if (res != VK_SUCCESS) {
        LOG(ERROR, "can not bind memory");
        return NGL_ERROR_MEMORY;
    }

    return 0;
}

int ngli_buffer_vk_upload(struct buffer *s, const void *data, uint32_t size, uint32_t offset)
{
    void *dst;
    VkResult res = ngli_buffer_map(s, size, 0, &dst);
    if (res != VK_SUCCESS)
        return NGL_ERROR_GENERIC;
    memcpy(dst, data, size);
    ngli_buffer_unmap(s);

    return 0;
}

int ngli_buffer_vk_download(struct buffer* s, void* data, uint32_t size, uint32_t offset) {
    void* src;
    ngli_buffer_map(s, size, offset, &src);
    memcpy(data, (uint8_t*)src + offset, size);
    ngli_buffer_unmap(s);
    return 0;
}

int ngli_buffer_vk_map(struct buffer *s, int size, uint32_t offset, void** data)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    if (!data) {
        LOG(ERROR, "data is NULL");
        return NGL_ERROR_GENERIC;
    }
    VkResult res = vkMapMemory(vk->device, s_priv->memory, offset, size, 0, data);
    if (res != VK_SUCCESS)
        return NGL_ERROR_GENERIC;

    return 0;
}

void ngli_buffer_vk_unmap(struct buffer* s) {
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    vkUnmapMemory(vk->device, s_priv->memory);
}

void ngli_buffer_vk_freep(struct buffer **sp)
{
    if (!*sp)
        return;

    struct buffer *s = *sp;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct buffer_vk *s_priv = (struct buffer_vk *)s;

    vkDestroyBuffer(vk->device, s_priv->buffer, NULL);
    vkFreeMemory(vk->device, s_priv->memory, NULL);
    ngli_freep(sp);
}
