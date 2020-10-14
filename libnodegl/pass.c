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
#include "gctx.h"
#include "hmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "pass.h"
#include "pgcraft.h"
#include "pipeline.h"
#include "program.h"
#include "texture.h"
#include "topology.h"
#include "type.h"
#include "utils.h"

struct pipeline_desc {
    struct pgcraft *crafter;
    struct pipeline *pipeline;
    int modelview_matrix_index;
    int projection_matrix_index;
    int normal_matrix_index;
};

static int register_uniform(struct pass *s, const char *name, struct ngl_node *uniform, int stage)
{
    if (!ngli_darray_push(&s->uniform_nodes, &uniform))
        return NGL_ERROR_MEMORY;

    struct pgcraft_uniform crafter_uniform = {.stage = stage};
    snprintf(crafter_uniform.name, sizeof(crafter_uniform.name), "%s", name);

    if (uniform->class->category == NGLI_NODE_CATEGORY_BUFFER) {
        struct buffer_priv *buffer_priv = uniform->priv_data;
        crafter_uniform.type  = buffer_priv->data_type;
        crafter_uniform.count = buffer_priv->count;
        crafter_uniform.data  = buffer_priv->data;
    } else if (uniform->class->category == NGLI_NODE_CATEGORY_UNIFORM) {
        struct variable_priv *variable_priv = uniform->priv_data;
        crafter_uniform.type  = variable_priv->data_type;
        crafter_uniform.data  = variable_priv->data;
    } else {
        ngli_assert(0);
    }

    const struct pass_params *params = &s->params;
    if (params->properties) {
        struct ngl_node *resprops_node = ngli_hmap_get(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_priv *resprops = resprops_node->priv_data;
            crafter_uniform.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_uniforms, &crafter_uniform))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_builtin_uniforms(struct pass *s)
{
    struct pgcraft_uniform crafter_uniforms[] = {
        {.name = "ngl_modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage=NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "ngl_projection_matrix", .type = NGLI_TYPE_MAT4, .stage=NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "ngl_normal_matrix",     .type = NGLI_TYPE_MAT3, .stage=NGLI_PROGRAM_SHADER_VERT, .data = NULL},
    };

    for (int i = 0; i < NGLI_ARRAY_NB(crafter_uniforms); i++) {
        struct pgcraft_uniform *crafter_uniform = &crafter_uniforms[i];
        if (!ngli_darray_push(&s->crafter_uniforms, crafter_uniform))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int register_texture(struct pass *s, const char *name, struct ngl_node *texture, int stage)
{
    if (!ngli_darray_push(&s->texture_nodes, &texture))
        return NGL_ERROR_MEMORY;

    struct texture_priv *texture_priv = texture->priv_data;

    /*
     * Note: we do not assign crafter_texture.texture here since it is not
     * initialized yet: non-media texture are initialized during prefetch and
     * media-texture are initialized during update.
     */
    struct pgcraft_texture crafter_texture = {
        .stage  = stage,
        .image  = &texture_priv->image,
        .format = texture_priv->params.format,
    };
    snprintf(crafter_texture.name, sizeof(crafter_texture.name), "%s", name);

    switch (texture->class->id) {
    case NGL_NODE_TEXTURE2D:   crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE2D; break;
    case NGL_NODE_TEXTURE3D:   crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE3D; break;
    case NGL_NODE_TEXTURECUBE: crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE;      break;
    default:
        ngli_assert(0);
    }

    const struct pass_params *params = &s->params;
    if (params->properties) {
        const struct ngl_node *resprops_node = ngli_hmap_get(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_priv *resprops = resprops_node->priv_data;
            if (resprops->as_image) {
                if (texture->class->id != NGL_NODE_TEXTURE2D) {
                    LOG(ERROR, "\"%s\" can not be accessed as an image; only Texture2D is supported as image", name);
                    return NGL_ERROR_UNSUPPORTED;
                }
                crafter_texture.type = NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE2D;
            }
            crafter_texture.writable  = resprops->writable;
            crafter_texture.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_textures, &crafter_texture))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_block(struct pass *s, const char *name, struct ngl_node *block_node, int stage)
{
    int ret = ngli_node_block_ref(block_node);
    if (ret < 0)
        return ret;

    if (!ngli_darray_push(&s->block_nodes, &block_node)) {
        ngli_node_block_unref(block_node);
        return NGL_ERROR_MEMORY;
    }

    struct ngl_ctx *ctx = s->ctx;
    struct gctx *gctx = ctx->gctx;
    const struct limits *limits = &gctx->limits;

    struct pgcraft_block crafter_block = {.stage = stage};
    snprintf(crafter_block.name, sizeof(crafter_block.name), "%s", name);

    struct block_priv *block_priv = block_node->priv_data;
    struct block *block = &block_priv->block;

    /*
     * Select buffer type. We prefer UBO over SSBO, but in the following
     * situations, UBO is not possible.
     */
    int type = NGLI_TYPE_UNIFORM_BUFFER;
    if (block->layout == NGLI_BLOCK_LAYOUT_STD430) {
        LOG(DEBUG, "block %s has a std430 layout, declaring it as SSBO", name);
        type = NGLI_TYPE_STORAGE_BUFFER;
    } else if (block->size > limits->max_uniform_block_size) {
        LOG(DEBUG, "block %s is larger than the max UBO size (%d > %d), declaring it as SSBO",
            name, block->size, limits->max_uniform_block_size);
        type = NGLI_TYPE_STORAGE_BUFFER;
    } else {
        const struct pass_params *params = &s->params;
        if (params->properties) {
            const struct ngl_node *resprops_node = ngli_hmap_get(params->properties, name);
            if (resprops_node) {
                const struct resourceprops_priv *resprops = resprops_node->priv_data;
                if (resprops->variadic || resprops->writable)
                    type = NGLI_TYPE_STORAGE_BUFFER;
            }
        }
    }

    /*
     * Warning: the block type may be adjusted by another pass if the block
     * node is shared between them. This means shared crafter blocks must
     * point to the same block: pgcraft_block struct can not contain a copy of
     * the block.
     *
     * Also note that the program crafting happens in the pass prepare, which
     * happens after all pass inits, so all blocks will be registered by this
     * time.
     */
    block->type = type;
    crafter_block.block = block;
    crafter_block.buffer = block_priv->buffer;

    if (!ngli_darray_push(&s->crafter_blocks, &crafter_block))
        return NGL_ERROR_MEMORY;

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

static int register_attribute(struct pass *s, const char *name, struct ngl_node *attribute, int rate)
{
    if (!attribute)
        return 0;

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
    struct buffer *buffer = attribute_priv->buffer;

    if (attribute_priv->block) {
        struct block_priv *block_priv = attribute_priv->block->priv_data;
        const struct block *block = &block_priv->block;
        const struct block_field *fields = ngli_darray_data(&block->fields);
        const struct block_field *fi = &fields[attribute_priv->block_field];
        stride = fi->stride;
        offset = fi->offset;
        buffer = block_priv->buffer;
    }

    /*
     * FIXME: we should probably expose ngl_position as vec3 instead of vec4 to
     * avoid this exception.
     */
    const int attr_type = strcmp(name, "ngl_position") ? attribute_priv->data_type : NGLI_TYPE_VEC4;

    struct pgcraft_attribute crafter_attribute = {
        .type   = attr_type,
        .format = format,
        .stride = stride,
        .offset = offset,
        .rate   = rate,
        .buffer = buffer,
    };
    snprintf(crafter_attribute.name, sizeof(crafter_attribute.name), "%s", name);

    const struct pass_params *params = &s->params;
    if (params->properties) {
        const struct ngl_node *resprops_node = ngli_hmap_get(params->properties, name);
        if (resprops_node) {
            const struct resourceprops_priv *resprops = resprops_node->priv_data;
            crafter_attribute.precision = resprops->precision;
        }
    }

    if (!ngli_darray_push(&s->crafter_attributes, &crafter_attribute))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int register_resource(struct pass *s, const char *name, struct ngl_node *node, int stage)
{
    switch (node->class->category) {
    case NGLI_NODE_CATEGORY_UNIFORM:
    case NGLI_NODE_CATEGORY_BUFFER:  return register_uniform(s, name, node, stage);
    case NGLI_NODE_CATEGORY_TEXTURE: return register_texture(s, name, node, stage);
    case NGLI_NODE_CATEGORY_BLOCK:   return register_block(s, name, node, stage);
    default:
        ngli_assert(0);
    }
}

static int register_resources(struct pass *s, const struct hmap *resources, int stage)
{
    if (!resources)
        return 0;

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(resources, entry))) {
        int ret = register_resource(s, entry->key, entry->data, stage);
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
        s->indices_buffer = indices_priv->buffer;
        s->indices_format = indices_priv->data_format;
        s->nb_indices = indices_priv->count;
    } else {
        struct ngl_node *vertices = geometry_priv->vertices_buffer;
        struct buffer_priv *buffer_priv = vertices->priv_data;
        s->nb_vertices = buffer_priv->count;
    }
    s->nb_instances = s->params.nb_instances;

    int ret;

    if ((ret = register_resources(s, params->vert_resources, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = register_resources(s, params->frag_resources, NGLI_PROGRAM_SHADER_FRAG)) < 0)
        return ret;

    if ((ret = check_attributes(s, params->attributes, 0)) < 0 ||
        (ret = check_attributes(s, params->instance_attributes, 1)) < 0)
        return ret;

    if ((ret = register_attribute(s, "ngl_position", geometry_priv->vertices_buffer, 0)) < 0 ||
        (ret = register_attribute(s, "ngl_uvcoord",  geometry_priv->uvcoords_buffer, 0)) < 0 ||
        (ret = register_attribute(s, "ngl_normal",   geometry_priv->normals_buffer, 0)) < 0)
        return ret;

    if (params->attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->attributes, entry))) {
            int ret = register_attribute(s, entry->key, entry->data, 0);
            if (ret < 0)
                return ret;
        }
    }

    if (params->instance_attributes) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->instance_attributes, entry))) {
            int ret = register_attribute(s, entry->key, entry->data, 1);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int pass_compute_init(struct pass *s)
{
    const struct pass_params *params = &s->params;

    int ret = register_resources(s, params->compute_resources, NGLI_PROGRAM_SHADER_COMP);
    if (ret < 0)
        return ret;

    s->pipeline_type = NGLI_PIPELINE_TYPE_COMPUTE;

    return 0;
}

int ngli_pass_prepare(struct pass *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gctx *gctx = ctx->gctx;
    struct rnode *rnode = ctx->rnode_pos;

    const int format = rnode->rendertarget_desc.depth_stencil.format;
    if (rnode->graphicstate.depth_test && !ngli_format_has_depth(format)) {
        LOG(ERROR, "depth testing is not support on rendertargets with no depth attachment");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (rnode->graphicstate.stencil_test && !ngli_format_has_stencil(format)) {
        LOG(ERROR, "stencil operations are not support on rendertargets with no stencil attachment");
        return NGL_ERROR_INVALID_USAGE;
    }

    struct pipeline_graphics pipeline_graphics = s->pipeline_graphics;
    pipeline_graphics.state = rnode->graphicstate;
    pipeline_graphics.rt_desc = rnode->rendertarget_desc;

    struct pipeline_params pipeline_params = {
        .type          = s->pipeline_type,
        .graphics      = pipeline_graphics,
    };

    const struct pgcraft_params crafter_params = {
        .vert_base         = s->params.vert_base,
        .frag_base         = s->params.frag_base,
        .comp_base         = s->params.comp_base,
        .uniforms          = ngli_darray_data(&s->crafter_uniforms),
        .nb_uniforms       = ngli_darray_count(&s->crafter_uniforms),
        .textures          = ngli_darray_data(&s->crafter_textures),
        .nb_textures       = ngli_darray_count(&s->crafter_textures),
        .attributes        = ngli_darray_data(&s->crafter_attributes),
        .nb_attributes     = ngli_darray_count(&s->crafter_attributes),
        .blocks            = ngli_darray_data(&s->crafter_blocks),
        .nb_blocks         = ngli_darray_count(&s->crafter_blocks),
        .vert_out_vars     = s->params.vert_out_vars,
        .nb_vert_out_vars  = s->params.nb_vert_out_vars,
        .nb_frag_output    = s->params.nb_frag_output,
    };

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    ctx->rnode_pos->id = ngli_darray_count(&s->pipeline_descs) - 1;

    memset(desc, 0, sizeof(*desc));

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    int ret = ngli_pgcraft_craft(desc->crafter, &pipeline_params, &crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline = ngli_pipeline_create(gctx);
    if (!desc->pipeline)
        return NGL_ERROR_MEMORY;

    ret = ngli_pipeline_init(desc->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->normal_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "ngl_normal_matrix", NGLI_PROGRAM_SHADER_VERT);
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

    ngli_darray_init(&s->crafter_attributes, sizeof(struct pgcraft_attribute), 0);
    ngli_darray_init(&s->crafter_textures, sizeof(struct pgcraft_texture), 0);
    ngli_darray_init(&s->crafter_uniforms, sizeof(struct pgcraft_uniform), 0);
    ngli_darray_init(&s->crafter_blocks, sizeof(struct pgcraft_block), 0);

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

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
        ngli_pipeline_freep(&desc->pipeline);
        ngli_pgcraft_freep(&desc->crafter);
    }
    ngli_darray_reset(&s->pipeline_descs);

    if (s->indices)
        ngli_node_buffer_unref(s->indices);

    ngli_darray_reset(&s->uniform_nodes);
    ngli_darray_reset(&s->texture_nodes);
    reset_block_nodes(&s->block_nodes);
    reset_buffer_nodes(&s->attribute_nodes);

    ngli_darray_reset(&s->crafter_attributes);
    ngli_darray_reset(&s->crafter_textures);
    ngli_darray_reset(&s->crafter_uniforms);
    ngli_darray_reset(&s->crafter_blocks);

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
    const struct pass_params *params = &s->params;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];
    struct pipeline *pipeline = desc->pipeline;

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

    struct darray *texture_infos_array = &desc->crafter->texture_infos;
    struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        struct pgcraft_texture_info *info = &texture_infos[i];
        const struct pgcraft_texture_info_field *fields = info->fields;
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

    if (s->pipeline_type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        if (ctx->bind_current_rendertarget) {
            struct gctx *gctx = ctx->gctx;
            ngli_gctx_set_rendertarget(gctx, ctx->current_rendertarget);
            ctx->bind_current_rendertarget = 0;
        }

        if (s->indices_buffer)
            ngli_pipeline_draw_indexed(pipeline, s->indices_buffer, s->indices_format, s->nb_indices, s->nb_instances);
        else
            ngli_pipeline_draw(pipeline, s->nb_vertices, s->nb_instances);
    } else {
        ngli_pipeline_dispatch(pipeline, params->nb_group_x, params->nb_group_y, params->nb_group_z);
    }

    return 0;
}
