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

#if defined(TARGET_LINUX)
# define VK_USE_PLATFORM_XLIB_KHR
# define VK_USE_PLATFORM_WAYLAND_KHR
#elif defined(TARGET_ANDROID)
# define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(TARGET_DARWIN)
# define VK_USE_PLATFORM_MACOS_MVK
#elif defined(TARGET_IPHONE)
# define VK_USE_PLATFORM_IOS_MVK
#elif defined(TARGET_MINGW_W64)
# define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <string.h>

#include "format.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "vkcontext.h"

// FIXME: rely on DEBUG_VK
#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                                     void *user_data)
{
    int level = NGL_LOG_INFO;
    switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   level = NGL_LOG_ERROR;    break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = NGL_LOG_WARNING;  break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    level = NGL_LOG_INFO;     break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: level = NGL_LOG_VERBOSE;  break;
    default:
        break;
    }

    const char *msg_type = "GENERAL";
    switch (type) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: msg_type  = "VALIDATION"; break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: msg_type = "PERFORMANCE"; break;
    default:
        break;
    }

    ngli_log_print(level, __FILE__, __LINE__, "debug_callback", "%s: %s", msg_type, callback_data->pMessage);
    return VK_FALSE;
}
#endif

static const char *platform_ext_names[] = {
    [NGL_PLATFORM_XLIB]    = "VK_KHR_xlib_surface",
    [NGL_PLATFORM_ANDROID] = "VK_KHR_android_surface",
    [NGL_PLATFORM_MACOS]   = "VK_MVK_macos_surface",
    [NGL_PLATFORM_IOS]     = "VK_MVK_ios_surface",
    [NGL_PLATFORM_WINDOWS] = "VK_KHR_win32_surface",
    [NGL_PLATFORM_WAYLAND] = "VK_KHR_wayland_surface",
};

static VkResult create_instance(struct vkcontext *s, int platform)
{
    s->api_version = VK_API_VERSION_1_0;

    VK_LOAD_FUN(NULL, EnumerateInstanceVersion);
    if (EnumerateInstanceVersion) {
        VkResult res = EnumerateInstanceVersion(&s->api_version);
        if (res != VK_SUCCESS) {
            LOG(ERROR, "could not enumerate Vulkan instance version");
        }
    }

    LOG(DEBUG, "supported Vulkan version: %d.%d.%d",
        (int)VK_VERSION_MAJOR(s->api_version),
        (int)VK_VERSION_MINOR(s->api_version),
        (int)VK_VERSION_PATCH(s->api_version));

    VkResult res = vkEnumerateInstanceLayerProperties(&s->nb_layers, NULL);
    if (res != VK_SUCCESS)
        return res;

    s->layers = ngli_calloc(s->nb_layers, sizeof(*s->layers));
    if (!s->layers)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumerateInstanceLayerProperties(&s->nb_layers, s->layers);
    if (res != VK_SUCCESS)
        return res;

    LOG(DEBUG, "available Vulkan layers:");
    for (uint32_t i = 0; i < s->nb_layers; i++)
        LOG(DEBUG, "  %d/%d: %s", i+1, s->nb_layers, s->layers[i].layerName);

    res = vkEnumerateInstanceExtensionProperties(NULL, &s->nb_extensions, NULL);
    if (res != VK_SUCCESS)
        return res;

    s->extensions = ngli_calloc(s->nb_extensions, sizeof(*s->extensions));
    if (!s->extensions)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumerateInstanceExtensionProperties(NULL, &s->nb_extensions, s->extensions);
    if (res != VK_SUCCESS)
        return res;

    LOG(DEBUG, "available Vulkan extensions:");
    for (uint32_t i = 0; i < s->nb_extensions; i++) {
        LOG(DEBUG, "  %d/%d: %s v%d", i+1, s->nb_extensions,
            s->extensions[i].extensionName, s->extensions[i].specVersion);
    }

    if (platform < 0 || platform >= NGLI_ARRAY_NB(platform_ext_names)) {
        LOG(ERROR, "unsupported platform: %d", platform);
        return VK_ERROR_UNKNOWN;
    }

    const char *surface_extension_name = platform_ext_names[platform];
    if (!surface_extension_name) {
        LOG(ERROR, "unsupported platform: %d", platform);
        return VK_ERROR_UNKNOWN;
    }

    const char *mandatory_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surface_extension_name,
#ifdef VK_USE_PLATFORM_MACOS_MVK
        "VK_MVK_moltenvk",
        "VK_MVK_macos_surface",
        "VK_EXT_metal_surface",
        //"VK_EXT_debug_report",
        //"VK_EXT_debug_utils",
#endif
    };

    struct darray extensions;
    ngli_darray_init(&extensions, sizeof(const char **), 0);

    struct darray layers;
    ngli_darray_init(&layers, sizeof(const char **), 0);

    for (uint32_t i = 0; i < NGLI_ARRAY_NB(mandatory_extensions); i++) {
        if (!ngli_darray_push(&extensions, &mandatory_extensions[i])) {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
    }

#if ENABLE_DEBUG
    int has_debug_extension = 0;
    const char *debug_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    for (uint32_t i = 0; i < s->nb_extensions; i++) {
        if (!strcmp(s->extensions[i].extensionName, debug_extension)) {
            if (!ngli_darray_push(&extensions, &debug_extension)) {
                res = VK_ERROR_OUT_OF_HOST_MEMORY;
                goto done;
            }
            has_debug_extension = 1;
            break;
        }
    }

    int has_validation_layer = 0;
    const char *debug_layer = "VK_LAYER_KHRONOS_validation";
    for (uint32_t i = 0; i < s->nb_layers; i++) {
        if (!strcmp(s->layers[i].layerName, debug_layer)) {
            if (!ngli_darray_push(&layers, &debug_layer)) {
                res = VK_ERROR_OUT_OF_HOST_MEMORY;
                goto done;
            }
            has_validation_layer = 1;
            break;
        }
    }

    if (!has_validation_layer)
        LOG(WARNING, "missing validation layer: %s", debug_layer);
#endif

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName = "node.gl",
        .engineVersion = NODEGL_VERSION_INT,
        .apiVersion = s->api_version,
    };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = ngli_darray_count(&extensions),
        .ppEnabledExtensionNames = ngli_darray_data(&extensions),
        .enabledLayerCount = ngli_darray_count(&layers),
        .ppEnabledLayerNames = ngli_darray_data(&layers),
    };

    res = vkCreateInstance(&instance_create_info, NULL, &s->instance);
    if (res != VK_SUCCESS)
        return res;

#if ENABLE_DEBUG
    if (has_debug_extension) {
        VK_LOAD_FUN(s->instance, CreateDebugUtilsMessengerEXT);
        if (!CreateDebugUtilsMessengerEXT)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        VkDebugUtilsMessengerCreateInfoEXT info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
            .pUserData = NULL,
        };

        res = CreateDebugUtilsMessengerEXT(s->instance, &info, NULL, &s->debug_callback);
        if (res != VK_SUCCESS)
            return res;
    }
#endif

done:
    ngli_darray_reset(&extensions);
    ngli_darray_reset(&layers);

    return res;
}

static VkResult create_window_surface(struct vkcontext *s, const struct ngl_config *config)
{
    if (config->offscreen)
        return VK_SUCCESS;

    if (!config->window)
        return VK_ERROR_UNKNOWN;

    const int platform = config->platform;
    if (platform == NGL_PLATFORM_XLIB) {
#if defined(TARGET_LINUX)
        s->display = (Display *)config->display;
        if (!s->display) {
            s->display = XOpenDisplay(NULL);
            if (!s->display) {
                LOG(ERROR, "could not open X11 display");
                return VK_ERROR_UNKNOWN;
            }
            s->own_display = 1;
        }

        const VkXlibSurfaceCreateInfoKHR surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy = s->display,
            .window = config->window,
        };

        VK_LOAD_FUN(s->instance, CreateXlibSurfaceKHR);
        if (!CreateXlibSurfaceKHR) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateXlibSurfaceKHR(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    } else if (platform == NGL_PLATFORM_ANDROID) {
#if defined(TARGET_ANDROID)
        const VkAndroidSurfaceCreateInfoKHR surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .window = (const void *)config->window,
        };

        VK_LOAD_FUN(s->instance, CreateAndroidSurfaceKHR);
        if (!CreateAndroidSurfaceKHR) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateAndroidSurfaceKHR(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    } else if (platform == NGL_PLATFORM_MACOS) {
#if defined(TARGET_DARWIN)
        const VkMacOSSurfaceCreateInfoMVK surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
            .pView = (const void *)config->window,
        };

        VK_LOAD_FUN(s->instance, CreateMacOSSurfaceMVK);
        if (!CreateMacOSSurfaceMVK) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateMacOSSurfaceMVK(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#endif
    } else if (platform == NGL_PLATFORM_IOS) {
#if defined(TARGET_IPHONE)
        const VkIOSSurfaceCreateInfoMVK surface_create_info = {
            .sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK,
            .pView = (const void *)config->window,
        };

        VK_LOAD_FUN(s->instance, CreateIOSSurfaceMVK);
        if (!CreateIOSSurfaceMVK) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkResult res = CreateIOSSurfaceMVK(s->instance, &surface_create_info, NULL, &s->surface);
        if (res != VK_SUCCESS)
            return res;
#endif
    } else if (platform == NGL_PLATFORM_WINDOWS) {
        ngli_assert(0);
    } else if (platform == NGL_PLATFORM_WAYLAND) {
        ngli_assert(0);
    } else {
        ngli_assert(0);
    }

    return VK_SUCCESS;
}

static VkResult enumerate_physical_devices(struct vkcontext *s, const struct ngl_config *config)
{
    VkResult res = vkEnumeratePhysicalDevices(s->instance, &s->nb_phy_devices, NULL);
    if (res != VK_SUCCESS)
        return res;

    if (!s->nb_phy_devices)
        return VK_ERROR_DEVICE_LOST;

    s->phy_devices = ngli_calloc(s->nb_phy_devices, sizeof(*s->phy_devices));
    if (!s->phy_devices)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumeratePhysicalDevices(s->instance, &s->nb_phy_devices, s->phy_devices);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

static VkResult select_physical_device(struct vkcontext *s, const struct ngl_config *config)
{
    static const struct {
        const char *name;
        uint32_t priority;
    } types[] = {
        [VK_PHYSICAL_DEVICE_TYPE_OTHER]          = {"other",      1},
        [VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU] = {"integrated", 4},
        [VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU]   = {"discrete",   5},
        [VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU]    = {"virtual",    3},
        [VK_PHYSICAL_DEVICE_TYPE_CPU]            = {"cpu",        2},
    };

    uint32_t priority = 0;
    for (uint32_t i = 0; i < s->nb_phy_devices; i++) {
        VkPhysicalDevice phy_device = s->phy_devices[i];

        VkPhysicalDeviceProperties dev_props;
        vkGetPhysicalDeviceProperties(phy_device, &dev_props);

        VkPhysicalDeviceFeatures dev_features;
        vkGetPhysicalDeviceFeatures(phy_device, &dev_features);

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(phy_device, &mem_props);

        if (dev_props.deviceType >= NGLI_ARRAY_NB(types)) {
            LOG(ERROR, "device %s has unknown type: 0x%x, skipping", dev_props.deviceName, dev_props.deviceType);
            continue;
        }

        uint32_t qfamily_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &qfamily_count, NULL);
        VkQueueFamilyProperties *qfamily_props = ngli_calloc(qfamily_count, sizeof(*qfamily_props));
        if (!qfamily_props)
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &qfamily_count, qfamily_props);

        int32_t found_queues = 0;
        int32_t queue_family_graphics_id = -1;
        int32_t queue_family_present_id = -1;
        for (uint32_t j = 0; j < qfamily_count; j++) {
            const VkQueueFamilyProperties props = qfamily_props[j];
            const VkQueueFlags flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((props.queueFlags & flags) == flags)
                queue_family_graphics_id = j;
            if (s->surface) {
                VkBool32 support;
                vkGetPhysicalDeviceSurfaceSupportKHR(phy_device, j, s->surface, &support);
                if (support)
                    queue_family_present_id = j;
            }
            found_queues = queue_family_graphics_id >= 0 && (!s->surface || queue_family_present_id >= 0);
            if (found_queues)
                break;
        }
        ngli_free(qfamily_props);

        if (!found_queues)
            continue;

        if (types[dev_props.deviceType].priority > priority) {
            priority = types[dev_props.deviceType].priority;
            s->phy_device = phy_device;
            s->phy_device_props = dev_props;
            s->graphics_queue_index = queue_family_graphics_id;
            s->present_queue_index = queue_family_present_id;
            s->dev_features = dev_features;
            s->phydev_mem_props = mem_props;
        }
    }

    if (!s->phy_device) {
        LOG(ERROR, "no valid physical device found");
        return VK_ERROR_DEVICE_LOST;
    }

    LOG(DEBUG, "select physical device: %s, graphics queue: %d, present queue: %d",
        s->phy_device_props.deviceName, s->graphics_queue_index, s->present_queue_index);

    return VK_SUCCESS;
}

static VkResult enumerate_extensions(struct vkcontext *s)
{
    VkResult res = vkEnumerateDeviceExtensionProperties(s->phy_device, NULL, &s->nb_device_extensions, NULL);
    if (res != VK_SUCCESS)
        return res;

    s->device_extensions = ngli_calloc(s->nb_device_extensions, sizeof(*s->device_extensions));
    if (!s->device_extensions)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumerateDeviceExtensionProperties(s->phy_device, NULL, &s->nb_device_extensions, s->device_extensions);
    if (res != VK_SUCCESS)
        return res;

    return VK_SUCCESS;
}

static VkResult create_device(struct vkcontext *s)
{
    int nb_queues = 0;
    float queue_priority = 1.0;
    VkDeviceQueueCreateInfo queues_create_info[2];

    VkDeviceQueueCreateInfo graphics_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = s->graphics_queue_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    queues_create_info[nb_queues++] = graphics_queue_create_info;

    if (s->present_queue_index != -1 &&
        s->graphics_queue_index != s->present_queue_index) {
        VkDeviceQueueCreateInfo present_queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = s->present_queue_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
        queues_create_info[nb_queues++] = present_queue_create_info;
    }

    VkPhysicalDeviceFeatures dev_features = {0};

#define ENABLE_FEATURE(feature, mandatory) do {                                  \
    if (s->dev_features.feature) {                                               \
        dev_features.feature = VK_TRUE;                                          \
    } else if (mandatory) {                                                      \
        LOG(ERROR, "Mandatory feature " #feature " is not supported by device"); \
        return -1;                                                               \
    }                                                                            \
} while (0)                                                                      \

    ENABLE_FEATURE(samplerAnisotropy, 1);
    //Not supported on Intel Haswell GPU
    ENABLE_FEATURE(vertexPipelineStoresAndAtomics, 0);
    ENABLE_FEATURE(fragmentStoresAndAtomics, 0);
    ENABLE_FEATURE(shaderStorageImageExtendedFormats, 1);

#undef ENABLE_FEATURE

    static const char *my_device_extension_names[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queues_create_info,
        .queueCreateInfoCount = nb_queues,
        .enabledExtensionCount = NGLI_ARRAY_NB(my_device_extension_names),
        .ppEnabledExtensionNames = my_device_extension_names,
        .pEnabledFeatures = &dev_features,
    };
    VkResult ret = vkCreateDevice(s->phy_device, &device_create_info, NULL, &s->device);
    if (ret != VK_SUCCESS)
        return ret;

    vkGetDeviceQueue(s->device, s->graphics_queue_index, 0, &s->graphic_queue);
    if (s->present_queue_index != -1)
        vkGetDeviceQueue(s->device, s->present_queue_index, 0, &s->present_queue);
    return ret;

    return VK_SUCCESS;
}

VkFormat ngli_vkcontext_find_supported_format(struct vkcontext *s, const VkFormat *formats,
                                              VkImageTiling tiling, VkFormatFeatureFlags features)
{
    uint32_t i = 0;
    while (formats[i]) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(s->phy_device, formats[i], &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
            return formats[i];
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
            return formats[i];
        i++;
    }
    return VK_FORMAT_UNDEFINED;
}

int ngli_vkcontext_find_memory_type(struct vkcontext *s, uint32_t type, VkMemoryPropertyFlags props)
{
    for (int i = 0; i < s->phydev_mem_props.memoryTypeCount; i++)
        if ((type & (1<<i)) && (s->phydev_mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return -1;
}

static VkResult query_swapchain_support(struct vkcontext *s)
{
    if (!s->surface)
        return VK_SUCCESS;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->phy_device, s->surface, &s->surface_caps);

    vkGetPhysicalDeviceSurfaceFormatsKHR(s->phy_device, s->surface, &s->nb_surface_formats, NULL);
    if (!s->nb_surface_formats)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;

    s->surface_formats = ngli_calloc(s->nb_surface_formats, sizeof(*s->surface_formats));
    if (!s->surface_formats)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s->phy_device, s->surface, &s->nb_surface_formats, s->surface_formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(s->phy_device, s->surface, &s->nb_present_modes, NULL);
    if (!s->nb_present_modes)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;

    s->present_modes = ngli_calloc(s->nb_present_modes, sizeof(*s->present_modes));
    if (!s->present_modes)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    vkGetPhysicalDeviceSurfacePresentModesKHR(s->phy_device, s->surface, &s->nb_present_modes, s->present_modes);

    return VK_SUCCESS;
}

static int ngli_format_from_vk_format(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_D32_SFLOAT:
            return NGLI_FORMAT_D32_SFLOAT;
        case VK_FORMAT_D16_UNORM:
            return NGLI_FORMAT_D16_UNORM;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return NGLI_FORMAT_D32_SFLOAT_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return NGLI_FORMAT_D24_UNORM_S8_UINT;
        default:
            ngli_assert(0);
    }
}

static VkResult select_preferred_formats(struct vkcontext *s)
{
    const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkFormat depth_stencil_formats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        0
    };
    VkFormat format = ngli_vkcontext_find_supported_format(s, depth_stencil_formats, tiling, features);
    if (!format)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    s->preferred_depth_stencil_format = ngli_format_from_vk_format(format);

    const VkFormat depth_formats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        0
    };
    format = ngli_vkcontext_find_supported_format(s, depth_formats, tiling, features);
    if (!format)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    s->preferred_depth_format = ngli_format_from_vk_format(format);

    return VK_SUCCESS;
}

struct vkcontext *ngli_vkcontext_create(void)
{
    struct vkcontext *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return s;
    return s;
}

int ngli_vkcontext_init(struct vkcontext *s, const struct ngl_config *config)
{
    VkResult res = create_instance(s, config->platform);
    if (res != VK_SUCCESS)
        return -1;

    res = create_window_surface(s, config);
    if (res != VK_SUCCESS)
        return -1;

    res = enumerate_physical_devices(s, config);
    if (res != VK_SUCCESS)
        return -1;

    res = select_physical_device(s, config);
    if (res != VK_SUCCESS)
        return -1;

    res = enumerate_extensions(s);
    if (res != VK_SUCCESS)
        return -1;

    res = create_device(s);
    if (res != VK_SUCCESS)
        return -1;

    res = query_swapchain_support(s);
    if (res != VK_SUCCESS)
        return -1;

    res = select_preferred_formats(s);
    if (res != VK_SUCCESS)
        return -1;

    return 0;
}

void *ngli_vkcontext_get_proc_addr(struct vkcontext *s, const char *name)
{
    return vkGetInstanceProcAddr(s->instance, name);
}

void ngli_vkcontext_freep(struct vkcontext **sp)
{
    if (!*sp)
        return;

    struct vkcontext *s = *sp;

    if (s->device) {
        vkDeviceWaitIdle(s->device);
        vkDestroyDevice(s->device, NULL);
    }

    if (s->surface)
        vkDestroySurfaceKHR(s->instance, s->surface, NULL);

    if (s->debug_callback) {
        VK_LOAD_FUN(s->instance, DestroyDebugUtilsMessengerEXT);
        if (DestroyDebugUtilsMessengerEXT)
            DestroyDebugUtilsMessengerEXT(s->instance, s->debug_callback, NULL);
    }

    ngli_freep(&s->present_modes);
    ngli_freep(&s->surface_formats);
    ngli_freep(&s->device_extensions);
    ngli_freep(&s->phy_devices);
    ngli_freep(&s->layers);
    ngli_freep(&s->extensions);

    vkDestroyInstance(s->instance, NULL);

#if defined(TARGET_LINUX)
    if (s->own_display)
        XCloseDisplay(s->display);
#endif

    ngli_freep(sp);
}
