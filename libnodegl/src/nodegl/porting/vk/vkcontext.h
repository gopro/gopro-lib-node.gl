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

#ifndef VKCONTEXT_H
#define VKCONTEXT_H

#include <stdlib.h>

#include <shaderc/shaderc.h>
#include <vulkan/vulkan.h>

#if defined(TARGET_LINUX)
#include <X11/Xlib.h>
#endif

#if defined(HAVE_WAYLAND)
#include <wayland-client.h>
#endif

#include "nodegl/darray.h"
#include "nodegl/nodegl.h"
#include "nodegl/rendertarget.h"
#include "nodegl/texture.h"

#define VK_FUN(name) PFN_vk##name

#define VK_LOAD_FUN(instance, name) \
    VK_FUN(name) name = (VK_FUN(name))vkGetInstanceProcAddr(instance, "vk" #name);

struct vkcontext {
    uint32_t api_version;
    VkInstance instance;
    VkLayerProperties *layers;
    uint32_t nb_layers;
    VkExtensionProperties *extensions;
    uint32_t nb_extensions;
    VkDebugUtilsMessengerEXT debug_callback;
    VkSurfaceKHR surface;

    VkExtensionProperties *device_extensions;
    uint32_t nb_device_extensions;

    int own_display;
#if defined(TARGET_LINUX)
    Display *display;
#endif

    VkPhysicalDevice *phy_devices;
    uint32_t nb_phy_devices;
    VkPhysicalDevice phy_device;
    VkPhysicalDeviceProperties phy_device_props;
    uint32_t graphics_queue_index;
    uint32_t present_queue_index;
    VkQueue graphic_queue;
    VkQueue present_queue;
    VkDevice device;

    int preferred_depth_format;
    int preferred_depth_stencil_format;

    VkPhysicalDeviceFeatures dev_features;
    VkPhysicalDeviceMemoryProperties phydev_mem_props;
    VkPhysicalDeviceLimits phydev_limits;

    VkSurfaceCapabilitiesKHR surface_caps;
    VkSurfaceFormatKHR *surface_formats;
    uint32_t nb_surface_formats;
    VkPresentModeKHR *present_modes;
    uint32_t nb_present_modes;
};

struct vkcontext *ngli_vkcontext_create(void);
int ngli_vkcontext_init(struct vkcontext *s, const struct ngl_config *config);
void *ngli_vkcontext_get_proc_addr(struct vkcontext *s, const char *name);
VkFormat ngli_vkcontext_find_supported_format(struct vkcontext *s, const VkFormat *formats,
                                              VkImageTiling tiling, VkFormatFeatureFlags features);
int ngli_vkcontext_find_memory_type(struct vkcontext *s, uint32_t type, VkMemoryPropertyFlags props);
void ngli_vkcontext_freep(struct vkcontext **sp);

#endif /* VKCONTEXT_H */
