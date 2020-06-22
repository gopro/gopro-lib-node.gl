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

struct pipeline_uniform {
    char name[MAX_ID_LEN];
    int type;
    int count;
    const void *data;
};

struct pipeline_texture {
    char name[MAX_ID_LEN];
    int type;
    int location;
    int binding;
    int stage;
    struct texture *texture;
};

struct pipeline_buffer {
    char name[MAX_ID_LEN];
    int type;
    int binding;
    int stage;
    struct buffer *buffer;
};

struct pipeline_attribute {
    char name[MAX_ID_LEN];
    int location;
    int format;
    int stride;
    int offset;
    int rate;
    struct buffer *buffer;
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

    const struct pipeline_texture *textures;
    int nb_textures;
    const struct pipeline_uniform *uniforms;
    int nb_uniforms;
    const struct pipeline_buffer *buffers;
    int nb_buffers;
    const struct pipeline_attribute *attributes;
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

    struct darray uniform_descs;
    struct darray texture_descs;
    struct darray buffer_descs;
    struct darray attribute_descs;
    int nb_unbound_attributes;

    const struct block *ublock[NGLI_PROGRAM_SHADER_NB];
    struct buffer *ubuffer[NGLI_PROGRAM_SHADER_NB];
    uint8_t *udata[NGLI_PROGRAM_SHADER_NB];
};

struct pipeline *ngli_pipeline_create(struct gctx *gctx);
int ngli_pipeline_init(struct pipeline *s, const struct pipeline_params *params);
int ngli_pipeline_update_attribute(struct pipeline *s, int index, struct buffer *buffer);
int ngli_pipeline_update_uniform(struct pipeline *s, int index, const void *value);
int ngli_pipeline_update_texture(struct pipeline *s, int index, struct texture *texture);
void ngli_pipeline_draw(struct pipeline *s, int nb_vertices, int nb_instances);
void ngli_pipeline_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances);
void ngli_pipeline_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z);

void ngli_pipeline_freep(struct pipeline **sp);

#endif
