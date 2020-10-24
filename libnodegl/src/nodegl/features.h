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

#ifndef FEATURES_H
#define FEATURES_H

#define NGLI_FEATURE_VERTEX_ARRAY_OBJECT          (1 << 0)
#define NGLI_FEATURE_TEXTURE_3D                   (1 << 1)
#define NGLI_FEATURE_TEXTURE_STORAGE              (1 << 2)
#define NGLI_FEATURE_COMPUTE_SHADER               (1 << 3)
#define NGLI_FEATURE_PROGRAM_INTERFACE_QUERY      (1 << 4)
#define NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE      (1 << 5)
#define NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT (1 << 6)
#define NGLI_FEATURE_FRAMEBUFFER_OBJECT           (1 << 7)
#define NGLI_FEATURE_INTERNALFORMAT_QUERY         (1 << 8)
#define NGLI_FEATURE_PACKED_DEPTH_STENCIL         (1 << 9)
#define NGLI_FEATURE_TIMER_QUERY                  (1 << 10)
#define NGLI_FEATURE_EXT_DISJOINT_TIMER_QUERY     (1 << 11)
#define NGLI_FEATURE_DRAW_INSTANCED               (1 << 12)
#define NGLI_FEATURE_INSTANCED_ARRAY              (1 << 13)
#define NGLI_FEATURE_UNIFORM_BUFFER_OBJECT        (1 << 14)
#define NGLI_FEATURE_INVALIDATE_SUBDATA           (1 << 15)
#define NGLI_FEATURE_OES_EGL_EXTERNAL_IMAGE       (1 << 16)
#define NGLI_FEATURE_DEPTH_TEXTURE                (1 << 17)
#define NGLI_FEATURE_RGB8_RGBA8                   (1 << 18)
#define NGLI_FEATURE_OES_EGL_IMAGE                (1 << 19)
#define NGLI_FEATURE_EGL_IMAGE_BASE_KHR           (1 << 20)
#define NGLI_FEATURE_EGL_EXT_IMAGE_DMA_BUF_IMPORT (1 << 21)
#define NGLI_FEATURE_SYNC                         (1 << 22)
#define NGLI_FEATURE_YUV_TARGET                   (1 << 23)
#define NGLI_FEATURE_TEXTURE_NPOT                 (1 << 24)
#define NGLI_FEATURE_TEXTURE_CUBE_MAP             (1 << 25)
#define NGLI_FEATURE_DRAW_BUFFERS                 (1 << 26)
#define NGLI_FEATURE_ROW_LENGTH                   (1 << 27)
#define NGLI_FEATURE_SOFTWARE                     (1 << 28)
#define NGLI_FEATURE_UINT_UNIFORMS                (1 << 29)
#define NGLI_FEATURE_EGL_ANDROID_GET_IMAGE_NATIVE_CLIENT_BUFFER (1 << 30)
#define NGLI_FEATURE_MAP_BUFFER                   (1ull << 31)

#define NGLI_FEATURE_COMPUTE_SHADER_ALL (NGLI_FEATURE_COMPUTE_SHADER           | \
                                         NGLI_FEATURE_PROGRAM_INTERFACE_QUERY  | \
                                         NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE  | \
                                         NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)

#endif
