/*
 * Copyright 2021 GoPro Inc.
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

#ifndef DISTMAP_H
#define DISTMAP_H

struct ngl_ctx;
struct distmap;

struct distmap *ngli_distmap_create(struct ngl_ctx *ctx);

struct distmap_params {
    /*
     * Spread is arbitrary: it represents how far an effect such as glowing
     * could be applied, but it's also used for padding around the shape to be
     * that the extremities of the distance map are always black, and thus not
     * affect neighbor glyph, typically when relying on mipmapping.
     */
    int spread;
    int shape_w;
    int shape_h;

    /* coordinate space to interpret the polynomials */
    float poly_corner[2];
    float poly_width[2];
    float poly_height[2];
};

int ngli_distmap_init(struct distmap *d, const struct distmap_params *params);

/*
 * Add a degree 3 polynomial for a shape.
 *
 * Coordinates system is from [0,0] to [1,1], with origin in the bottom-left
 */
int ngli_distmap_add_poly3(struct distmap *d, int shape_id, const float *x, const float *y);

int ngli_distmap_generate_texture(struct distmap *d);
struct texture *ngli_distmap_get_texture(const struct distmap *d);

/*
 * Return the texture UV coordinates of a given shape in the grid
 */
void ngli_distmap_get_shape_coords(const struct distmap *d, int shape_id, float *dst);

void ngli_distmap_freep(struct distmap **dp);

#endif
