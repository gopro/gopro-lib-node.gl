/*
 * Copyright 2016-2018 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include "block.h"
#include "buffer.h"
#include "glincludes.h"
#include "hmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "pass.h"
#include "pgcache.h"
#include "pgcraft.h"
#include "pipeline.h"
#include "program.h"
#include "texture.h"
#include "topology.h"
#include "type.h"
#include "utils.h"

struct pipeline_desc {
    struct pipeline pipeline;
    int modelview_matrix_index;
    int projection_matrix_index;
    int normal_matrix_index;
    struct darray texture_infos;
};

static int register_uniform(struct pass *s, const char *name, struct ngl_node *uniform)
{
    if (!uniform)
        return 0;

    struct hmap *infos = s->pipeline_program->uniforms;
    if (!ngli_hmap_get(infos, name)) {
        struct pass_params *params = &s->params;
        LOG(WARNING, "uniform %s attached to pipeline %s not found in shader", name, params->label);
        return 0;
    }

    struct pipeline_uniform pipeline_uniform = {{0}};
    snprintf(pipeline_uniform.name, sizeof(pipeline_uniform.name), "%s", name);

    if (uniform->class->category == NGLI_NODE_CATEGORY_BUFFER) {
        struct buffer_priv *buffer_priv = uniform->priv_data;
        pipeline_uniform.type  = buffer_priv->data_type;
        pipeline_uniform.count = buffer_priv->count;
        pipeline_uniform.data  = buffer_priv->data;
    } else if (uniform->class->category == NGLI_NODE_CATEGORY_UNIFORM) {
        struct variable_priv *variable_priv = uniform->priv_data;
        pipeline_uniform.type  = variable_priv->data_type;
        pipeline_uniform.count = 1;
        pipeline_uniform.data  = variable_priv->data;
    } else {
        ngli_assert(0);
    }

    if (!ngli_darray_push(&s->pipeline_uniforms, &pipeline_uniform))
        return NGL_ERROR_MEMORY;

    if (!ngli_darray_push(&s->uniform_nodes, &uniform))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_builtin_uniforms(struct pass *s)
{
    struct pipeline_uniform pipeline_uniforms[] = {
        {.name = "ngl_modelview_matrix",  .type = NGLI_TYPE_MAT4, .count = 1, .data = NULL},
        {.name = "ngl_projection_matrix", .type = NGLI_TYPE_MAT4, .count = 1, .data = NULL},
        {.name = "ngl_normal_matrix",     .type = NGLI_TYPE_MAT3, .count = 1, .data = NULL},
    };

    for (int i = 0; i < NGLI_ARRAY_NB(pipeline_uniforms); i++) {
        struct pipeline_uniform *pipeline_uniform = &pipeline_uniforms[i];
        struct hmap *infos = s->pipeline_program->uniforms;
        if (!ngli_hmap_get(infos, pipeline_uniform->name))
            continue;
        if (!ngli_darray_push(&s->pipeline_uniforms, pipeline_uniform))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

struct texture_info_field {
    int active;
    int index;
    int type;
    int is_sampler_or_image;
};

struct texture_info {
    const char *name;
    struct image *image;
    struct texture_info_field fields[NGLI_INFO_FIELD_NB];
};

static const struct texture_info_map {
    const char *suffix;
    const int *allowed_types;
    int field_id;
} texture_info_maps[] = {
    {"_sampling_mode",    (const int[]){NGLI_TYPE_INT, 0},                         NGLI_INFO_FIELD_SAMPLING_MODE},
    {"_sampler",          (const int[]){NGLI_TYPE_SAMPLER_2D,
                                        NGLI_TYPE_SAMPLER_3D,
                                        NGLI_TYPE_SAMPLER_CUBE,
                                        NGLI_TYPE_IMAGE_2D, 0},                    NGLI_INFO_FIELD_DEFAULT_SAMPLER},
    {"_coord_matrix",     (const int[]){NGLI_TYPE_MAT4, 0},                        NGLI_INFO_FIELD_COORDINATE_MATRIX},
    {"_color_matrix",     (const int[]){NGLI_TYPE_MAT4, 0},                        NGLI_INFO_FIELD_COLOR_MATRIX},
    {"_dimensions",       (const int[]){NGLI_TYPE_VEC2,
                                        NGLI_TYPE_VEC3, 0},                        NGLI_INFO_FIELD_DIMENSIONS},
    {"_ts",               (const int[]){NGLI_TYPE_FLOAT, 0},                       NGLI_INFO_FIELD_TIMESTAMP},
    {"_external_sampler", (const int[]){NGLI_TYPE_SAMPLER_EXTERNAL_OES,
                                        NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT, 0}, NGLI_INFO_FIELD_OES_SAMPLER},
    {"_y_sampler",        (const int[]){NGLI_TYPE_SAMPLER_2D, 0},                  NGLI_INFO_FIELD_Y_SAMPLER},
    {"_uv_sampler",       (const int[]){NGLI_TYPE_SAMPLER_2D, 0},                  NGLI_INFO_FIELD_UV_SAMPLER},
    {"_y_rect_sampler",   (const int[]){NGLI_TYPE_SAMPLER_2D_RECT, 0},             NGLI_INFO_FIELD_Y_RECT_SAMPLER},
    {"_uv_rect_sampler",  (const int[]){NGLI_TYPE_SAMPLER_2D_RECT, 0},             NGLI_INFO_FIELD_UV_RECT_SAMPLER},
};

static int is_allowed_type(const int *allowed_types, int type)
{
    for (int i = 0; allowed_types[i]; i++)
        if (allowed_types[i] == type)
            return 1;
    return 0;
}

static int is_sampler_or_image(int type)
{
    switch (type) {
    case NGLI_TYPE_SAMPLER_2D:
    case NGLI_TYPE_SAMPLER_2D_RECT:
    case NGLI_TYPE_SAMPLER_3D:
    case NGLI_TYPE_SAMPLER_CUBE:
    case NGLI_TYPE_SAMPLER_EXTERNAL_OES:
    case NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT:
    case NGLI_TYPE_IMAGE_2D:
        return 1;
    default:
        return 0;
    }
}

static int register_texture(struct pass *s, const char *name, struct ngl_node *texture)
{
    if (!texture)
        return 0;

    struct texture_priv *texture_priv = texture->priv_data;
    struct image *image = &texture_priv->image;

    struct texture_info info = {
        .name  = name,
        .image = image,
    };

    struct hmap *infos = s->pipeline_program->uniforms;
    for (int i = 0; i < NGLI_ARRAY_NB(texture_info_maps); i++) {
        const struct texture_info_map *map = &texture_info_maps[i];

        struct texture_info_field *field = &info.fields[map->field_id];

        char uniform_name[MAX_ID_LEN];
        snprintf(uniform_name, sizeof(uniform_name), "%s%s", name, map->suffix);

        const struct program_variable_info *uniform = ngli_hmap_get(infos, uniform_name);
        if (!uniform)
            continue;

        if (!is_allowed_type(map->allowed_types, uniform->type)) {
            LOG(ERROR, "invalid type 0x%x found for texture uniform %s",
                uniform->type, uniform_name);
            return NGL_ERROR_INVALID_ARG;
        }
        field->active = 1;
        field->type = uniform->type;
        field->is_sampler_or_image = is_sampler_or_image(uniform->type);

        if (field->is_sampler_or_image) {
            struct pipeline_texture pipeline_texture = {
                .name     = {0},
                .type     = uniform->type,
                .location = uniform->location,
                .binding  = uniform->binding,
            };
            snprintf(pipeline_texture.name, sizeof(pipeline_texture.name), "%s", uniform_name);
            if (!ngli_darray_push(&s->pipeline_textures, &pipeline_texture))
                return NGL_ERROR_MEMORY;
        } else {
            struct pipeline_uniform pipeline_uniform = {
                .type  = uniform->type,
                .count = 1
            };
            snprintf(pipeline_uniform.name, sizeof(pipeline_uniform.name), "%s", uniform_name);
            if (!ngli_darray_push(&s->pipeline_uniforms, &pipeline_uniform))
                return NGL_ERROR_MEMORY;
        }
    }

    const struct texture_info_field *fields = info.fields;
    if (fields[NGLI_INFO_FIELD_COLOR_MATRIX].active &&
        fields[NGLI_INFO_FIELD_OES_SAMPLER].active &&
        fields[NGLI_INFO_FIELD_OES_SAMPLER].type == NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT) {
        LOG(WARNING, "color_matrix is not supported with 2DY2YEXT sampler");
    }

    uint32_t supported_image_layouts = 0;

    if (fields[NGLI_INFO_FIELD_DEFAULT_SAMPLER].active)
        supported_image_layouts |= 1 << NGLI_IMAGE_LAYOUT_DEFAULT;

    if (fields[NGLI_INFO_FIELD_OES_SAMPLER].active)
        supported_image_layouts |= 1 << NGLI_IMAGE_LAYOUT_MEDIACODEC;

    if (fields[NGLI_INFO_FIELD_Y_SAMPLER].active ||
        fields[NGLI_INFO_FIELD_UV_SAMPLER].active)
        supported_image_layouts |= 1 << NGLI_IMAGE_LAYOUT_NV12;

    if (fields[NGLI_INFO_FIELD_Y_RECT_SAMPLER].active ||
        fields[NGLI_INFO_FIELD_UV_RECT_SAMPLER].active)
        supported_image_layouts |= 1 << NGLI_IMAGE_LAYOUT_NV12_RECTANGLE;

    if (!supported_image_layouts)
        LOG(WARNING, "no sampler found for texture %s", name);

    texture_priv->supported_image_layouts &= supported_image_layouts;

    if (!ngli_darray_push(&s->texture_infos, &info))
        return NGL_ERROR_MEMORY;

    if (!ngli_darray_push(&s->texture_nodes, &texture))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_block(struct pass *s, const char *name, struct ngl_node *block)
{
    if (!block)
        return 0;

    const struct hmap *infos = s->pipeline_program->buffer_blocks;
    const struct program_variable_info *info = ngli_hmap_get(infos, name);
    if (!info) {
        struct pass_params *params = &s->params;
        LOG(WARNING, "block %s attached to pipeline %s not found in shader", name, params->label);
        return 0;
    }

    struct block_priv *block_priv = block->priv_data;
    struct buffer *buffer = &block_priv->buffer;
    struct pipeline_buffer pipeline_buffer = {
        .type = info->type,
        .binding = info->binding,
        .buffer = buffer,
    };
    snprintf(pipeline_buffer.name, sizeof(pipeline_buffer.name), "%s", name);

    if (!ngli_darray_push(&s->pipeline_buffers, &pipeline_buffer))
        return NGL_ERROR_MEMORY;

    int ret = ngli_node_block_ref(block);
    if (ret < 0)
        return ret;

    if (!ngli_darray_push(&s->block_nodes, &block)) {
        ngli_node_block_unref(block);
        return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int check_attributes(struct pass *s, struct hmap *attributes, int per_instance)
{
    if (!attributes)
        return 0;

    const struct geometry_priv *geometry = s->params.geometry->priv_data;
    const int64_t max_indices = geometry->max_indices;

    const struct buffer_priv *vertices = geometry->vertices_buffer->priv_data;
    const int nb_vertices = vertices->count;

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(attributes, entry))) {
        const struct ngl_node *anode = entry->data;
        const struct buffer_priv *buffer = anode->priv_data;

        if (per_instance) {
            if (buffer->count != s->params.nb_instances) {
                LOG(ERROR, "attribute buffer %s count (%d) does not match instance count (%d)",
                    entry->key, buffer->count, s->params.nb_instances);
                return NGL_ERROR_INVALID_ARG;
            }
        } else {
            if (geometry->indices_buffer) {
                if (max_indices >= buffer->count) {
                    LOG(ERROR, "indices buffer contains values exceeding attribute buffer %s count (%" PRId64 " >= %d)",
                        entry->key, max_indices, buffer->count);
                    return NGL_ERROR_INVALID_ARG;
                }
            } else {
                if (buffer->count != nb_vertices) {
                    LOG(ERROR, "attribute buffer %s count (%d) does not match vertices count (%d)",
                        entry->key, buffer->count, nb_vertices);
                    return NGL_ERROR_INVALID_ARG;
                }
            }
        }
    }
    return 0;
}

static int register_attribute(struct pass *s, const char *name, struct ngl_node *attribute, int rate, int warn)
{
    if (!attribute)
        return 0;

    const struct hmap *infos = s->pipeline_program->attributes;
    const struct program_variable_info *info = ngli_hmap_get(infos, name);
    if (!info) {
        if (warn) {
            const struct pass_params *params = &s->params;
            LOG(WARNING, "attribute %s attached to pipeline %s not found in shader", name, params->label);
        }
        return 0;
    }

    int ret = ngli_node_buffer_ref(attribute);
    if (ret < 0)
        return ret;

    if (!ngli_darray_push(&s->attribute_nodes, &attribute)) {
        ngli_node_buffer_unref(attribute);
        return NGL_ERROR_MEMORY;
    }

    struct buffer_priv *attribute_priv = attribute->priv_data;
    const int format = attribute_priv->data_format;
    int stride = attribute_priv->data_stride;
    int offset = 0;
    int class_id = attribute->class->id;
    struct buffer *buffer = &attribute_priv->buffer;

    if (attribute_priv->block) {
        struct block_priv *block_priv = attribute_priv->block->priv_data;
        const struct ngl_node *f = block_priv->fields[attribute_priv->block_field];
        const struct block *block = &block_priv->block;
        const struct block_field *fields = ngli_darray_data(&block->fields);
        const struct block_field *fi = &fields[attribute_priv->block_field];
        stride = fi->stride;
        offset = fi->offset;
        buffer = &block_priv->buffer;
        class_id = f->class->id;
    }

    if (info->type == NGLI_TYPE_MAT4 && class_id != NGL_NODE_BUFFERMAT4) {
        LOG(ERROR, "attribute '%s' type does not match the type declared in the shader", name);
        return NGL_ERROR_INVALID_ARG;
    }

    const int attribute_count = info->type == NGLI_TYPE_MAT4 ? 4 : 1;
    const int attribute_offset = ngli_format_get_bytes_per_pixel(format);
    for (int i = 0; i < attribute_count; i++) {
        struct pipeline_attribute pipeline_attribute = {
            .location = info->location + i,
            .format = format,
            .stride = stride,
            .offset = offset + i * attribute_offset,
            .rate   = rate,
            .buffer = buffer,
        };
        snprintf(pipeline_attribute.name, sizeof(pipeline_attribute.name), "%s", name);

        if (!ngli_darray_push(&s->pipeline_attributes, &pipeline_attribute))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int register_resource(struct pass *s, const char *name, struct ngl_node *node)
{
    switch (node->class->category) {
    case NGLI_NODE_CATEGORY_UNIFORM:
    case NGLI_NODE_CATEGORY_BUFFER:  return register_uniform(s, name, node);
    case NGLI_NODE_CATEGORY_TEXTURE: return register_texture(s, name, node);
    case NGLI_NODE_CATEGORY_BLOCK:   return register_block(s, name, node);
    default:
        ngli_assert(0);
    }
}

static int register_resources(struct pass *s, const struct hmap *resources)
{
    if (!resources)
        return 0;

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(resources, entry))) {
        int ret = register_resource(s, entry->key, entry->data);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int pass_graphics_init(struct pass *s)
{
    const struct pass_params *params = &s->params;

    s->pipeline_type = NGLI_PIPELINE_TYPE_GRAPHICS;

    struct geometry_priv *geometry_priv = params->geometry->priv_data;
    struct pipeline_graphics *graphics = &s->pipeline_graphics;

    graphics->topology = geometry_priv->topology;
    graphics->nb_instances = s->params.nb_instances;

    if (geometry_priv->indices_buffer) {
        struct ngl_node *indices = geometry_priv->indices_buffer;
        struct buffer_priv *indices_priv = indices->priv_data;
        if (indices_priv->block) {
            LOG(ERROR, "geometry indices buffers referencing a block are not supported");
            return NGL_ERROR_UNSUPPORTED;
        }

        int ret = ngli_node_buffer_ref(indices);
        if (ret < 0)
            return ret;

        s->indices = indices;
        s->indices_buffer = &indices_priv->buffer;

        graphics->nb_indices = indices_priv->count;
        graphics->indices_format = indices_priv->data_format;
        graphics->indices = &indices_priv->buffer;
    } else {
        struct ngl_node *vertices = geometry_priv->vertices_buffer;
        struct buffer_priv *buffer_priv = vertices->priv_data;
        graphics->nb_vertices = buffer_priv->count;
    }

    int ret;

    if ((ret = register_resources(s, params->vert_resources)) < 0 ||
        (ret = register_resources(s, params->frag_resources)) < 0)
        return ret;

    if ((ret = check_attributes(s, params->attributes, 0)) < 0 ||
        (ret = check_attributes(s, params->instance_attributes, 1)) < 0)
        return ret;

    if ((ret = register_attribute(s, "ngl_position", geometry_priv->vertices_buffer, 0, 0)) < 0 ||
        (ret = register_attribute(s, "ngl_uvcoord",  geometry_priv->uvcoords_buffer, 0, 0)) < 0 ||
        (ret = register_attribute(s, "ngl_normal",   geometry_priv->normals_buffer,  0, 0)) < 0)
        return ret;

    if (params->attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->attributes, entry))) {
            int ret = register_attribute(s, entry->key, entry->data, 0, 1);
            if (ret < 0)
                return ret;
        }
    }

    if (params->instance_attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->instance_attributes, entry))) {
            int ret = register_attribute(s, entry->key, entry->data, 1, 1);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int pass_compute_init(struct pass *s)
{
    const struct pass_params *params = &s->params;

    int ret = register_resources(s, params->compute_resources);
    if (ret < 0)
        return ret;

    s->pipeline_type = NGLI_PIPELINE_TYPE_COMPUTE;
    s->pipeline_compute.nb_group_x = params->nb_group_x;
    s->pipeline_compute.nb_group_y = params->nb_group_y;
    s->pipeline_compute.nb_group_z = params->nb_group_z;

    return 0;
}

int ngli_pass_prepare(struct pass *s)
{
    struct ngl_ctx *ctx = s->ctx;

    struct pipeline_graphics pipeline_graphics = s->pipeline_graphics;
    pipeline_graphics.state = ctx->graphicstate;
    pipeline_graphics.rt_desc = *ctx->rendertarget_desc;

    struct pipeline_params pipeline_params = {
        .type          = s->pipeline_type,
        .graphics      = pipeline_graphics,
        .compute       = s->pipeline_compute,
        .program       = s->pipeline_program,
        .attributes    = ngli_darray_data(&s->pipeline_attributes),
        .nb_attributes = ngli_darray_count(&s->pipeline_attributes),
        .uniforms      = ngli_darray_data(&s->pipeline_uniforms),
        .nb_uniforms   = ngli_darray_count(&s->pipeline_uniforms),
        .textures      = ngli_darray_data(&s->pipeline_textures),
        .nb_textures   = ngli_darray_count(&s->pipeline_textures),
        .buffers       = ngli_darray_data(&s->pipeline_buffers),
        .nb_buffers    = ngli_darray_count(&s->pipeline_buffers),
    };

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    ctx->rnode_pos->id = ngli_darray_count(&s->pipeline_descs) - 1;

    struct pipeline *pipeline = &desc->pipeline;
    int ret = ngli_pipeline_init(pipeline, ctx, &pipeline_params);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pipeline_get_uniform_index(pipeline, "ngl_modelview_matrix");
    desc->projection_matrix_index = ngli_pipeline_get_uniform_index(pipeline, "ngl_projection_matrix");
    desc->normal_matrix_index = ngli_pipeline_get_uniform_index(pipeline, "ngl_normal_matrix");

    ngli_darray_init(&desc->texture_infos, sizeof(struct texture_info), 0);

    struct texture_info *texture_infos = ngli_darray_data(&s->texture_infos);
    for (int i = 0; i < ngli_darray_count(&s->texture_infos); i++) {
        struct texture_info *info = ngli_darray_push(&desc->texture_infos, &texture_infos[i]);
        if (!info)
            return NGL_ERROR_MEMORY;
        for (int j = 0; j < NGLI_ARRAY_NB(texture_info_maps); j++) {
            const struct texture_info_map *map = &texture_info_maps[j];

            struct texture_info_field *field = &info->fields[map->field_id];
            if (!field->active) {
                field->index = -1;
                continue;
            }

            char name[MAX_ID_LEN];
            snprintf(name, sizeof(name), "%s%s", info->name, map->suffix);

            if (field->is_sampler_or_image)
                field->index = ngli_pipeline_get_texture_index(pipeline, name);
            else
                field->index = ngli_pipeline_get_uniform_index(pipeline, name);
        }
    }

    return 0;
}

int ngli_pass_init(struct pass *s, struct ngl_ctx *ctx, const struct pass_params *params)
{
    s->ctx = ctx;
    s->params = *params;

    ngli_darray_init(&s->attribute_nodes, sizeof(struct ngl_node *), 0);
    ngli_darray_init(&s->texture_nodes, sizeof(struct ngl_node *), 0);
    ngli_darray_init(&s->uniform_nodes, sizeof(struct ngl_node *), 0);
    ngli_darray_init(&s->block_nodes, sizeof(struct ngl_node *), 0);

    ngli_darray_init(&s->texture_infos, sizeof(struct texture_info), 0);

    ngli_darray_init(&s->pipeline_attributes, sizeof(struct pipeline_attribute), 0);
    ngli_darray_init(&s->pipeline_textures, sizeof(struct pipeline_texture), 0);
    ngli_darray_init(&s->pipeline_uniforms, sizeof(struct pipeline_uniform), 0);
    ngli_darray_init(&s->pipeline_buffers, sizeof(struct pipeline_buffer), 0);

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    struct program_priv *program_priv = params->program->priv_data;
    s->pipeline_program = &program_priv->program;

    int ret = register_builtin_uniforms(s);
    if (ret < 0)
        return ret;

    ret = params->geometry ? pass_graphics_init(s)
                           : pass_compute_init(s);
    if (ret < 0)
        return ret;

    return 0;
}

#define NODE_TYPE_DEFAULT 0
#define NODE_TYPE_BLOCK   1
#define NODE_TYPE_BUFFER  2

static void reset_nodes(struct darray *p, int type)
{
    struct ngl_node **nodes = ngli_darray_data(p);
    for (int i = 0; i < ngli_darray_count(p); i++) {
        if (type == NODE_TYPE_BLOCK)
            ngli_node_block_unref(nodes[i]);
        else if (type == NODE_TYPE_BUFFER)
            ngli_node_buffer_unref(nodes[i]);
    }
    ngli_darray_reset(p);
}

#define DECLARE_RESET_NODES_FUNC(name, type)       \
static void reset_##name##_nodes(struct darray *p) \
{                                                  \
    reset_nodes(p, type);                          \
}                                                  \

DECLARE_RESET_NODES_FUNC(block, NODE_TYPE_BLOCK)
DECLARE_RESET_NODES_FUNC(buffer, NODE_TYPE_BUFFER)

void ngli_pass_uninit(struct pass *s)
{
    if (!s->ctx)
        return;

    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const int nb_descs = ngli_darray_count(&s->pipeline_descs);
    for (int i = 0; i < nb_descs; i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_reset(&desc->pipeline);
        ngli_darray_reset(&desc->texture_infos);
    }
    ngli_darray_reset(&s->pipeline_descs);

    if (s->indices)
        ngli_node_buffer_unref(s->indices);

    ngli_darray_reset(&s->uniform_nodes);
    ngli_darray_reset(&s->texture_nodes);
    reset_block_nodes(&s->block_nodes);
    reset_buffer_nodes(&s->attribute_nodes);

    ngli_darray_reset(&s->texture_infos);

    ngli_darray_reset(&s->pipeline_attributes);
    ngli_darray_reset(&s->pipeline_textures);
    ngli_darray_reset(&s->pipeline_uniforms);
    ngli_darray_reset(&s->pipeline_buffers);

    ngli_pgcache_release_program(&s->default_program);

    memset(s, 0, sizeof(*s));
}

static int update_nodes(struct darray *p, double t, int type)
{
    struct ngl_node **nodes = ngli_darray_data(p);
    for (int i = 0; i < ngli_darray_count(p); i++) {
        struct ngl_node *node = nodes[i];
        int ret = ngli_node_update(node, t);
        if (ret < 0)
            return ret;
        if (type == NODE_TYPE_BLOCK) {
            ret = ngli_node_block_upload(node);
            if (ret < 0)
                return ret;
        } else if (type == NODE_TYPE_BUFFER) {
            ret = ngli_node_buffer_upload(node);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

#define DECLARE_UPDATE_NODES_FUNC(name, type)                \
static int update_##name##_nodes(struct darray *p, double t) \
{                                                            \
    return update_nodes(p, t, type);                         \
}

DECLARE_UPDATE_NODES_FUNC(common, NODE_TYPE_DEFAULT)
DECLARE_UPDATE_NODES_FUNC(block, NODE_TYPE_BLOCK)
DECLARE_UPDATE_NODES_FUNC(buffer, NODE_TYPE_BUFFER)

int ngli_pass_update(struct pass *s, double t)
{
    int ret;
    if ((ret = update_common_nodes(&s->uniform_nodes, t)) < 0 ||
        (ret = update_common_nodes(&s->texture_nodes, t)) < 0 ||
        (ret = update_block_nodes(&s->block_nodes, t)) < 0 ||
        (ret = update_buffer_nodes(&s->attribute_nodes, t)))
        return ret;

    return 0;
}

int ngli_pass_exec(struct pass *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline *pipeline = &desc->pipeline;

    const float *modelview_matrix = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_update_uniform(pipeline, desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_update_uniform(pipeline, desc->projection_matrix_index, projection_matrix);

    if (desc->normal_matrix_index >= 0) {
        float normal_matrix[3*3];
        ngli_mat3_from_mat4(normal_matrix, modelview_matrix);
        ngli_mat3_inverse(normal_matrix, normal_matrix);
        ngli_mat3_transpose(normal_matrix, normal_matrix);
        ngli_pipeline_update_uniform(pipeline, desc->normal_matrix_index, normal_matrix);
    }

    struct texture_info *texture_infos = ngli_darray_data(&desc->texture_infos);
    for (int i = 0; i < ngli_darray_count(&s->texture_infos); i++) {
        struct texture_info *info = &texture_infos[i];
        const struct texture_info_field *fields = info->fields;
        struct image *image = info->image;
        const float ts = image->ts;

        ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
        ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);
        ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_TIMESTAMP].index, &ts);

        if (image->params.layout) {
            const float dimensions[] = {image->params.width, image->params.height, image->params.depth};
            ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_DIMENSIONS].index, dimensions);
        }

        int ret = -1;
        switch (image->params.layout) {
        case NGLI_IMAGE_LAYOUT_DEFAULT:
            ret = ngli_pipeline_update_texture(pipeline, fields[NGLI_INFO_FIELD_DEFAULT_SAMPLER].index, image->planes[0]);
            break;
        case NGLI_IMAGE_LAYOUT_NV12:
            ret = ngli_pipeline_update_texture(pipeline, fields[NGLI_INFO_FIELD_Y_SAMPLER].index, image->planes[0]);
            ret &= ngli_pipeline_update_texture(pipeline, fields[NGLI_INFO_FIELD_UV_SAMPLER].index, image->planes[1]);
            break;
        case NGLI_IMAGE_LAYOUT_NV12_RECTANGLE:
            ret = ngli_pipeline_update_texture(pipeline, fields[NGLI_INFO_FIELD_Y_RECT_SAMPLER].index, image->planes[0]);
            ret &= ngli_pipeline_update_texture(pipeline, fields[NGLI_INFO_FIELD_UV_RECT_SAMPLER].index, image->planes[1]);
            break;
        case NGLI_IMAGE_LAYOUT_MEDIACODEC:
            ret = ngli_pipeline_update_texture(pipeline, fields[NGLI_INFO_FIELD_OES_SAMPLER].index, image->planes[0]);
            break;
        default:
            break;
        }
        const int layout = ret < 0 ? NGLI_IMAGE_LAYOUT_NONE : image->params.layout;
        ngli_pipeline_update_uniform(pipeline, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &layout);
    }

    ngli_pipeline_exec(pipeline);

    return 0;
}
