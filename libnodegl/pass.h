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

#ifndef PASS_H
#define PASS_H

#include <stdint.h>
#include "darray.h"
#include "pipeline.h"
#include "program.h"

struct ngl_ctx;

struct pass_params {
    const char *label;
    struct ngl_node *program;
    struct hmap *textures;
    struct hmap *uniforms;
    struct hmap *blocks;

    /* graphics */
    struct ngl_node *geometry;
    int nb_instances;
    struct hmap *attributes;
    struct hmap *instance_attributes;

    /* compute */
    int nb_group_x;
    int nb_group_y;
    int nb_group_z;
};

enum {
    NGLI_PASS_TYPE_GRAPHIC,
    NGLI_PASS_TYPE_COMPUTE,
};

struct pass {
    struct ngl_ctx *ctx;
    struct pass_params params;

    struct program default_program;

    struct darray attribute_nodes;
    struct darray texture_nodes;
    struct darray uniform_nodes;
    struct darray block_nodes;

    struct darray texture_infos;

    struct ngl_node *indices;
    struct buffer *indices_buffer;

    int pipeline_type;
    struct program *pipeline_program;
    struct pipeline_graphics pipeline_graphics;
    struct pipeline_compute pipeline_compute;
    struct darray pipeline_attributes;
    struct darray pipeline_uniforms;
    struct darray pipeline_textures;
    struct darray pipeline_buffers;
    struct darray pipeline_descs;
};

int ngli_pass_init(struct pass *s, struct ngl_ctx *ctx, const struct pass_params *params);
int ngli_pass_prepare(struct pass *s);
void ngli_pass_uninit(struct pass *s);
int ngli_pass_update(struct pass *s, double t);
int ngli_pass_exec(struct pass *s);

#endif
