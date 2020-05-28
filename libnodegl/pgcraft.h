/*
 * Copyright 2020 GoPro Inc.
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

#ifndef PGCRAFT_H
#define PGCRAFT_H

#include "block.h"
#include "bstr.h"
#include "buffer.h"
#include "image.h"
#include "pipeline.h"
#include "texture.h"

struct ngl_ctx;

struct pgcraft_uniform { // also buffers (for arrays)
    char name[MAX_ID_LEN];
    int type;
    int stage;
    int count;
    int precision;
    const void *data;
};

enum pgcraft_shader_tex_type {
    NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE2D,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE2D,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE3D,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE,
    NGLI_PGCRAFT_SHADER_TEX_TYPE_NB
};

struct pgcraft_texture {
    char name[MAX_ID_LEN];
    enum pgcraft_shader_tex_type type;
    int stage;
    int precision;
    int writable;
    int format;
    struct texture *texture;
    struct image *image;
};

struct pgcraft_block {
    char name[MAX_ID_LEN];
    const char *instance_name;
    int stage;
    int variadic;
    const struct block *block;
    struct buffer *buffer;
};

struct pgcraft_attribute {
    char name[MAX_ID_LEN];
    int type;
    int precision;
    int format;
    int stride;
    int offset;
    int rate;
    struct buffer *buffer;
};

struct pgcraft_iovar {
    char name[MAX_ID_LEN];
    int type;
};

enum {
    NGLI_INFO_FIELD_SAMPLING_MODE,
    NGLI_INFO_FIELD_DEFAULT_SAMPLER,
    NGLI_INFO_FIELD_COORDINATE_MATRIX,
    NGLI_INFO_FIELD_COLOR_MATRIX,
    NGLI_INFO_FIELD_DIMENSIONS,
    NGLI_INFO_FIELD_TIMESTAMP,
    NGLI_INFO_FIELD_OES_SAMPLER,
    NGLI_INFO_FIELD_Y_SAMPLER,
    NGLI_INFO_FIELD_UV_SAMPLER,
    NGLI_INFO_FIELD_Y_RECT_SAMPLER,
    NGLI_INFO_FIELD_UV_RECT_SAMPLER,
    NGLI_INFO_FIELD_NB
};

struct pgcraft_texture_info_field {
    char name[MAX_ID_LEN];
    int type;
    int index;
    int stage;
};

struct pgcraft_texture_info {
    int stage;
    int precision;
    int writable;
    int format;
    struct texture *texture;
    struct image *image;
    struct pgcraft_texture_info_field fields[NGLI_INFO_FIELD_NB];
};

struct pgcraft_params {
    const char *vert_base;
    const char *frag_base;
    const char *comp_base;

    const struct pgcraft_uniform *uniforms;
    int nb_uniforms;
    const struct pgcraft_texture *textures;
    int nb_textures;
    const struct pgcraft_block *blocks;
    int nb_blocks;
    const struct pgcraft_attribute *attributes;
    int nb_attributes;

    const struct pgcraft_iovar *vert_out_vars;
    int nb_vert_out_vars;

    int nb_frag_output;
};

enum {
    NGLI_BINDING_TYPE_UBO,
    NGLI_BINDING_TYPE_SSBO,
    NGLI_BINDING_TYPE_TEXTURE,
    NGLI_BINDING_TYPE_NB
};

#define NB_BINDINGS (NGLI_PROGRAM_SHADER_NB * NGLI_BINDING_TYPE_NB)
#define BIND_ID(stage, type) ((stage) * NGLI_BINDING_TYPE_NB + (type))

struct pgcraft {
    struct darray texture_infos; // pgcraft_texture_info

    /* uniforms block */
    int use_ublock;
    struct block ublock[NGLI_PROGRAM_SHADER_NB];
    struct buffer *ubuffer[NGLI_PROGRAM_SHADER_NB];

    /* private */
    struct ngl_ctx *ctx;
    struct bstr *shaders[NGLI_PROGRAM_SHADER_NB];

    struct darray pipeline_uniforms;
    struct darray pipeline_textures;
    struct darray pipeline_buffers;
    struct darray pipeline_attributes;

    struct darray filtered_pipeline_uniforms;
    struct darray filtered_pipeline_textures;
    struct darray filtered_pipeline_buffers;
    struct darray filtered_pipeline_attributes;

    struct darray vert_out_vars; // pgcraft_iovar

    struct program *program;

    int bindings[NB_BINDINGS];
    int *next_bindings[NB_BINDINGS];
    int in_locations[NGLI_PROGRAM_SHADER_NB];
    int out_locations[NGLI_PROGRAM_SHADER_NB];

    /* GLSL info */
    int glsl_version;
    const char *glsl_version_suffix;
    const char *sym_vertex_index;
    const char *sym_instance_index;
    const char *rg; // 2-component texture picking (could be either rg or ra depending on the OpenGL version)
    int has_in_out_qualifiers;
    int has_in_out_layout_qualifiers;
    int has_precision_qualifiers;
    int has_modern_texture_picking;
    int *next_in_location;  // 1st stage only
    int *next_out_location; // last stage only
    const char * const *required_tex_exts;
};

struct pgcraft *ngli_pgcraft_create(struct ngl_ctx *ctx);

int ngli_pgcraft_craft(struct pgcraft *s,
                       struct pipeline_params *dst,
                       const struct pgcraft_params *params);

int ngli_pgcraft_get_uniform_index(const struct pgcraft *s, const char *name, int stage);

void ngli_pgcraft_freep(struct pgcraft **sp);

#endif
