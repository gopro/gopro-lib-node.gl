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

#ifndef PIPELINE_H
#define PIPELINE_H

#include "block.h"
#include "buffer.h"
#include "darray.h"
#include "graphicstate.h"
#include "program.h"
#include "rendertarget.h"
#include "texture.h"

struct gctx;

enum {
    NGLI_ACCESS_UNDEFINED,
    NGLI_ACCESS_READ_BIT,
    NGLI_ACCESS_WRITE_BIT,
    NGLI_ACCESS_READ_WRITE,
    NGLI_ACCESS_NB
};

NGLI_STATIC_ASSERT(texture_access, (NGLI_ACCESS_READ_BIT | NGLI_ACCESS_WRITE_BIT) == NGLI_ACCESS_READ_WRITE);

struct pipeline_uniform_desc {
    char name[MAX_ID_LEN];
    int type;
    int count;
};

struct pipeline_texture_desc {
    char name[MAX_ID_LEN];
    int type;
    int location;
    int binding;
    int access;
    int stage;
};

struct pipeline_buffer_desc {
    char name[MAX_ID_LEN];
    int type;
    int binding;
    int access;
    int stage;
};

struct pipeline_attribute_desc {
    char name[MAX_ID_LEN];
    int location;
    int format;
    int stride;
    int offset;
    int rate;
};

struct pipeline_graphics {
    int topology;
    struct graphicstate state;
    struct rendertarget_desc rt_desc;
};

enum {
    NGLI_PIPELINE_TYPE_GRAPHICS,
    NGLI_PIPELINE_TYPE_COMPUTE,
};

struct pipeline_params {
    int type;
    const struct pipeline_graphics graphics;
    const struct program *program;

    const struct pipeline_texture_desc *textures_desc;
    int nb_textures;
    const struct pipeline_uniform_desc *uniforms_desc;
    int nb_uniforms;
    const struct pipeline_buffer_desc *buffers_desc;
    int nb_buffers;
    const struct pipeline_attribute_desc *attributes_desc;
    int nb_attributes;
};

struct pipeline_resource_params {
    struct texture **textures;
    int nb_textures;
    void **uniforms;
    int nb_uniforms;
    struct buffer **buffers;
    int nb_buffers;
    struct buffer **attributes;
    int nb_attributes;

    /*
     * Uniform block info per stage; required if the pipeline doesn't support
     * uniforms.
     */
    const struct block *ublock[NGLI_PROGRAM_SHADER_NB];
    struct buffer *ubuffer[NGLI_PROGRAM_SHADER_NB];
};

struct pipeline {
    struct gctx *gctx;

    int type;
    struct pipeline_graphics graphics;
    const struct program *program;
};

struct pipeline *ngli_pipeline_create(struct gctx *gctx);
int ngli_pipeline_init(struct pipeline *s, const struct pipeline_params *params);
int ngli_pipeline_set_resources(struct pipeline *s, const struct pipeline_resource_params *data_params);
int ngli_pipeline_update_attribute(struct pipeline *s, int index, struct buffer *buffer);
int ngli_pipeline_update_uniform(struct pipeline *s, int index, const void *value);
int ngli_pipeline_update_texture(struct pipeline *s, int index, struct texture *texture);
int ngli_pipeline_update_buffer(struct pipeline *s, int index, struct buffer *buffer);
void ngli_pipeline_draw(struct pipeline *s, int nb_vertices, int nb_instances);
void ngli_pipeline_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances);
void ngli_pipeline_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z);

void ngli_pipeline_freep(struct pipeline **sp);

#endif
