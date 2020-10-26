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
#include <sxplayer.h>

#include "colorconv.h"
#include "format.h"
#include "image.h"
#include "math_utils.h"
#include "utils.h"

struct color_info ngli_color_info_from_sxplayer_frame(const struct sxplayer_frame *frame)
{
    return (struct color_info){
        .space     = frame->color_space,
        .range     = frame->color_range,
        .primaries = frame->color_primaries,
        .transfer  = frame->color_trc,
    };
}

static const int nb_planes_map[] = {
    [NGLI_IMAGE_LAYOUT_DEFAULT]        = 1,
    [NGLI_IMAGE_LAYOUT_MEDIACODEC]     = 1,
    [NGLI_IMAGE_LAYOUT_NV12]           = 2,
    [NGLI_IMAGE_LAYOUT_NV12_RECTANGLE] = 2,
};

NGLI_STATIC_ASSERT(nb_planes_map, NGLI_ARRAY_NB(nb_planes_map) == NGLI_NB_IMAGE_LAYOUTS);

void ngli_image_init(struct image *s, const struct image_params *params, struct texture **planes)
{
    ngli_image_reset(s);
    ngli_assert(params->layout > NGLI_IMAGE_LAYOUT_NONE && params->layout < NGLI_NB_IMAGE_LAYOUTS);
    s->params = *params;
    s->nb_planes = nb_planes_map[params->layout];
    for (int i = 0; i < s->nb_planes; i++)
        s->planes[i] = planes[i];
    if (params->layout == NGLI_IMAGE_LAYOUT_NV12 ||
        params->layout == NGLI_IMAGE_LAYOUT_NV12_RECTANGLE) {
        ngli_colorconv_get_ycbcr_to_rgb_color_matrix(s->color_matrix, &params->color_info);
    }
}

void ngli_image_reset(struct image *s)
{
    memset(s, 0, sizeof(*s));
    ngli_mat4_identity(s->color_matrix);
    ngli_mat4_identity(s->coordinates_matrix);
}

uint64_t ngli_image_get_memory_size(const struct image *s)
{
    uint64_t size = 0;
    for (int i = 0; i < s->nb_planes; i++) {
        const struct texture *plane = s->planes[i];
        const struct texture_params *params = &plane->params;
        size += params->width
              * params->height
              * NGLI_MAX(params->depth, 1)
              * ngli_format_get_bytes_per_pixel(params->format);
    }
    return size;
}
