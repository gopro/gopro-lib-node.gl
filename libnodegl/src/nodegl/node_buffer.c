/*
 * Copyright 2017 GoPro Inc.
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

#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "type.h"

#define OFFSET(x) offsetof(struct buffer_priv, x)
static const struct node_param buffer_params[] = {
    {"count",  PARAM_TYPE_INT,    OFFSET(count),
               .desc=NGLI_DOCSTRING("number of elements")},
    {"data",   PARAM_TYPE_DATA,   OFFSET(data),
               .desc=NGLI_DOCSTRING("buffer of `count` elements")},
    {"filename", PARAM_TYPE_STR,  OFFSET(filename),
               .desc=NGLI_DOCSTRING("filename from which the buffer will be read, cannot be used with `data`")},
    {"block",  PARAM_TYPE_NODE,    OFFSET(block),
               .node_types=(const int[]){NGL_NODE_BLOCK, -1},
               .desc=NGLI_DOCSTRING("reference a field from the given block")},
    {"block_field", PARAM_TYPE_INT, OFFSET(block_field),
                    .desc=NGLI_DOCSTRING("field index in `block`")},
    {NULL}
};

int ngli_node_buffer_ref(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct buffer_priv *s = node->priv_data;

    if (s->block)
        return ngli_node_block_ref(s->block);

    if (s->buffer_refcount++ == 0) {
        s->buffer = ngli_buffer_create(gctx);
        if (!s->buffer)
            return NGL_ERROR_MEMORY;

        int ret = ngli_buffer_init(s->buffer, s->data_size, s->usage);
        if (ret < 0)
            return ret;

        ret = ngli_buffer_upload(s->buffer, s->data, s->data_size, 0);
        if (ret < 0)
            return ret;

        s->buffer_last_upload_time = -1.;
    }

    return 0;
}

void ngli_node_buffer_unref(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->block)
        return ngli_node_block_unref(s->block);

    ngli_assert(s->buffer_refcount);
    if (s->buffer_refcount-- == 1)
        ngli_buffer_freep(&s->buffer);
}

int ngli_node_buffer_upload(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->block)
        return ngli_node_block_upload(s->block);

    if (s->dynamic && s->buffer_last_upload_time != node->last_update_time) {
        int ret = ngli_buffer_upload(s->buffer, s->data, s->data_size, 0);
        if (ret < 0)
            return ret;
        s->buffer_last_upload_time = node->last_update_time;
    }

    return 0;
}

static int buffer_init_from_data(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->count = s->count ? s->count : s->data_size / s->data_stride;
    if (s->data_size != s->count * s->data_stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            s->count,
            s->data_stride,
            s->data_size);
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

static int buffer_init_from_filename(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->fd = open(s->filename, O_RDONLY);
    if (s->fd < 0) {
        LOG(ERROR, "could not open '%s'", s->filename);
        return NGL_ERROR_IO;
    }

    off_t filesize = lseek(s->fd, 0, SEEK_END);
    off_t ret      = lseek(s->fd, 0, SEEK_SET);
    if (filesize < 0 || ret < 0) {
        LOG(ERROR, "could not seek in '%s'", s->filename);
        return NGL_ERROR_IO;
    }
    s->data_size = filesize;
    s->count = s->count ? s->count : s->data_size / s->data_stride;

    if (s->data_size != s->count * s->data_stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            s->count,
            s->data_stride,
            s->data_size);
        return NGL_ERROR_INVALID_DATA;
    }

    s->data = ngli_calloc(s->count, s->data_stride);
    if (!s->data)
        return NGL_ERROR_MEMORY;

    int n = read(s->fd, s->data, s->data_size);
    if (n < 0) {
        LOG(ERROR, "could not read '%s': %zd", s->filename, n);
        return NGL_ERROR_IO;
    }

    if (n != s->data_size) {
        LOG(ERROR, "read %zd bytes does not match expected size of %d bytes", n, s->data_size);
        return NGL_ERROR_IO;
    }

    return 0;
}

static int buffer_init_from_count(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->count = s->count ? s->count : 1;
    s->data_size = s->count * s->data_stride;
    s->data = ngli_calloc(s->count, s->data_stride);
    if (!s->data)
        return NGL_ERROR_MEMORY;

    return 0;
}

static int buffer_init_from_block(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;
    const struct block_priv *block = s->block->priv_data;
    if (s->block_field < 0 || s->block_field >= block->nb_fields) {
        LOG(ERROR, "invalid field id %d; %s has %d fields",
            s->block_field, s->block->label, block->nb_fields);
        return NGL_ERROR_INVALID_ARG;
    }

    struct ngl_node *buffer_target = block->fields[s->block_field];
    if (buffer_target->class->id != node->class->id) {
        LOG(ERROR, "%s[%d] of type %s mismatches %s local type",
            s->block->label, s->block_field, buffer_target->class->name, node->class->name);
        return NGL_ERROR_INVALID_ARG;
    }

    struct buffer_priv *buffer_target_priv = buffer_target->priv_data;
    if (s->count > buffer_target_priv->count) {
        LOG(ERROR, "block buffer reference count can not be larger than target buffer count (%d > %d)",
            s->count, buffer_target_priv->count);
        return NGL_ERROR_INVALID_ARG;
    }
    s->count = s->count ? s->count : buffer_target_priv->count;
    s->data = buffer_target_priv->data;
    s->data_stride = buffer_target_priv->data_stride;
    s->data_size = s->count * s->data_stride;

    return 0;
}

static int buffer_init(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->data && s->filename) {
        LOG(ERROR,
            "data and filename option cannot be set at the same time");
        return NGL_ERROR_INVALID_ARG;
    }

    if (s->block && (s->data || s->filename)) {
        LOG(ERROR, "block option can not be set with data or filename");
        return NGL_ERROR_INVALID_ARG;
    }

    if (node->class->id == NGL_NODE_BUFFERMAT4) {
        s->data_comp = 4 * 4;
        s->data_stride = s->data_comp * sizeof(float);
    } else {
        s->data_comp = ngli_format_get_nb_comp(s->data_format);
        s->data_stride = ngli_format_get_bytes_per_pixel(s->data_format);
    }

    s->usage = NGLI_BUFFER_USAGE_STATIC;

    if (s->data)
        return buffer_init_from_data(node);
    if (s->filename)
        return buffer_init_from_filename(node);
    if (s->block)
        return buffer_init_from_block(node);

    return buffer_init_from_count(node);
}

static void buffer_uninit(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    if (s->filename) {
        ngli_freep(&s->data);
        s->data_size = 0;

        if (s->fd) {
            int ret = close(s->fd);
            if (ret < 0) {
                LOG(ERROR, "could not properly close '%s'", s->filename);
            }
        }
    } else if (s->block) {
        /* Prevent the param API to free a non-owned pointer */
        s->data = NULL;
        s->data_size = 0;
    }
}

#define DEFINE_BUFFER_CLASS(class_id, class_name, type, format, dtype) \
static int buffer##type##_init(struct ngl_node *node)           \
{                                                               \
    struct buffer_priv *s = node->priv_data;                    \
    s->data_format = format;                                    \
    s->data_type = dtype;                                       \
    return buffer_init(node);                                   \
}                                                               \
                                                                \
const struct node_class ngli_buffer##type##_class = {           \
    .id        = class_id,                                      \
    .category  = NGLI_NODE_CATEGORY_BUFFER,                     \
    .name      = class_name,                                    \
    .init      = buffer##type##_init,                           \
    .uninit    = buffer_uninit,                                 \
    .priv_size = sizeof(struct buffer_priv),                    \
    .params    = buffer_params,                                 \
    .params_id = "Buffer",                                      \
    .file      = __FILE__,                                      \
};

DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBYTE,   "BufferByte",   byte,   NGLI_FORMAT_R8_SNORM,            NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC2,  "BufferBVec2",  bvec2,  NGLI_FORMAT_R8G8_SNORM,          NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC3,  "BufferBVec3",  bvec3,  NGLI_FORMAT_R8G8B8_SNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC4,  "BufferBVec4",  bvec4,  NGLI_FORMAT_R8G8B8A8_SNORM,      NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT,    "BufferInt",    int,    NGLI_FORMAT_R32_SINT,            NGLI_TYPE_INT)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT64,  "BufferInt64",  int64,  NGLI_FORMAT_R64_SINT,            NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC2,  "BufferIVec2",  ivec2,  NGLI_FORMAT_R32G32_SINT,         NGLI_TYPE_IVEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC3,  "BufferIVec3",  ivec3,  NGLI_FORMAT_R32G32B32_SINT,      NGLI_TYPE_IVEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC4,  "BufferIVec4",  ivec4,  NGLI_FORMAT_R32G32B32A32_SINT,   NGLI_TYPE_IVEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSHORT,  "BufferShort",  short,  NGLI_FORMAT_R16_SNORM,           NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC2,  "BufferSVec2",  svec2,  NGLI_FORMAT_R16G16_SNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC3,  "BufferSVec3",  svec3,  NGLI_FORMAT_R16G16B16_SNORM,     NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC4,  "BufferSVec4",  svec4,  NGLI_FORMAT_R16G16B16A16_SNORM,  NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBYTE,  "BufferUByte",  ubyte,  NGLI_FORMAT_R8_UNORM,            NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC2, "BufferUBVec2", ubvec2, NGLI_FORMAT_R8G8_UNORM,          NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC3, "BufferUBVec3", ubvec3, NGLI_FORMAT_R8G8B8_UNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC4, "BufferUBVec4", ubvec4, NGLI_FORMAT_R8G8B8A8_UNORM,      NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUINT,   "BufferUInt",   uint,   NGLI_FORMAT_R32_UINT,            NGLI_TYPE_UINT)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC2, "BufferUIVec2", uivec2, NGLI_FORMAT_R32G32_UINT,         NGLI_TYPE_UIVEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC3, "BufferUIVec3", uivec3, NGLI_FORMAT_R32G32B32_UINT,      NGLI_TYPE_UIVEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC4, "BufferUIVec4", uivec4, NGLI_FORMAT_R32G32B32A32_UINT,   NGLI_TYPE_UIVEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSHORT, "BufferUShort", ushort, NGLI_FORMAT_R16_UNORM,           NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC2, "BufferUSVec2", usvec2, NGLI_FORMAT_R16G16_UNORM,        NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC3, "BufferUSVec3", usvec3, NGLI_FORMAT_R16G16B16_UNORM,     NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC4, "BufferUSVec4", usvec4, NGLI_FORMAT_R16G16B16A16_UNORM,  NGLI_TYPE_NONE)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERFLOAT,  "BufferFloat",  float,  NGLI_FORMAT_R32_SFLOAT,          NGLI_TYPE_FLOAT)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC2,   "BufferVec2",   vec2,   NGLI_FORMAT_R32G32_SFLOAT,       NGLI_TYPE_VEC2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC3,   "BufferVec3",   vec3,   NGLI_FORMAT_R32G32B32_SFLOAT,    NGLI_TYPE_VEC3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC4,   "BufferVec4",   vec4,   NGLI_FORMAT_R32G32B32A32_SFLOAT, NGLI_TYPE_VEC4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERMAT4,   "BufferMat4",   mat4,   NGLI_FORMAT_R32G32B32A32_SFLOAT, NGLI_TYPE_MAT4)
