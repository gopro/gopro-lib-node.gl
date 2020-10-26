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
#include "ngfx/porting/vulkan/VKInstance.h"
#include "ngfx/core/DebugUtil.h"
#include "ngfx/porting/vulkan/VKDebugUtil.h"
#include "ngfx/porting/vulkan/VKConfig.h"
#include <cstring>
using namespace ngfx;

void VKInstance::create(const char* appName, const char* engineName, uint32_t apiVersion, bool enableValidation)
{
    VkResult vkResult;
    this->settings.validation = enableValidation;

    // Validation can also be forced via a define
#if defined(_VALIDATION)
    this->settings.validation = true;
#endif

    appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.pEngineName = engineName;
    appInfo.apiVersion = apiVersion;

    instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    // Enable surface extensions depending on os
    std::vector<const char*> surfaceExtensions = { VK_SURFACE_EXTENSION_NAMES };
    for (auto& ext: surfaceExtensions) instanceExtensions.push_back(ext);

    if (enabledInstanceExtensions.size() > 0) {
        for (auto enabledExtension : enabledInstanceExtensions) {
            instanceExtensions.push_back(enabledExtension);
        }
    }

    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.pApplicationInfo = &appInfo;
    if (instanceExtensions.size() > 0)
    {
        if (settings.validation)
        {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }
    if (settings.validation)
    {
        // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
        // Note that on Android this layer requires at least NDK r20
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
        // Check if this layer is available at v level
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        instanceLayerProperties.resize(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            createInfo.ppEnabledLayerNames = &validationLayerName;
            createInfo.enabledLayerCount = 1;
        } else {
            ERR("Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled");
        }
    }
    V(vkCreateInstance(&createInfo, nullptr, &v));
}

VKInstance::~VKInstance() {
    if (v) VK_TRACE(vkDestroyInstance(v, nullptr));
}
