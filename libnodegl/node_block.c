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

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

enum {
    LAYOUT_STD140,
    LAYOUT_STD430,
    NB_LAYOUTS
};

static const struct param_choices layout_choices = {
    .name = "memory_layout",
    .consts = {
        {"std140", LAYOUT_STD140, .desc=NGLI_DOCSTRING("standard uniform block memory layout 140")},
        {"std430", LAYOUT_STD430, .desc=NGLI_DOCSTRING("standard uniform block memory layout 430")},
        {NULL}
    }
};

#define FIELD_TYPES_BUFFER_LIST           NGL_NODE_ANIMATEDBUFFERFLOAT, \
                                          NGL_NODE_ANIMATEDBUFFERVEC2,  \
                                          NGL_NODE_ANIMATEDBUFFERVEC3,  \
                                          NGL_NODE_ANIMATEDBUFFERVEC4,  \
                                          NGL_NODE_BUFFERFLOAT,         \
                                          NGL_NODE_BUFFERVEC2,          \
                                          NGL_NODE_BUFFERVEC3,          \
                                          NGL_NODE_BUFFERVEC4,          \
                                          NGL_NODE_BUFFERINT,           \
                                          NGL_NODE_BUFFERIVEC2,         \
                                          NGL_NODE_BUFFERIVEC3,         \
                                          NGL_NODE_BUFFERIVEC4,         \
                                          NGL_NODE_BUFFERUINT,          \
                                          NGL_NODE_BUFFERUIVEC2,        \
                                          NGL_NODE_BUFFERUIVEC3,        \
                                          NGL_NODE_BUFFERUIVEC4,        \
                                          NGL_NODE_BUFFERMAT4

#define FIELD_TYPES_UNIFORMS_LIST         NGL_NODE_UNIFORMFLOAT,        \
                                          NGL_NODE_UNIFORMVEC2,         \
                                          NGL_NODE_UNIFORMVEC3,         \
                                          NGL_NODE_UNIFORMVEC4,         \
                                          NGL_NODE_UNIFORMINT,          \
                                          NGL_NODE_UNIFORMMAT4,         \
                                          NGL_NODE_UNIFORMQUAT,         \
                                          NGL_NODE_ANIMATEDFLOAT,       \
                                          NGL_NODE_ANIMATEDVEC2,        \
                                          NGL_NODE_ANIMATEDVEC3,        \
                                          NGL_NODE_ANIMATEDVEC4,        \
                                          NGL_NODE_ANIMATEDQUAT,        \
                                          NGL_NODE_STREAMEDINT,         \
                                          NGL_NODE_STREAMEDFLOAT,       \
                                          NGL_NODE_STREAMEDVEC2,        \
                                          NGL_NODE_STREAMEDVEC3,        \
                                          NGL_NODE_STREAMEDVEC4,        \
                                          NGL_NODE_STREAMEDMAT4

#define FIELD_TYPES_LIST (const int[]){FIELD_TYPES_BUFFER_LIST, FIELD_TYPES_UNIFORMS_LIST, -1}

#define OFFSET(x) offsetof(struct block_priv, x)
static const struct node_param block_params[] = {
    {"fields", PARAM_TYPE_NODELIST, OFFSET(fields),
               .node_types=FIELD_TYPES_LIST,
               .desc=NGLI_DOCSTRING("block fields defined in the graphic program")},
    {"layout", PARAM_TYPE_SELECT, OFFSET(layout), {.i64=LAYOUT_STD140},
               .choices=&layout_choices,
               .desc=NGLI_DOCSTRING("memory layout set in the graphic program")},
    {NULL}
};

int ngli_node_block_ref(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct block_priv *s = node->priv_data;

    if (s->buffer_refcount++ == 0) {
        int ret = ngli_buffer_init(&s->buffer, ctx, s->data_size, s->usage);
        if (ret < 0)
            return ret;

        ret = ngli_buffer_upload(&s->buffer, s->data, s->data_size);
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
        ngli_buffer_reset(&s->buffer);
}

int ngli_node_block_upload(struct ngl_node *node)
{
    struct block_priv *s = node->priv_data;

    if (s->has_changed && s->buffer_last_upload_time != node->last_update_time) {
        int ret = ngli_buffer_upload(&s->buffer, s->data, s->data_size);
        if (ret < 0)
            return ret;
        s->buffer_last_upload_time = node->last_update_time;
        s->has_changed = 0;
    }

    return 0;
}

static int get_buffer_stride(const struct ngl_node *node, int layout)
{
    switch (node->class->id) {
        case NGL_NODE_ANIMATEDBUFFERFLOAT:
        case NGL_NODE_BUFFERFLOAT:          return sizeof(float) * (layout == LAYOUT_STD140 ? 4 : 1);
        case NGL_NODE_ANIMATEDBUFFERVEC2:
        case NGL_NODE_BUFFERVEC2:           return sizeof(float) * (layout == LAYOUT_STD140 ? 4 : 2);
        case NGL_NODE_ANIMATEDBUFFERVEC3:
        case NGL_NODE_BUFFERVEC3:
        case NGL_NODE_ANIMATEDBUFFERVEC4:
        case NGL_NODE_BUFFERVEC4:           return sizeof(float) * 4;
        case NGL_NODE_BUFFERINT:
        case NGL_NODE_BUFFERUINT:           return sizeof(int) * (layout == LAYOUT_STD140 ? 4 : 1);
        case NGL_NODE_BUFFERIVEC2:
        case NGL_NODE_BUFFERUIVEC2:         return sizeof(int) * (layout == LAYOUT_STD140 ? 4 : 2);
        case NGL_NODE_BUFFERIVEC3:
        case NGL_NODE_BUFFERUIVEC3:
        case NGL_NODE_BUFFERIVEC4:
        case NGL_NODE_BUFFERUIVEC4:         return sizeof(int) * 4;
        case NGL_NODE_BUFFERMAT4:           return sizeof(float) * 4 * 4;
    }
    return 0;
}

static int get_buffer_size(const struct ngl_node *bnode, int layout)
{
    const struct buffer_priv *b = bnode->priv_data;
    return b->count * get_buffer_stride(bnode, layout);
}

static int get_quat_size(const struct ngl_node *quat, int layout)
{
    struct variable_priv *quat_priv = quat->priv_data;
    return sizeof(float) * 4 * (quat_priv->as_mat4 ? 4 : 1);
}

static int get_node_size(const struct ngl_node *node, int layout)
{
    switch (node->class->id) {
        case NGL_NODE_ANIMATEDFLOAT:
        case NGL_NODE_STREAMEDFLOAT:
        case NGL_NODE_UNIFORMFLOAT:         return sizeof(float) * 1;
        case NGL_NODE_ANIMATEDVEC2:
        case NGL_NODE_STREAMEDVEC2:
        case NGL_NODE_UNIFORMVEC2:          return sizeof(float) * 2;
        case NGL_NODE_ANIMATEDVEC3:
        case NGL_NODE_STREAMEDVEC3:
        case NGL_NODE_UNIFORMVEC3:          return sizeof(float) * 3;
        case NGL_NODE_ANIMATEDVEC4:
        case NGL_NODE_STREAMEDVEC4:
        case NGL_NODE_UNIFORMVEC4:          return sizeof(float) * 4;
        case NGL_NODE_STREAMEDMAT4:
        case NGL_NODE_UNIFORMMAT4:          return sizeof(float) * 4 * 4;
        case NGL_NODE_STREAMEDINT:
        case NGL_NODE_UNIFORMINT:           return sizeof(int);
        case NGL_NODE_ANIMATEDQUAT:
        case NGL_NODE_UNIFORMQUAT:          return get_quat_size(node, layout);
        default:                            return get_buffer_size(node, layout);
    }
}

static int get_node_align(const struct ngl_node *node, int layout)
{
    switch (node->class->id) {
        case NGL_NODE_ANIMATEDFLOAT:
        case NGL_NODE_STREAMEDFLOAT:
        case NGL_NODE_UNIFORMFLOAT:         return sizeof(float) * 1;
        case NGL_NODE_ANIMATEDVEC2:
        case NGL_NODE_STREAMEDVEC2:
        case NGL_NODE_UNIFORMVEC2:          return sizeof(float) * 2;
        case NGL_NODE_ANIMATEDVEC3:
        case NGL_NODE_STREAMEDVEC3:
        case NGL_NODE_UNIFORMVEC3:
        case NGL_NODE_ANIMATEDVEC4:
        case NGL_NODE_STREAMEDVEC4:
        case NGL_NODE_UNIFORMVEC4:
        case NGL_NODE_STREAMEDMAT4:
        case NGL_NODE_UNIFORMMAT4:
        case NGL_NODE_ANIMATEDQUAT:
        case NGL_NODE_UNIFORMQUAT:
        case NGL_NODE_BUFFERMAT4:           return sizeof(float) * 4;
        case NGL_NODE_STREAMEDINT:
        case NGL_NODE_UNIFORMINT:           return sizeof(int);
        default:                            return get_buffer_stride(node, layout);
    }
    return 0;
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
                                 const struct block_field_info *fi)
{
    const struct variable_priv *uniform = node->priv_data;
    memcpy(dst, uniform->data, uniform->data_size);
}

static void update_buffer_field(uint8_t *dst,
                                const struct ngl_node *node,
                                const struct block_field_info *fi)
{
    const struct buffer_priv *buffer = node->priv_data;
    if (buffer->data_stride == fi->stride)
        memcpy(dst, buffer->data, fi->size);
    else
        for (int i = 0; i < buffer->count; i++)
            memcpy(dst + i * fi->stride, buffer->data + i * buffer->data_stride, buffer->data_stride);
}

enum field_type { IS_SINGLE, IS_ARRAY };

static const struct {
    int (*has_changed)(const struct ngl_node *node);
    void (*update_data)(uint8_t *dst, const struct ngl_node *node, const struct block_field_info *fi);
} field_funcs[] = {
    [IS_SINGLE] = {has_changed_uniform, update_uniform_field},
    [IS_ARRAY]  = {has_changed_buffer,  update_buffer_field},
};

static void update_block_data(struct block_priv *s, int forced)
{
    for (int i = 0; i < s->nb_fields; i++) {
        const struct ngl_node *field_node = s->fields[i];
        const struct block_field_info *fi = &s->field_info[i];
        if (!forced && !field_funcs[fi->is_array ? IS_ARRAY : IS_SINGLE].has_changed(field_node))
            continue;
        field_funcs[fi->is_array ? IS_ARRAY : IS_SINGLE].update_data(s->data + fi->offset, field_node, fi);
        s->has_changed = 1; // TODO: only re-upload the changing data segments
    }
}

#define FEATURES_STD140 (NGLI_FEATURE_UNIFORM_BUFFER_OBJECT | NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)
#define FEATURES_STD430 (NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)

static int block_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct block_priv *s = node->priv_data;

    if (s->layout == LAYOUT_STD140 && !(gl->features & FEATURES_STD140)) {
        LOG(ERROR, "std140 blocks are not supported by this context");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (s->layout == LAYOUT_STD430 && !(gl->features & FEATURES_STD430)) {
        LOG(ERROR, "std430 blocks are not supported by this context");
        return NGL_ERROR_UNSUPPORTED;
    }

    s->field_info = ngli_calloc(s->nb_fields, sizeof(*s->field_info));
    if (!s->field_info)
        return NGL_ERROR_MEMORY;

    s->usage = NGLI_BUFFER_USAGE_STATIC;

    s->data_size = 0;
    for (int i = 0; i < s->nb_fields; i++) {
        const struct ngl_node *field_node = s->fields[i];
        const int is_array = field_node->class->category == NGLI_NODE_CATEGORY_BUFFER;
        const int size   = get_node_size(field_node, s->layout);
        const int align  = get_node_align(field_node, s->layout);

        ngli_assert(align);

        const int remain = s->data_size % align;
        const int offset = s->data_size + (remain ? align - remain : 0);

        if (field_funcs[is_array ? IS_ARRAY : IS_SINGLE].has_changed(field_node))
            s->usage = NGLI_BUFFER_USAGE_DYNAMIC;

        struct block_field_info *fi = &s->field_info[i];
        fi->is_array = is_array;
        fi->size    = size;
        fi->stride  = get_buffer_stride(field_node, s->layout);
        fi->offset  = offset;

        s->data_size = offset + fi->size;
        LOG(DEBUG, "%s.field[%d]: %s offset=%d size=%d stride=%d",
            node->label, i, field_node->label, fi->offset, fi->size, fi->stride);
    }

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

    // Check for live changes (the uniform updates below will reset their
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

    ngli_free(s->field_info);
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
