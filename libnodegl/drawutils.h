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

#ifndef DRAWUTILS_H
#define DRAWUTILS_H

#include <stdint.h>

#define NGLI_COLOR_VEC4_TO_U32(vec) (((uint32_t)((vec)[0] * 255) & 0xff) << 24 | \
                                     ((uint32_t)((vec)[1] * 255) & 0xff) << 16 | \
                                     ((uint32_t)((vec)[2] * 255) & 0xff) <<  8 | \
                                     ((uint32_t)((vec)[3] * 255) & 0xff))

struct canvas {
    uint8_t *buf;
    int w, h;
};

struct rect {
    int x, y, w, h;
};

#define NGLI_FONT_H 8
#define NGLI_FONT_W 7

void ngli_drawutils_draw_rect(struct canvas *canvas, const struct rect *rect, uint32_t color);
void ngli_drawutils_print(struct canvas *canvas, int x, int y, const char *str, uint32_t color);
int ngli_drawutils_get_font_atlas(struct canvas *c_dst);
void ngli_drawutils_get_atlas_uvcoords(uint8_t chr, float *dst);

#endif
