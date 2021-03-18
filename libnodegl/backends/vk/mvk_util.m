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

#include "mvk_util.h"
#include "mvk_util_internal.h"
#include "memory.h"
#include <MoltenVK/vk_mvk_moltenvk.h>

struct mvk_util *mvk_util_create(void)
{
    struct mvk_util *s = ngli_calloc(1, sizeof(*s));
    return s;
}

void mvk_util_freep(struct mvk_util **sp)
{
    if (!*sp)
        return;

    ngli_freep(sp);
}

void mvk_util_on_physical_device_created(struct mvk_util *s, VkPhysicalDevice physical_device)
{
    id<MTLDevice> mtl_device;
    vkGetMTLDeviceMVK(physical_device, &mtl_device);
    s->mtl_device = mtl_device;
}
