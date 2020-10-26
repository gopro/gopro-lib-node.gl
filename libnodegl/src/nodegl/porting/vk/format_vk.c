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

#include "format_vk.h"

int ngli_format_get_vk_format(struct vkcontext *vk, int data_format, VkFormat *format)
{
    static const struct entry {
        VkFormat format;
    } format_map[] = {
        [NGLI_FORMAT_UNDEFINED]            = {VK_FORMAT_UNDEFINED},
        [NGLI_FORMAT_R8_UNORM]             = {VK_FORMAT_R8_UNORM},
        [NGLI_FORMAT_R8_SNORM]             = {VK_FORMAT_R8_SNORM},
        [NGLI_FORMAT_R8_UINT]              = {VK_FORMAT_R8_UINT},
        [NGLI_FORMAT_R8_SINT]              = {VK_FORMAT_R8_SINT},
        [NGLI_FORMAT_R8G8_UNORM]           = {VK_FORMAT_R8G8_UNORM},
        [NGLI_FORMAT_R8G8_SNORM]           = {VK_FORMAT_R8G8_SNORM},
        [NGLI_FORMAT_R8G8_UINT]            = {VK_FORMAT_R8G8_UINT},
        [NGLI_FORMAT_R8G8_SINT]            = {VK_FORMAT_R8G8_SINT},
        [NGLI_FORMAT_R8G8B8_UNORM]         = {VK_FORMAT_R8G8B8_UNORM},
        [NGLI_FORMAT_R8G8B8_SNORM]         = {VK_FORMAT_R8G8B8_SNORM},
        [NGLI_FORMAT_R8G8B8_UINT]          = {VK_FORMAT_R8G8B8_UINT},
        [NGLI_FORMAT_R8G8B8_SINT]          = {VK_FORMAT_R8G8B8_SINT},
        [NGLI_FORMAT_R8G8B8_SRGB]          = {VK_FORMAT_R8G8B8_SRGB},
        [NGLI_FORMAT_R8G8B8A8_UNORM]       = {VK_FORMAT_R8G8B8A8_UNORM},
        [NGLI_FORMAT_R8G8B8A8_SNORM]       = {VK_FORMAT_R8G8B8A8_SNORM},
        [NGLI_FORMAT_R8G8B8A8_UINT]        = {VK_FORMAT_R8G8B8A8_UINT},
        [NGLI_FORMAT_R8G8B8A8_SINT]        = {VK_FORMAT_R8G8B8A8_SINT},
        [NGLI_FORMAT_R8G8B8A8_SRGB]        = {VK_FORMAT_R8G8B8A8_SRGB},
        [NGLI_FORMAT_B8G8R8A8_UNORM]       = {VK_FORMAT_B8G8R8A8_UNORM},
        [NGLI_FORMAT_B8G8R8A8_SNORM]       = {VK_FORMAT_B8G8R8A8_SNORM},
        [NGLI_FORMAT_B8G8R8A8_UINT]        = {VK_FORMAT_B8G8R8A8_UINT},
        [NGLI_FORMAT_B8G8R8A8_SINT]        = {VK_FORMAT_B8G8R8A8_SINT},
        [NGLI_FORMAT_R16_UNORM]            = {VK_FORMAT_R16_UNORM},
        [NGLI_FORMAT_R16_SNORM]            = {VK_FORMAT_R16_SNORM},
        [NGLI_FORMAT_R16_UINT]             = {VK_FORMAT_R16_UINT},
        [NGLI_FORMAT_R16_SINT]             = {VK_FORMAT_R16_SINT},
        [NGLI_FORMAT_R16_SFLOAT]           = {VK_FORMAT_R16_SFLOAT},
        [NGLI_FORMAT_R16G16_UNORM]         = {VK_FORMAT_R16G16_UNORM},
        [NGLI_FORMAT_R16G16_SNORM]         = {VK_FORMAT_R16G16_SNORM},
        [NGLI_FORMAT_R16G16_UINT]          = {VK_FORMAT_R16G16_UINT},
        [NGLI_FORMAT_R16G16_SINT]          = {VK_FORMAT_R16G16_SINT},
        [NGLI_FORMAT_R16G16_SFLOAT]        = {VK_FORMAT_R16G16_SFLOAT},
        [NGLI_FORMAT_R16G16B16_UNORM]      = {VK_FORMAT_R16G16B16_UNORM},
        [NGLI_FORMAT_R16G16B16_SNORM]      = {VK_FORMAT_R16G16B16_SNORM},
        [NGLI_FORMAT_R16G16B16_UINT]       = {VK_FORMAT_R16G16B16_UINT},
        [NGLI_FORMAT_R16G16B16_SINT]       = {VK_FORMAT_R16G16B16_SINT},
        [NGLI_FORMAT_R16G16B16_SFLOAT]     = {VK_FORMAT_R16G16B16_SFLOAT},
        [NGLI_FORMAT_R16G16B16A16_UNORM]   = {VK_FORMAT_R16G16B16A16_UNORM},
        [NGLI_FORMAT_R16G16B16A16_SNORM]   = {VK_FORMAT_R16G16B16A16_SNORM},
        [NGLI_FORMAT_R16G16B16A16_UINT]    = {VK_FORMAT_R16G16B16A16_UINT},
        [NGLI_FORMAT_R16G16B16A16_SINT]    = {VK_FORMAT_R16G16B16A16_SINT},
        [NGLI_FORMAT_R16G16B16A16_SFLOAT]  = {VK_FORMAT_R16G16B16A16_SFLOAT},
        [NGLI_FORMAT_R32_UINT]             = {VK_FORMAT_R32_UINT},
        [NGLI_FORMAT_R32_SINT]             = {VK_FORMAT_R32_SINT},
        [NGLI_FORMAT_R32_SFLOAT]           = {VK_FORMAT_R32_SFLOAT},
        [NGLI_FORMAT_R32G32_UINT]          = {VK_FORMAT_R32G32_UINT},
        [NGLI_FORMAT_R32G32_SINT]          = {VK_FORMAT_R32G32_SINT},
        [NGLI_FORMAT_R32G32_SFLOAT]        = {VK_FORMAT_R32G32_SFLOAT},
        [NGLI_FORMAT_R32G32B32_UINT]       = {VK_FORMAT_R32G32B32_UINT},
        [NGLI_FORMAT_R32G32B32_SINT]       = {VK_FORMAT_R32G32B32_SINT},
        [NGLI_FORMAT_R32G32B32_SFLOAT]     = {VK_FORMAT_R32G32B32_SFLOAT},
        [NGLI_FORMAT_R32G32B32A32_UINT]    = {VK_FORMAT_R32G32B32A32_UINT},
        [NGLI_FORMAT_R32G32B32A32_SINT]    = {VK_FORMAT_R32G32B32A32_SINT},
        [NGLI_FORMAT_R32G32B32A32_SFLOAT]  = {VK_FORMAT_R32G32B32A32_SFLOAT},
        [NGLI_FORMAT_D16_UNORM]            = {VK_FORMAT_D16_UNORM},
        [NGLI_FORMAT_X8_D24_UNORM_PACK32]  = {VK_FORMAT_X8_D24_UNORM_PACK32},
        [NGLI_FORMAT_D32_SFLOAT]           = {VK_FORMAT_D32_SFLOAT},
        [NGLI_FORMAT_D24_UNORM_S8_UINT]    = {VK_FORMAT_D24_UNORM_S8_UINT},
        [NGLI_FORMAT_D32_SFLOAT_S8_UINT]   = {VK_FORMAT_D32_SFLOAT_S8_UINT},
        [NGLI_FORMAT_S8_UINT]              = {VK_FORMAT_S8_UINT},
    };

    ngli_assert(data_format >= 0 && data_format < NGLI_ARRAY_NB(format_map));
    const struct entry *entry = &format_map[data_format];

    ngli_assert(data_format == NGLI_FORMAT_UNDEFINED || entry->format);

    if (format)
        *format = entry->format;

    return 0;
}
