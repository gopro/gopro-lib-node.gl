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

#ifndef TYPE_H
#define TYPE_H

enum {
    NGLI_TYPE_NONE,
    NGLI_TYPE_INT,
    NGLI_TYPE_IVEC2,
    NGLI_TYPE_IVEC3,
    NGLI_TYPE_IVEC4,
    NGLI_TYPE_UINT,
    NGLI_TYPE_UIVEC2,
    NGLI_TYPE_UIVEC3,
    NGLI_TYPE_UIVEC4,
    NGLI_TYPE_FLOAT,
    NGLI_TYPE_VEC2,
    NGLI_TYPE_VEC3,
    NGLI_TYPE_VEC4,
    NGLI_TYPE_MAT3,
    NGLI_TYPE_MAT4,
    NGLI_TYPE_BOOL,
    NGLI_TYPE_SAMPLER_2D,
    NGLI_TYPE_SAMPLER_2D_RECT,
    NGLI_TYPE_SAMPLER_3D,
    NGLI_TYPE_SAMPLER_CUBE,
    NGLI_TYPE_SAMPLER_EXTERNAL_OES,
    NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT,
    NGLI_TYPE_IMAGE_2D,
    NGLI_TYPE_UNIFORM_BUFFER,
    NGLI_TYPE_STORAGE_BUFFER,
    NGLI_TYPE_NB
};

#endif
