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

#ifndef IMAGE_H
#define IMAGE_H

#include <sxplayer.h>

#include "texture.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NGLI_COLOR_INFO_DEFAULTS {             \
    .space     = SXPLAYER_COL_SPC_UNSPECIFIED, \
    .range     = SXPLAYER_COL_RNG_UNSPECIFIED, \
    .primaries = SXPLAYER_COL_PRI_UNSPECIFIED, \
    .transfer  = SXPLAYER_COL_TRC_UNSPECIFIED, \
}                                              \

struct color_info {
    int space;
    int range;
    int primaries;
    int transfer;
};

struct color_info ngli_color_info_from_sxplayer_frame(const struct sxplayer_frame *frame);

enum image_layout {
    NGLI_IMAGE_LAYOUT_NONE           = 0,
    NGLI_IMAGE_LAYOUT_DEFAULT        = 1,
    NGLI_IMAGE_LAYOUT_MEDIACODEC     = 2,
    NGLI_IMAGE_LAYOUT_NV12           = 3,
    NGLI_IMAGE_LAYOUT_NV12_RECTANGLE = 4,
    NGLI_NB_IMAGE_LAYOUTS
};

struct image_params {
    int width;
    int height;
    int depth;
    enum image_layout layout;
    struct color_info color_info;
};

struct image {
    struct image_params params;
    struct texture *planes[4];
    int nb_planes;
    NGLI_ALIGNED_MAT(color_matrix);
    /* mutable fields after initialization */
    NGLI_ALIGNED_MAT(coordinates_matrix);
    double ts;
};

void ngli_image_init(struct image *s, const struct image_params *params, struct texture **planes);
void ngli_image_reset(struct image *s);
uint64_t ngli_image_get_memory_size(const struct image *s);

#ifdef __cplusplus
}
#endif

#endif
