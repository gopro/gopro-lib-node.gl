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

#include <string.h>
#include <stddef.h>

#include "block.h"
#include "buffer.h"
#include "gctx.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

static const struct param_choices layout_choices = {
    .name = "memory_layout",
    .consts = {
        {"std140", NGLI_BLOCK_LAYOUT_STD140, .desc=NGLI_DOCSTRING("standard uniform block memory layout 140")},
        {"std430", NGLI_BLOCK_LAYOUT_STD430, .desc=NGLI_DOCSTRING("standard uniform block memory layout 430")},
        {NULL}
    }
};

#define FIELD_TYPES_LIST (const int[]){NGL_NODE_ANIMATEDBUFFERFLOAT,    \
                                       NGL_NODE_ANIMATEDBUFFERVEC2,     \
                                       NGL_NODE_ANIMATEDBUFFERVEC3,     \
                                       NGL_NODE_ANIMATEDBUFFERVEC4,     \
                                       NGL_NODE_STREAMEDBUFFERINT,      \
                                       NGL_NODE_STREAMEDBUFFERIVEC2,    \
                                       NGL_NODE_STREAMEDBUFFERIVEC3,    \
                                       NGL_NODE_STREAMEDBUFFERIVEC4,    \
                                       NGL_NODE_STREAMEDBUFFERUINT,     \
                                       NGL_NODE_STREAMEDBUFFERUIVEC2,   \
                                       NGL_NODE_STREAMEDBUFFERUIVEC3,   \
                                       NGL_NODE_STREAMEDBUFFERUIVEC4,   \
                                       NGL_NODE_STREAMEDBUFFERFLOAT,    \
                                       NGL_NODE_STREAMEDBUFFERVEC2,     \
                                       NGL_NODE_STREAMEDBUFFERVEC3,     \
                                       NGL_NODE_STREAMEDBUFFERVEC4,     \
                                       NGL_NODE_BUFFERFLOAT,            \
                                       NGL_NODE_BUFFERVEC2,             \
                                       NGL_NODE_BUFFERVEC3,             \
                                       NGL_NODE_BUFFERVEC4,             \
                                       NGL_NODE_BUFFERINT,              \
                                       NGL_NODE_BUFFERIVEC2,            \
                                       NGL_NODE_BUFFERIVEC3,            \
                                       NGL_NODE_BUFFERIVEC4,            \
                                       NGL_NODE_BUFFERUINT,             \
                                       NGL_NODE_BUFFERUIVEC2,           \
                                       NGL_NODE_BUFFERUIVEC3,           \
                                       NGL_NODE_BUFFERUIVEC4,           \
                                       NGL_NODE_BUFFERMAT4,             \
                                       NGL_NODE_UNIFORMBOOL,            \
                                       NGL_NODE_UNIFORMFLOAT,           \
                                       NGL_NODE_UNIFORMVEC2,            \
                                       NGL_NODE_UNIFORMVEC3,            \
                                       NGL_NODE_UNIFORMVEC4,            \
                                       NGL_NODE_UNIFORMINT,             \
                                       NGL_NODE_UNIFORMIVEC2,           \
                                       NGL_NODE_UNIFORMIVEC3,           \
                                       NGL_NODE_UNIFORMIVEC4,           \
                                       NGL_NODE_UNIFORMUINT,            \
                                       NGL_NODE_UNIFORMUIVEC2,          \
                                       NGL_NODE_UNIFORMUIVEC3,          \
                                       NGL_NODE_UNIFORMUIVEC4,          \
                                       NGL_NODE_UNIFORMMAT4,            \
                                       NGL_NODE_UNIFORMQUAT,            \
                                       NGL_NODE_ANIMATEDFLOAT,          \
                                       NGL_NODE_ANIMATEDVEC2,           \
                                       NGL_NODE_ANIMATEDVEC3,           \
                                       NGL_NODE_ANIMATEDVEC4,           \
                                       NGL_NODE_ANIMATEDQUAT,           \
                                       NGL_NODE_STREAMEDINT,            \
                                       NGL_NODE_STREAMEDIVEC2,          \
                                       NGL_NODE_STREAMEDIVEC3,          \
                                       NGL_NODE_STREAMEDIVEC4,          \
                                       NGL_NODE_STREAMEDUINT,           \
                                       NGL_NODE_STREAMEDUIVEC2,         \
                                       NGL_NODE_STREAMEDUIVEC3,         \
                                       NGL_NODE_STREAMEDUIVEC4,         \
                                       NGL_NODE_STREAMEDFLOAT,          \
                                       NGL_NODE_STREAMEDVEC2,           \
                                       NGL_NODE_STREAMEDVEC3,           \
                                       NGL_NODE_STREAMEDVEC4,           \
                                       NGL_NODE_STREAMEDMAT4,           \
                                       NGL_NODE_TIME,                   \
                                       -1}

#define OFFSET(x) offsetof(struct block_priv, x)
static const struct node_param block_params[] = {
    {"fields", PARAM_TYPE_NODELIST, OFFSET(fields),
               .node_types=FIELD_TYPES_LIST,
               .desc=NGLI_DOCSTRING("block fields defined in the graphic program")},
    {"layout", PARAM_TYPE_SELECT, OFFSET(layout), {.i64=NGLI_BLOCK_LAYOUT_STD140},
               .choices=&layout_choices,
               .desc=NGLI_DOCSTRING("memory layout set in the graphic program")},
    {NULL}
};

int ngli_node_block_ref(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct block_priv *s = node->priv_data;

    if (s->buffer_refcount++ == 0) {
        s->buffer = ngli_buffer_create(gctx);
        if (!s->buffer)
            return NGL_ERROR_MEMORY;

        int ret = ngli_buffer_init(s->buffer, s->data_size, s->usage);
        if (ret < 0)
            return ret;

        ret = ngli_buffer_upload(s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;

        s->buffer_last_upload_time = -1.;
    }

    return 0;
}

void ngli_node_block_unref(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    ngli_assert(s->buffer_refcount);
    if (s->buffer_refcount-- == 1)
        ngli_buffer_freep(&s->buffer);
}

int ngli_node_block_upload(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    if (s->has_changed && s->buffer_last_upload_time != node->last_update_time) {
        int ret = ngli_buffer_upload(s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;
        s->buffer_last_upload_time = node->last_update_time;
        s->has_changed = 0;
    }

    return 0;
}

static int get_node_data_type(const struct ngl_node *node)
{
    if (node->class->category == NGLI_NODE_CATEGORY_UNIFORM) {
        const struct variable_priv *variable = node->priv_data;
        return variable->data_type;
    } else if (node->class->category == NGLI_NODE_CATEGORY_BUFFER) {
        const struct buffer_priv *buffer = node->priv_data;
        return buffer->data_type;
    } else {
        ngli_assert(0);
    }
}

static int get_node_data_count(const struct ngl_node *node)
{
    if (node->class->category == NGLI_NODE_CATEGORY_UNIFORM) {
        return 0;
    } else if (node->class->category == NGLI_NODE_CATEGORY_BUFFER) {
        const struct buffer_priv *buffer = node->priv_data;
        return buffer->count;
    } else {
        ngli_assert(0);
    }
}

static int has_changed_uniform(const struct ngl_node *unode)
{
    const struct variable_priv *uniform = unode->priv_data;
    return uniform->dynamic || uniform->live_changed;
}

static int has_changed_buffer(const struct ngl_node *bnode)
{
    const struct buffer_priv *buffer = bnode->priv_data;
    return buffer->dynamic;
}

static void update_uniform_field(uint8_t *dst,
                                 const struct ngl_node *node,
                                 const struct block_field *fi)
{
    const struct variable_priv *uniform = node->priv_data;
    ngli_block_field_copy(fi, dst, uniform->data);
}

static void update_buffer_field(uint8_t *dst,
                                const struct ngl_node *node,
                                const struct block_field *fi)
{
    const struct buffer_priv *buffer = node->priv_data;
    ngli_block_field_copy(fi, dst, buffer->data);
}

enum field_type { IS_SINGLE, IS_ARRAY };

static const struct {
    int (*has_changed)(const struct ngl_node *node);
    void (*update_data)(uint8_t *dst, const struct ngl_node *node, const struct block_field *fi);
} field_funcs[] = {
    [IS_SINGLE] = {has_changed_uniform, update_uniform_field},
    [IS_ARRAY]  = {has_changed_buffer,  update_buffer_field},
};

static void update_block_data(struct block_priv *s, int forced)
{
    const struct block_field *field_info = ngli_darray_data(&s->block.fields);
    for (int i = 0; i < s->nb_fields; i++) {
        const struct ngl_node *field_node = s->fields[i];
        const struct block_field *fi = &field_info[i];
        if (!forced && !field_funcs[fi->count ? IS_ARRAY : IS_SINGLE].has_changed(field_node))
            continue;
        field_funcs[fi->count ? IS_ARRAY : IS_SINGLE].update_data(s->data + fi->offset, field_node, fi);
        s->has_changed = 1; // TODO: only re-upload the changing data segments
    }
}

static int cmp_str(const void *a, const void *b)
{
    const char *s0 = *(const char * const *)a;
    const char *s1 = *(const char * const *)b;
    return strcmp(s0, s1);
}

static int check_dup_labels(const char *block_name, struct ngl_node * const *nodes, int nb_nodes)
{
    const char **labels = ngli_calloc(nb_nodes, sizeof(*labels));
    if (!labels)
        return NGL_ERROR_MEMORY;
    for (int i = 0; i < nb_nodes; i++) {
        if (!nodes[i]->label) {
            LOG(ERROR, "block field labels cannot be NULL");
            return NGL_ERROR_INVALID_ARG;
        }
        labels[i] = nodes[i]->label;
    }
    qsort(labels, nb_nodes, sizeof(*labels), cmp_str);
    for (int i = 1; i < nb_nodes; i++) {
        if (!strcmp(labels[i - 1], labels[i])) {
            LOG(ERROR, "duplicated label %s in block %s", labels[i], block_name);
            ngli_free(labels);
            return NGL_ERROR_INVALID_ARG;
        }
    }
    ngli_free(labels);
    return 0;
}

#define FEATURES_STD140 (NGLI_FEATURE_UNIFORM_BUFFER_OBJECT | NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)
#define FEATURES_STD430 (NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)

static int block_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct block_priv *s = node->priv_data;

    if (s->layout == NGLI_BLOCK_LAYOUT_STD140 && !(gctx->features & FEATURES_STD140)) {
        LOG(ERROR, "std140 blocks are not supported by this context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (s->layout == NGLI_BLOCK_LAYOUT_STD430 && !(gctx->features & FEATURES_STD430)) {
        LOG(ERROR, "std430 blocks are not supported by this context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (!s->nb_fields) {
        LOG(ERROR, "block fields must not be empty");
        return NGL_ERROR_INVALID_ARG;
    }

    int ret = check_dup_labels(node->label, s->fields, s->nb_fields);
    if (ret < 0)
        return ret;

    ngli_block_init(&s->block, s->layout);

    s->usage = NGLI_BUFFER_USAGE_STATIC;

    for (int i = 0; i < s->nb_fields; i++) {
        const struct ngl_node *field_node = s->fields[i];
        const int type  = get_node_data_type(field_node);
        const int count = get_node_data_count(field_node);

        int ret = ngli_block_add_field(&s->block, field_node->label, type, count);
        if (ret < 0)
            return ret;

        if (field_funcs[count ? IS_ARRAY : IS_SINGLE].has_changed(field_node))
            s->usage = NGLI_BUFFER_USAGE_DYNAMIC;

        const struct block_field *fields = ngli_darray_data(&s->block.fields);
        const struct block_field *fi = &fields[i];
        LOG(DEBUG, "%s.field[%d]: %s offset=%d size=%d stride=%d",
            node->label, i, field_node->label, fi->offset, fi->size, fi->stride);
    }

    s->data_size = s->block.size;
    LOG(DEBUG, "total %s size: %d", node->label, s->data_size);
    s->data = ngli_calloc(1, s->data_size);
    if (!s->data)
        return NGL_ERROR_MEMORY;

    update_block_data(s, 1);
    return 0;
}

static int block_update(struct ngl_node *node, double t)
{
    struct block_priv *s = node->priv_data;

    // Check for live changes (the uniform updates below reset their
    // live_changed flag to 0)
    update_block_data(s, 0);

    for (int i = 0; i < s->nb_fields; i++) {
        struct ngl_node *field_node = s->fields[i];
        int ret = ngli_node_update(field_node, t);
        if (ret < 0)
            return ret;
    }

    // Check for update changes (animations)
    update_block_data(s, 0);

    return 0;
}

static void block_uninit(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    ngli_block_reset(&s->block);
    ngli_free(s->data);
}

const struct node_class ngli_block_class = {
    .id        = NGL_NODE_BLOCK,
    .category  = NGLI_NODE_CATEGORY_BLOCK,
    .name      = "Block",
    .init      = block_init,
    .update    = block_update,
    .uninit    = block_uninit,
    .priv_size = sizeof(struct block_priv),
    .params    = block_params,
    .file      = __FILE__,
};
