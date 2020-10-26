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

#ifndef ANDROID_IMAGEREADER_H
#define ANDROID_IMAGEREADER_H

#include <stdint.h>
#include <android/hardware_buffer.h>

#include "android_ctx.h"

enum {
    NGLI_ANDROID_IMAGE_FORMAT_RGBA_8888         = 0x1,
    NGLI_ANDROID_IMAGE_FORMAT_RGBX_8888         = 0x2,
    NGLI_ANDROID_IMAGE_FORMAT_RGB_888           = 0x3,
    NGLI_ANDROID_IMAGE_FORMAT_RGB_565           = 0x4,
    NGLI_ANDROID_IMAGE_FORMAT_RGBA_FP16         = 0x16,
    NGLI_ANDROID_IMAGE_FORMAT_YUV_420_888       = 0x23,
    NGLI_ANDROID_IMAGE_FORMAT_JPEG              = 0x100,
    NGLI_ANDROID_IMAGE_FORMAT_RAW16             = 0x20,
    NGLI_ANDROID_IMAGE_FORMAT_RAW_PRIVATE       = 0x24,
    NGLI_ANDROID_IMAGE_FORMAT_RAW10             = 0x25,
    NGLI_ANDROID_IMAGE_FORMAT_RAW12             = 0x26,
    NGLI_ANDROID_IMAGE_FORMAT_DEPTH16           = 0x44363159,
    NGLI_ANDROID_IMAGE_FORMAT_DEPTH_POINT_CLOUD = 0x101,
    NGLI_ANDROID_IMAGE_FORMAT_PRIVATE           = 0x22,
    NGLI_ANDROID_IMAGE_FORMAT_Y8                = 0x20203859,
    NGLI_ANDROID_IMAGE_FORMAT_HEIC              = 0x48454946,
    NGLI_ANDROID_IMAGE_FORMAT_DEPTH_JPEG        = 0x69656963
};

struct android_image;

AHardwareBuffer *ngli_android_image_get_hardware_buffer(struct android_image *s);
void ngli_android_image_freep(struct android_image **sp);

struct android_imagereader;

struct android_imagereader *ngli_android_imagereader_create(struct android_ctx *api, int width, int height, int format, int max_images);
int  ngli_android_imagereader_get_window(struct android_imagereader *s, void **window);
int  ngli_android_imagereader_acquire_next_image(struct android_imagereader *s, struct android_image **imagep);
void ngli_android_imagereader_freep(struct android_imagereader **sp);

#endif /* ANDROID_IMAGEREADER_H */
