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
#include "util_vk.h"

const char *vk_res2str(VkResult res)
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
#ifdef VK_ERROR_OUT_OF_POOL_MEMORY
        case VK_ERROR_OUT_OF_POOL_MEMORY:       return "out of pool memory";
#endif
#ifdef VK_ERROR_INVALID_EXTERNAL_HANDLE
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:  return "invalid external handle";
#endif
        case VK_ERROR_SURFACE_LOST_KHR:         return "surface lost (KHR)";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native window in use (KHR)";
        case VK_SUBOPTIMAL_KHR:                 return "suboptimal (KHR)";
        case VK_ERROR_OUT_OF_DATE_KHR:          return "out of date (KHR)";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "incompatible display (KHR)";
        case VK_ERROR_VALIDATION_FAILED_EXT:    return "validation failed ext";
        case VK_ERROR_INVALID_SHADER_NV:        return "invalid shader nv";
#ifdef VK_ERROR_FRAGMENTATION_EXT
        case VK_ERROR_FRAGMENTATION_EXT:        return "fragmentation ext";
#endif
#ifdef VK_ERROR_NOT_PERMITTED_EXT
        case VK_ERROR_NOT_PERMITTED_EXT:        return "not permitted ext";
#endif
        default:                                return "unknown";
    }
}
