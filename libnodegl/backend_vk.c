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

#define VK_USE_PLATFORM_XLIB_KHR // XXX
#include <vulkan/vulkan.h>

#include "backend.h"
#include "glcontext.h"
#include "log.h"

#define ENABLE_DEBUG 1

static const VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pEngineName = "node.gl",
    .engineVersion = NODEGL_VERSION_INT,
    .apiVersion = VK_API_VERSION_1_1, // 1.0?
};

static const char *my_extension_names[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#if 0
    VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
    VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    VK_KHR_MIR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
};

static const char *my_device_extension_names[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#if ENABLE_DEBUG
static const char *my_layers[] = {
    "VK_LAYER_LUNARG_standard_validation",
};

static const char *vk_res2str(VkResult res)
{
    switch (res) {
        case VK_SUCCESS:                        return "sucess";
        case VK_NOT_READY:                      return "not ready";
        case VK_TIMEOUT:                        return "timeout";
        case VK_EVENT_SET:                      return "event set";
        case VK_EVENT_RESET:                    return "event reset";
        case VK_INCOMPLETE:                     return "incomplete";
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return "out of host memory";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "out of device memory";
        case VK_ERROR_INITIALIZATION_FAILED:    return "initialization failed";
        case VK_ERROR_DEVICE_LOST:              return "device lost";
        case VK_ERROR_MEMORY_MAP_FAILED:        return "memory map failed";
        case VK_ERROR_LAYER_NOT_PRESENT:        return "layer not present";
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return "extension not present";
        case VK_ERROR_FEATURE_NOT_PRESENT:      return "feature not present";
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return "incompatible driver";
        case VK_ERROR_TOO_MANY_OBJECTS:         return "too many objects";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "format not supported";
        case VK_ERROR_FRAGMENTED_POOL:          return "fragmented pool";
        case VK_ERROR_OUT_OF_POOL_MEMORY:       return "out of pool memory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:  return "invalid external handle";
        case VK_ERROR_SURFACE_LOST_KHR:         return "surface lost (KHR)";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native window in use (KHR)";
        case VK_SUBOPTIMAL_KHR:                 return "suboptimal (KHR)";
        case VK_ERROR_OUT_OF_DATE_KHR:          return "out of date (KHR)";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "incompatible display (KHR)";
        case VK_ERROR_VALIDATION_FAILED_EXT:    return "validation failed ext";
        case VK_ERROR_INVALID_SHADER_NV:        return "invalid shader nv";
        case VK_ERROR_FRAGMENTATION_EXT:        return "fragmentation ext";
        case VK_ERROR_NOT_PERMITTED_EXT:        return "not permitted ext";
        default:                                return "unknown";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags,
                                                     VkDebugReportObjectTypeEXT obj_type,
                                                     uint64_t obj,
                                                     size_t location,
                                                     int32_t code,
                                                     const char* layer_prefix,
                                                     const char* msg,
                                                     void *user_data)
{
    LOG(INFO, "[%s @ 0x%"PRIx64"] [%s%s%s%s%s ]: %s", layer_prefix, obj,
            flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT         ? " INFO"  : "",
            flags & VK_DEBUG_REPORT_WARNING_BIT_EXT             ? " WARN"  : "",
            flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ? " PERF"  : "",
            flags & VK_DEBUG_REPORT_ERROR_BIT_EXT               ? " ERROR" : "",
            flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT               ? " DEBUG" : "",
            msg);
    return VK_FALSE;
}
#endif

static VkSurfaceFormatKHR select_swapchain_surface_format(const VkSurfaceFormatKHR *formats,
                                                          uint32_t nb_formats)
{
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

static uint32_t clip_u32(uint32_t x, uint32_t min, uint32_t max)
{
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}

static VkExtent2D select_swapchain_current_extent(const struct glcontext *vk,
                                                  VkSurfaceCapabilitiesKHR caps)
{
    if (caps.currentExtent.width != UINT32_MAX) {
        LOG(DEBUG, "current extent: %dx%d", caps.currentExtent.width, caps.currentExtent.height);
        return caps.currentExtent;
    }

    VkExtent2D ext = {
        .width  = clip_u32(vk->config.width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
        .height = clip_u32(vk->config.height, caps.minImageExtent.height, caps.maxImageExtent.height),
    };
    LOG(DEBUG, "swapchain extent %dx%d", ext.width, ext.height);
    return ext;
}

static VkResult query_swapchain_support(struct vk_swapchain_support *swap,
                                        VkSurfaceKHR surface,
                                        VkPhysicalDevice phy_device)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &swap->caps);

    free(swap->formats);
    swap->formats = NULL;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &swap->nb_formats, NULL);
    if (swap->nb_formats) {
        swap->formats = calloc(swap->nb_formats, sizeof(*swap->formats));
        if (!swap->formats)
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &swap->nb_formats, swap->formats);
    }

    free(swap->present_modes);
    swap->present_modes = NULL;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &swap->nb_present_modes, NULL);
    if (swap->nb_present_modes) {
        swap->present_modes = calloc(swap->nb_present_modes, sizeof(*swap->present_modes));
        if (!swap->present_modes)
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &swap->nb_present_modes, swap->present_modes);
    }

    return VK_SUCCESS;
}

static VkResult probe_vulkan_extensions(struct glcontext *vk)
{
    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
    VkExtensionProperties *ext_props = calloc(ext_count, sizeof(*ext_props));
    if (!ext_props)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkEnumerateInstanceExtensionProperties(NULL, &ext_count, ext_props);
    LOG(DEBUG, "Vulkan extensions available:");
    for (uint32_t i = 0; i < ext_count; i++) {
        LOG(DEBUG, "  %d/%d: %s v%d", i+1, ext_count,
               ext_props[i].extensionName, ext_props[i].specVersion);
        if (!strcmp(ext_props[i].extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
            vk->surface_create_type = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    }
    free(ext_props);
    return VK_SUCCESS;
}

static VkResult list_vulkan_layers(void)
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties *layer_props = calloc(layer_count, sizeof(*layer_props));
    if (!layer_props)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkEnumerateInstanceLayerProperties(&layer_count, layer_props);
    LOG(DEBUG, "Vulkan layers available:");
    for (uint32_t i = 0; i < layer_count; i++)
        LOG(DEBUG, "  %d/%d: %s", i+1, layer_count, layer_props[i].layerName);
    free(layer_props);
    return VK_SUCCESS;
}

static VkResult create_vulkan_instance(struct glcontext *vk)
{
    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = NGLI_ARRAY_NB(my_extension_names),
        .ppEnabledExtensionNames = my_extension_names,
#if ENABLE_DEBUG
        .enabledLayerCount = NGLI_ARRAY_NB(my_layers),
        .ppEnabledLayerNames = my_layers,
#endif
    };
    return vkCreateInstance(&instance_create_info, NULL, &vk->instance);
}

static void *vulkan_get_proc_addr(struct glcontext *vk, const char *name)
{
    void *proc_addr = vkGetInstanceProcAddr(vk->instance, name);
    if (!proc_addr) {
        LOG(ERROR, "can not find %s extension", name);
        return NULL;
    }
    return proc_addr;
}

#if ENABLE_DEBUG
static VkResult setup_vulkan_debug_callback(struct glcontext *vk)
{
    void *proc_addr = vulkan_get_proc_addr(vk, "vkCreateDebugReportCallbackEXT");
    if (!proc_addr)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    PFN_vkCreateDebugReportCallbackEXT func = proc_addr;
    VkDebugReportCallbackCreateInfoEXT callback_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = 0 /* VK_DEBUG_REPORT_INFORMATION_BIT_EXT */
               | VK_DEBUG_REPORT_WARNING_BIT_EXT
               | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
               | VK_DEBUG_REPORT_ERROR_BIT_EXT
               /* | VK_DEBUG_REPORT_DEBUG_BIT_EXT*/,
        .pfnCallback = debug_callback,
    };
    return func(vk->instance, &callback_create_info, NULL, &vk->report_callback);
}
#endif

static int string_in(const char *target, const char * const *str_list, int nb_str)
{
    for (int i = 0; i < nb_str; i++)
        if (!strcmp(target, str_list[i]))
            return 1;
    return 0;
}

static VkResult get_filtered_ext_props(const VkExtensionProperties *src_props, uint32_t nb_src_props,
                                       VkExtensionProperties **dst_props_p, uint32_t *nb_dst_props_p,
                                       const char * const *filtered_names, int nb_filtered_names)
{
    *dst_props_p = NULL;
    *nb_dst_props_p = 0;

    VkExtensionProperties *dst_props = calloc(nb_src_props, sizeof(*dst_props));
    if (!dst_props_p)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    int nb_dst_props = 0;
    for (int j = 0; j < nb_src_props; j++) {
        if (!string_in(src_props[j].extensionName, filtered_names, nb_filtered_names))
            continue;
        dst_props[nb_dst_props++] = src_props[j];
        if (nb_dst_props == nb_filtered_names)
            break;
    }

    *dst_props_p = dst_props;
    *nb_dst_props_p = nb_dst_props;
    return VK_SUCCESS;
}

static VkResult select_vulkan_physical_device(struct glcontext *vk)
{
    uint32_t phydevice_count = 0;
    vkEnumeratePhysicalDevices(vk->instance, &phydevice_count, NULL);
    if (!phydevice_count) {
        LOG(ERROR, "no physical device available");
        return VK_ERROR_DEVICE_LOST;
    }
    VkPhysicalDevice *phy_devices = calloc(phydevice_count, sizeof(*phy_devices));
    if (!phy_devices)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkEnumeratePhysicalDevices(vk->instance, &phydevice_count, phy_devices);
    LOG(INFO, "Vulkan physical devices available:");
    for (uint32_t i = 0; i < phydevice_count; i++) {
        VkPhysicalDevice phy_device = phy_devices[i];

        /* Device properties */
        static const char *physical_device_types[] = {
            [VK_PHYSICAL_DEVICE_TYPE_OTHER] = "other",
            [VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU] = "integrated",
            [VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU] = "discrete",
            [VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU] = "virtual",
            [VK_PHYSICAL_DEVICE_TYPE_CPU] = "cpu",
        };
        VkPhysicalDeviceProperties dev_props;
        vkGetPhysicalDeviceProperties(phy_device, &dev_props);
        LOG(INFO, "  %d/%d: %s (%s)", i+1, phydevice_count,
               dev_props.deviceName,
               physical_device_types[dev_props.deviceType]);

        /* Device features */
        VkPhysicalDeviceFeatures dev_features;
        vkGetPhysicalDeviceFeatures(phy_device, &dev_features);

        /* Device queue families */
        int queue_family_graphics_id = -1;
        int queue_family_present_id = -1;
        uint32_t qfamily_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &qfamily_count, NULL);
        VkQueueFamilyProperties *qfamily_props = calloc(qfamily_count, sizeof(*qfamily_props));
        if (!qfamily_props)
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &qfamily_count, qfamily_props);
        LOG(DEBUG, "  queue props:");
        for (uint32_t j = 0; j < qfamily_count; j++) {
            VkQueueFamilyProperties props = qfamily_props[j];
            LOG(DEBUG, "    family %d/%d:%s%s%s%s%s (count: %d)", j+1, qfamily_count,
                   props.queueFlags & VK_QUEUE_GRAPHICS_BIT       ? " Graphics"      : "",
                   props.queueFlags & VK_QUEUE_COMPUTE_BIT        ? " Compute"       : "",
                   props.queueFlags & VK_QUEUE_TRANSFER_BIT       ? " Transfer"      : "",
                   props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ? " SparseBinding" : "",
                   props.queueFlags & VK_QUEUE_PROTECTED_BIT      ? " Protected"     : "",
                   props.queueCount);
            if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                queue_family_graphics_id = j;

            VkBool32 surface_support;
            vkGetPhysicalDeviceSurfaceSupportKHR(phy_device, j, vk->surface, &surface_support);
            if (surface_support)
                queue_family_present_id = j;
        }
        free(qfamily_props);

        /* Device extensions */
        uint32_t extprops_count;
        vkEnumerateDeviceExtensionProperties(phy_device, NULL, &extprops_count, NULL);
        VkExtensionProperties *ext_props = calloc(extprops_count, sizeof(*ext_props));
        if (!ext_props)
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        vkEnumerateDeviceExtensionProperties(phy_device, NULL, &extprops_count, ext_props);
        LOG(DEBUG, "  extensions available:");
        for (uint32_t j = 0; j < extprops_count; j++) {
            LOG(DEBUG, "    %d/%d: %s v%d", j+1, extprops_count,
                   ext_props[j].extensionName, ext_props[j].specVersion);
        }

        /* Filtered device extensions */
        VkExtensionProperties *my_ext_props;
        uint32_t my_ext_props_count;
        uint32_t my_ext_props_target_count = NGLI_ARRAY_NB(my_device_extension_names);
        VkResult ret = get_filtered_ext_props(ext_props, extprops_count,
                                              &my_ext_props, &my_ext_props_count,
                                              my_device_extension_names, NGLI_ARRAY_NB(my_device_extension_names));
        free(ext_props);
        free(my_ext_props);
        if (ret != VK_SUCCESS)
            return ret;

        /* Swapchain support */
        ret = query_swapchain_support(&vk->swapchain_support, vk->surface, phy_device);
        if (ret != VK_SUCCESS)
            return ret;
        LOG(DEBUG, "  Swapchain: %d formats, %d presentation modes",
               vk->swapchain_support.nb_formats, vk->swapchain_support.nb_present_modes);

        /* Device selection criterias */
        LOG(DEBUG, "  Graphics:%d Present:%d DeviceEXT:%d/%d",
               queue_family_graphics_id, queue_family_present_id,
               my_ext_props_count, my_ext_props_target_count);
        if (!vk->physical_device &&
            queue_family_graphics_id != -1 &&
            queue_family_present_id != -1 &&
            my_ext_props_count == my_ext_props_target_count &&
            vk->swapchain_support.nb_formats &&
            vk->swapchain_support.nb_present_modes) {
            LOG(DEBUG, "  -> device selected");
            vk->physical_device = phy_device;
            vk->queue_family_graphics_id = queue_family_graphics_id;
            vk->queue_family_present_id = queue_family_present_id;
        }
    }
    free(phy_devices);
    if (!vk->physical_device) {
        LOG(ERROR, "no valid physical device found");
        return VK_ERROR_DEVICE_LOST;
    }

    vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &vk->phydev_mem_props);

    return VK_SUCCESS;
}

static VkResult create_vulkan_device(struct glcontext *vk)
{
    /* Device Queue info */
    int nb_queues = 1;

    VkDeviceQueueCreateInfo graphic_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk->queue_family_graphics_id,
        .queueCount = 1,
        .pQueuePriorities = (const float[]){1.0f},
    };

    VkDeviceQueueCreateInfo queues_create_info[2];
    queues_create_info[0] = graphic_queue_create_info;
    if (vk->queue_family_graphics_id != vk->queue_family_present_id) {
        // XXX: needed?
        VkDeviceQueueCreateInfo present_queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = vk->queue_family_present_id,
            .queueCount = 1,
            .pQueuePriorities = (const float[]){1.0f},
        };
        queues_create_info[nb_queues++] = present_queue_create_info;
    }

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queues_create_info,
        .queueCreateInfoCount = nb_queues,
        .enabledExtensionCount = NGLI_ARRAY_NB(my_device_extension_names),
        .ppEnabledExtensionNames = my_device_extension_names,
    };
    return vkCreateDevice(vk->physical_device, &device_create_info, NULL, &vk->device);
}

static VkResult create_swapchain(struct glcontext *vk)
{
    // set maximum in flight frames
    vk->nb_in_flight_frames = 2;

    // re-query the swapchain to get current extent
    VkResult ret = query_swapchain_support(&vk->swapchain_support, vk->surface, vk->physical_device);
    if (ret != VK_SUCCESS)
        return ret;

    const struct vk_swapchain_support *swap = &vk->swapchain_support;

    vk->surface_format = select_swapchain_surface_format(swap->formats, swap->nb_formats);
    vk->present_mode = select_swapchain_present_mode(swap->present_modes, swap->nb_present_modes);
    vk->extent = select_swapchain_current_extent(vk, swap->caps);
    vk->config.width = vk->extent.width;
    vk->config.height = vk->extent.height;
    LOG(DEBUG, "current extent: %dx%d", vk->extent.width, vk->extent.height);

    uint32_t img_count = swap->caps.minImageCount + 1;
    if (swap->caps.maxImageCount && img_count > swap->caps.maxImageCount)
        img_count = swap->caps.maxImageCount;
    LOG(INFO, "swapchain image count: %d [%d-%d]", img_count,
           swap->caps.minImageCount, swap->caps.maxImageCount);

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surface,
        .minImageCount = img_count,
        .imageFormat = vk->surface_format.format,
        .imageColorSpace = vk->surface_format.colorSpace,
        .imageExtent = vk->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = swap->caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = vk->present_mode,
        .clipped = VK_TRUE,
    };

    const uint32_t queue_family_indices[2] = {
        vk->queue_family_graphics_id,
        vk->queue_family_present_id,
    };
    if (queue_family_indices[0] != queue_family_indices[1]) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = NGLI_ARRAY_NB(queue_family_indices);
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }

    return vkCreateSwapchainKHR(vk->device, &swapchain_create_info, NULL, &vk->swapchain);
}

// XXX: re-entrant
static VkResult create_swapchain_images(struct glcontext *vk)
{
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->nb_images, NULL);
    VkImage *imgs = realloc(vk->images, vk->nb_images * sizeof(*vk->images));
    if (!imgs)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vk->images = imgs;
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->nb_images, vk->images);
    return VK_SUCCESS;
}

static VkResult create_swapchain_image_views(struct glcontext *vk)
{
    vk->nb_image_views = vk->nb_images;
    vk->image_views = calloc(vk->nb_image_views, sizeof(*vk->image_views));
    if (!vk->image_views)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    for (uint32_t i = 0; i < vk->nb_images; i++) {
        VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk->images[i],
            .viewType = VK_IMAGE_TYPE_2D,
            .format = vk->surface_format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VkResult ret = vkCreateImageView(vk->device, &image_view_create_info,
                                         NULL, &vk->image_views[i]);
        if (ret != VK_SUCCESS)
            return ret;
    }

    return VK_SUCCESS;
}

static VkResult create_render_pass(struct glcontext *vk)
{
    VkAttachmentDescription color_attachment = {
        .format = vk->surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return vkCreateRenderPass(vk->device, &render_pass_create_info, NULL, &vk->render_pass);
}

static VkResult create_swapchain_framebuffers(struct glcontext *vk)
{
    vk->nb_framebuffers = vk->nb_image_views;
    vk->framebuffers = calloc(vk->nb_framebuffers, sizeof(*vk->framebuffers));
    if (!vk->framebuffers)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    for (uint32_t i = 0; i < vk->nb_framebuffers; i++) {
        //VkImageView attachments[] = {
        //    vk->image_views[i]
        //};

        VkFramebufferCreateInfo framebuffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vk->render_pass,
            .attachmentCount = 1,
            .pAttachments = &vk->image_views[i], //attachments,
            .width = vk->extent.width,
            .height = vk->extent.height,
            .layers = 1,
        };

        VkResult ret = vkCreateFramebuffer(vk->device, &framebuffer_create_info,
                                           NULL, &vk->framebuffers[i]);
        if (ret != VK_SUCCESS)
            return ret;
    }

    return VK_SUCCESS;
}

static VkResult create_command_pool(struct glcontext *vk)
{
    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk->queue_family_graphics_id,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // XXX
    };

    return vkCreateCommandPool(vk->device, &command_pool_create_info, NULL, &vk->command_pool);
}

static VkResult create_command_buffers(struct glcontext *vk)
{
    vk->nb_command_buffers = vk->nb_framebuffers;
    vk->command_buffers = calloc(vk->nb_command_buffers, sizeof(*vk->command_buffers));
    if (!vk->command_buffers)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    VkCommandBufferAllocateInfo command_buffers_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vk->nb_command_buffers,
    };

    VkResult ret = vkAllocateCommandBuffers(vk->device, &command_buffers_allocate_info,
                                            vk->command_buffers);
    if (ret != VK_SUCCESS)
        return ret;

    return VK_SUCCESS;
}

static VkResult create_semaphores(struct glcontext *vk)
{
    VkResult ret;

    vk->sem_img_avail = calloc(vk->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!vk->sem_img_avail)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    vk->sem_render_finished = calloc(vk->nb_in_flight_frames, sizeof(VkSemaphore));
    if (!vk->sem_render_finished)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    vk->fences = calloc(vk->nb_in_flight_frames, sizeof(VkFence));
    if (!vk->fences)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < vk->nb_in_flight_frames; i++) {
        if ((ret = vkCreateSemaphore(vk->device, &semaphore_create_info, NULL,
                                    &vk->sem_img_avail[i])) != VK_SUCCESS ||
            (ret = vkCreateSemaphore(vk->device, &semaphore_create_info, NULL,
                                    &vk->sem_render_finished[i])) != VK_SUCCESS ||
            (ret = vkCreateFence(vk->device, &fence_create_info, NULL,
                                 &vk->fences[i])) != VK_SUCCESS) {
            return ret;
        }
    }
    return VK_SUCCESS;
}

static VkResult create_window_surface(struct glcontext *vk,
                                      uintptr_t display,
                                      uintptr_t window,
                                      VkSurfaceKHR *surface)
{
    if (vk->surface_create_type == VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR) {
        VkXlibSurfaceCreateInfoKHR surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy = (Display *)display,
            .window = window,
        };

        if (!surface_create_info.dpy) {
            surface_create_info.dpy = XOpenDisplay(NULL);
            if (!surface_create_info.dpy) {
                LOG(ERROR, "could not retrieve X display");
                return -1;
            }
            // TODO free display
            //x11->own_display = 1;
        }

        PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR = vulkan_get_proc_addr(vk, "vkCreateXlibSurfaceKHR");
        if (!vkCreateXlibSurfaceKHR)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        return vkCreateXlibSurfaceKHR(vk->instance, &surface_create_info, NULL, surface);
    } else {
        // TODO
        ngli_assert(0);
    }

    return VK_ERROR_INITIALIZATION_FAILED;
}

static int vulkan_init(struct glcontext *vk, uintptr_t display, uintptr_t window)
{
    VkResult ret;

    // XXX: offscreen

    if ((ret = probe_vulkan_extensions(vk)) != VK_SUCCESS ||
        (ret = list_vulkan_layers()) != VK_SUCCESS ||
        (ret = create_vulkan_instance(vk)) != VK_SUCCESS ||
#if ENABLE_DEBUG
        (ret = setup_vulkan_debug_callback(vk)) != VK_SUCCESS ||
#endif
        (ret = create_window_surface(vk, display, window, &vk->surface)) != VK_SUCCESS ||
        (ret = select_vulkan_physical_device(vk)) != VK_SUCCESS ||
        (ret = create_vulkan_device(vk)) != VK_SUCCESS ||

        (ret = create_swapchain(vk)) != VK_SUCCESS ||
        (ret = create_swapchain_images(vk)) != VK_SUCCESS ||
        (ret = create_swapchain_image_views(vk)) != VK_SUCCESS ||
        (ret = create_render_pass(vk)) != VK_SUCCESS ||
        //(ret = create_graphics_pipelines(vk)) != VK_SUCCESS ||
        (ret = create_swapchain_framebuffers(vk)) != VK_SUCCESS ||
        (ret = create_command_pool(vk)) != VK_SUCCESS ||
        (ret = create_command_buffers(vk)) != VK_SUCCESS ||

        (ret = create_semaphores(vk)) != VK_SUCCESS) {
        //vulkan_uninit(vk);
        return -1;
    }

    return 0;
}

static void vulkan_swap_buffers(struct glcontext *vk)
{
    // XXX: store it in context
    VkQueue graphic_queue;
    vkGetDeviceQueue(vk->device, vk->queue_family_graphics_id, 0, &graphic_queue);
    VkQueue present_queue;
    vkGetDeviceQueue(vk->device, vk->queue_family_present_id, 0, &present_queue);

    VkSemaphore wait_sem[] = {vk->sem_img_avail[vk->current_frame]};
    VkSemaphore sig_sem[] = {vk->sem_render_finished[vk->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = NGLI_ARRAY_NB(wait_sem),
        .pWaitSemaphores = wait_sem,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk->command_buffers[vk->img_index],
        .signalSemaphoreCount = NGLI_ARRAY_NB(sig_sem),
        .pSignalSemaphores = sig_sem,
    };

    VkResult ret = vkQueueSubmit(graphic_queue, 1, &submit_info, vk->fences[vk->current_frame]);
    if (ret != VK_SUCCESS)
        LOG(ERROR, "submit failed");

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = sig_sem,
        .swapchainCount = 1,
        .pSwapchains = &vk->swapchain,
        .pImageIndices = &vk->img_index,
    };

    ret = vkQueuePresentKHR(present_queue, &present_info);
    if (ret == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG(ERROR, "PRESENT OUT OF DATE");
    } else if (ret == VK_SUBOPTIMAL_KHR) {
        LOG(ERROR, "PRESENT SUBOPTIMAL");
    } else if (ret != VK_SUCCESS) {
        LOG(ERROR, "failed to present image %s", vk_res2str(ret));
    }

    vk->current_frame = (vk->current_frame + 1) % vk->nb_in_flight_frames;
}

static void cleanup_swapchain(struct glcontext *vk)
{
    vkFreeCommandBuffers(vk->device, vk->command_pool,
                         vk->nb_command_buffers, vk->command_buffers);

    for (uint32_t i = 0; i < vk->nb_framebuffers; i++)
        vkDestroyFramebuffer(vk->device, vk->framebuffers[i], NULL);

    //vkDestroyPipeline(vk->device, vk->graphics_pipeline, NULL);
    //vkDestroyPipelineLayout(vk->device, vk->pipeline_layout, NULL);
    vkDestroyRenderPass(vk->device, vk->render_pass, NULL);

    for (uint32_t i = 0; i < vk->nb_image_views; i++)
        vkDestroyImageView(vk->device, vk->image_views[i], NULL);

    vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
}

// XXX: window minimizing? (fb gets zero width or height)
static int reset_swapchain(struct glcontext *vk)
{
    VkResult ret;

    LOG(INFO, "reset swapchain");
    vkDeviceWaitIdle(vk->device);
    cleanup_swapchain(vk);
    if ((ret = create_swapchain(vk)) != VK_SUCCESS ||
        (ret = create_swapchain_images(vk)) != VK_SUCCESS ||
        (ret = create_swapchain_image_views(vk)) != VK_SUCCESS ||
        (ret = create_render_pass(vk)) != VK_SUCCESS ||
        //(ret = create_graphics_pipelines(vk)) != VK_SUCCESS ||
        (ret = create_swapchain_framebuffers(vk)) != VK_SUCCESS ||
        (ret = create_command_buffers(vk)) != VK_SUCCESS)
        return -1;

    return 0;
}

static void vulkan_uninit(struct glcontext *vk)
{
    for (int i = 0; i < vk->nb_in_flight_frames; i++) {
        vkDestroySemaphore(vk->device, vk->sem_render_finished[i], NULL);
        vkDestroySemaphore(vk->device, vk->sem_img_avail[i], NULL);
        vkDestroyFence(vk->device, vk->fences[i], NULL);
    }

    cleanup_swapchain(vk);

    vkDestroyCommandPool(vk->device, vk->command_pool, NULL);

    free(vk->swapchain_support.formats);
    free(vk->swapchain_support.present_modes);
    vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
    free(vk->images);
    free(vk->image_views);
    vkDestroyDevice(vk->device, NULL);
#if ENABLE_DEBUG
    void *proc_addr = vulkan_get_proc_addr(vk, "vkDestroyDebugReportCallbackEXT");
    if (proc_addr) {
        PFN_vkDestroyDebugReportCallbackEXT func = proc_addr;
        func(vk->instance, vk->report_callback, NULL);
    }
#endif
    vkDestroyInstance(vk->instance, NULL);
}

static int vk_reconfigure(struct ngl_ctx *s, const struct ngl_config *config)
{
    struct glcontext *vk = s->glcontext;

    memcpy(&vk->config, config, sizeof(*config));
    return 0;
}

static int vk_configure(struct ngl_ctx *s, const struct ngl_config *config)
{
    struct glcontext *vk = calloc(1, sizeof(*vk));
    if (!vk)
        return -1;

    memcpy(&vk->config, config, sizeof(*config));

    int ret = vulkan_init(vk, config->display, config->window);
    if (ret < 0) {
        free(vk);
        return ret;
    }

    s->glcontext = vk;
    ngli_glstate_probe(vk, &s->glstate);

    return 0;
}

static int vk_pre_draw(struct ngl_ctx *s)
{
    int ret;
    struct glcontext *vk = s->glcontext;

    // XXX: store it in context
    VkQueue present_queue;
    vkGetDeviceQueue(vk->device, vk->queue_family_present_id, 0, &present_queue);

    vkWaitForFences(vk->device, 1, &vk->fences[vk->current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(vk->device, 1, &vk->fences[vk->current_frame]);

    VkResult vkret = vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX,
                                           vk->sem_img_avail[vk->current_frame], NULL, &vk->img_index);

    if (vkret == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG(ERROR, "ACQUIRE OUT OF DATE");
        ret = reset_swapchain(vk);
    } else if (vkret == VK_SUBOPTIMAL_KHR) {
        LOG(ERROR, "ACQUIRE SUBOPTIMAL");
        ret = reset_swapchain(vk);
    } else if (vkret != VK_SUCCESS) {
        LOG(ERROR, "failed to acquire image");
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int vk_post_draw(struct ngl_ctx *s, double t, int ret)
{
    struct glcontext *vk = s->glcontext;
    vulkan_swap_buffers(vk);
    return ret;
}

static int vk_destroy(struct ngl_ctx *s)
{
    struct glcontext *vk = s->glcontext;
    vulkan_uninit(vk);
    free(vk);
    s->glcontext = NULL;
    return 0;
}

const struct backend ngli_backend_vk = {
    .name         = "Vulkan",
    .reconfigure  = vk_reconfigure,
    .configure    = vk_configure,
    .pre_draw     = vk_pre_draw,
    .post_draw    = vk_post_draw,
    .destroy      = vk_destroy,
};
