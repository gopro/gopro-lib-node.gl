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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#ifdef VULKAN_BACKEND
// TODO
#else
static const struct param_choices usage_choices = {
    .name = "buffer_usage",
    .consts = {
        {"stream_draw",  GL_STREAM_DRAW,  .desc=NGLI_DOCSTRING("modified once by the application "
                                                               "and used at most a few times as a source for drawing")},
        {"stream_read",  GL_STREAM_READ,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used at most a few times to return the data to the application")},
        {"stream_copy",  GL_STREAM_COPY,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used at most a few times as a source for drawing")},
        {"static_draw",  GL_STATIC_DRAW,  .desc=NGLI_DOCSTRING("modified once by the application "
                                                               "and used many times as a source for drawing")},
        {"static_read",  GL_STATIC_READ,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used many times to return the data to the application")},
        {"static_copy",  GL_STATIC_COPY,  .desc=NGLI_DOCSTRING("modified once by reading data from the graphic pipeline "
                                                               "and used at most a few times a source for drawing")},
        {"dynamic_draw", GL_DYNAMIC_DRAW, .desc=NGLI_DOCSTRING("modified repeatedly by the application "
                                                               "and used many times as a source for drawing")},
        {"dynamic_read", GL_DYNAMIC_READ, .desc=NGLI_DOCSTRING("modified repeatedly by reading data from the graphic pipeline "
                                                               "and used many times to return data to the application")},
        {"dynamic_copy", GL_DYNAMIC_COPY, .desc=NGLI_DOCSTRING("modified repeatedly by reading data from the graphic pipeline "
                                                               "and used many times as a source for drawing")},
        {NULL}
    }
};
#endif

#define OFFSET(x) offsetof(struct buffer, x)
static const struct node_param buffer_params[] = {
    {"count",  PARAM_TYPE_INT,    OFFSET(count),
               .desc=NGLI_DOCSTRING("number of elements")},
    {"data",   PARAM_TYPE_DATA,   OFFSET(data),
               .desc=NGLI_DOCSTRING("buffer of `count` elements")},
    {"filename", PARAM_TYPE_STR,  OFFSET(filename),
               .desc=NGLI_DOCSTRING("filename from which the buffer will be read, cannot be used with `data`")},
    {"stride", PARAM_TYPE_INT,    OFFSET(data_stride),
               .desc=NGLI_DOCSTRING("stride of 1 element, in bytes")},
#ifdef VULKAN_BACKEND
// TODO
#else
    {"usage",  PARAM_TYPE_SELECT, OFFSET(usage),  {.i64=GL_STATIC_DRAW},
               .desc=NGLI_DOCSTRING("buffer usage hint"),
               .choices=&usage_choices},
#endif
    {NULL}
};

static int buffer_init_from_data(struct ngl_node *node)
{
    struct buffer *s = node->priv_data;

    s->count = s->count ? s->count : s->data_size / s->data_stride;
    if (s->data_size != s->count * s->data_stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            s->count,
            s->data_stride,
            s->data_size);
        return -1;
    }

    return 0;
}

static int buffer_init_from_filename(struct ngl_node *node)
{
    struct buffer *s = node->priv_data;

    s->fd = open(s->filename, O_RDONLY);
    if (s->fd < 0) {
        LOG(ERROR, "could not open '%s'", s->filename);
        return -1;
    }

    off_t filesize = lseek(s->fd, 0, SEEK_END);
    off_t ret      = lseek(s->fd, 0, SEEK_SET);
    if (filesize < 0 || ret < 0) {
        LOG(ERROR, "could not seek in '%s'", s->filename);
        return -1;
    }
    s->data_size = filesize;
    s->count = s->count ? s->count : s->data_size / s->data_stride;

    if (s->data_size != s->count * s->data_stride) {
        LOG(ERROR,
            "element count (%d) and data stride (%d) does not match data size (%d)",
            s->count,
            s->data_stride,
            s->data_size);
        return -1;
    }

    s->data = calloc(s->count, s->data_stride);
    if (!s->data)
        return -1;

    ssize_t n = read(s->fd, s->data, s->data_size);
    if (n < 0) {
        LOG(ERROR, "could not read '%s': %zd", s->filename, n);
        return -1;
    }

    if (n != s->data_size) {
        LOG(ERROR, "read %zd bytes does not match expected size of %d bytes", n, s->data_size);
        return -1;
    }

    return 0;
}

static int buffer_init_from_count(struct ngl_node *node)
{
    struct buffer *s = node->priv_data;

    s->count = s->count ? s->count : 1;
    s->data_size = s->count * s->data_stride;
    s->data = calloc(s->count, s->data_stride);
    if (!s->data)
        return -1;

    return 0;
}

// XXX: move to backend
#ifdef VULKAN_BACKEND
static int find_memory_type(struct glcontext *vk, uint32_t type_filter, VkMemoryPropertyFlags props)
{
    for (int i = 0; i < vk->phydev_mem_props.memoryTypeCount; i++)
        if ((type_filter & (1<<i)) && (vk->phydev_mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return -1;
}
#endif

static int buffer_init(struct ngl_node *node)
{
    struct buffer *s = node->priv_data;

    if (s->data && s->filename) {
        LOG(ERROR,
            "data and filename option cannot be set at the same time");
        return -1;
    }

    int ret;
    int data_comp_size;
    int nb_comp;

#ifdef VULKAN_BACKEND
    VkFormat format;

    switch (node->class->id) {
    case NGL_NODE_BUFFERBYTE:   data_comp_size = 1; nb_comp = 1; format = VK_FORMAT_R8_SINT;                break;
    case NGL_NODE_BUFFERBVEC2:  data_comp_size = 1; nb_comp = 2; format = VK_FORMAT_R8G8_SINT;              break;
    case NGL_NODE_BUFFERBVEC3:  data_comp_size = 1; nb_comp = 3; format = VK_FORMAT_R8G8B8_SINT;            break;
    case NGL_NODE_BUFFERBVEC4:  data_comp_size = 1; nb_comp = 4; format = VK_FORMAT_R8G8B8A8_SINT;          break;
    case NGL_NODE_BUFFERINT:    data_comp_size = 4; nb_comp = 1; format = VK_FORMAT_R32_SINT;               break;
    case NGL_NODE_BUFFERIVEC2:  data_comp_size = 4; nb_comp = 2; format = VK_FORMAT_R32G32_SINT;            break;
    case NGL_NODE_BUFFERIVEC3:  data_comp_size = 4; nb_comp = 3; format = VK_FORMAT_R32G32B32_SINT;         break;
    case NGL_NODE_BUFFERIVEC4:  data_comp_size = 4; nb_comp = 4; format = VK_FORMAT_R32G32B32A32_SINT;      break;
    case NGL_NODE_BUFFERSHORT:  data_comp_size = 2; nb_comp = 1; format = VK_FORMAT_R16_SINT;               break;
    case NGL_NODE_BUFFERSVEC2:  data_comp_size = 2; nb_comp = 2; format = VK_FORMAT_R16G16_SINT;            break;
    case NGL_NODE_BUFFERSVEC3:  data_comp_size = 2; nb_comp = 3; format = VK_FORMAT_R16G16B16_SINT;         break;
    case NGL_NODE_BUFFERSVEC4:  data_comp_size = 2; nb_comp = 4; format = VK_FORMAT_R16G16B16A16_SINT;      break;
    case NGL_NODE_BUFFERUBYTE:  data_comp_size = 1; nb_comp = 1; format = VK_FORMAT_R8_UINT;                break;
    case NGL_NODE_BUFFERUBVEC2: data_comp_size = 1; nb_comp = 2; format = VK_FORMAT_R8G8_UINT;              break;
    case NGL_NODE_BUFFERUBVEC3: data_comp_size = 1; nb_comp = 3; format = VK_FORMAT_R8G8B8_UINT;            break;
    case NGL_NODE_BUFFERUBVEC4: data_comp_size = 1; nb_comp = 4; format = VK_FORMAT_R8G8B8A8_UINT;          break;
    case NGL_NODE_BUFFERUINT:   data_comp_size = 4; nb_comp = 1; format = VK_FORMAT_R32_UINT;               break;
    case NGL_NODE_BUFFERUIVEC2: data_comp_size = 4; nb_comp = 2; format = VK_FORMAT_R32G32_UINT;            break;
    case NGL_NODE_BUFFERUIVEC3: data_comp_size = 4; nb_comp = 3; format = VK_FORMAT_R32G32B32_UINT;         break;
    case NGL_NODE_BUFFERUIVEC4: data_comp_size = 4; nb_comp = 4; format = VK_FORMAT_R32G32B32A32_UINT;      break;
    case NGL_NODE_BUFFERUSHORT: data_comp_size = 2; nb_comp = 1; format = VK_FORMAT_R16_UINT;               break;
    case NGL_NODE_BUFFERUSVEC2: data_comp_size = 2; nb_comp = 2; format = VK_FORMAT_R16G16_UINT;            break;
    case NGL_NODE_BUFFERUSVEC3: data_comp_size = 2; nb_comp = 3; format = VK_FORMAT_R16G16B16_UINT;         break;
    case NGL_NODE_BUFFERUSVEC4: data_comp_size = 2; nb_comp = 4; format = VK_FORMAT_R16G16B16A16_UINT;      break;
    case NGL_NODE_BUFFERFLOAT:  data_comp_size = 4; nb_comp = 1; format = VK_FORMAT_R32_SFLOAT;             break;
    case NGL_NODE_BUFFERVEC2:   data_comp_size = 4; nb_comp = 2; format = VK_FORMAT_R32G32_SFLOAT;          break;
    case NGL_NODE_BUFFERVEC3:   data_comp_size = 4; nb_comp = 3; format = VK_FORMAT_R32G32B32_SFLOAT;       break;
    case NGL_NODE_BUFFERVEC4:   data_comp_size = 4; nb_comp = 4; format = VK_FORMAT_R32G32B32A32_SFLOAT;    break;
    default:
        ngli_assert(0);
    }

    s->data_format = format;
#else
    GLenum comp_type;

    switch (node->class->id) {
    case NGL_NODE_BUFFERBYTE:   data_comp_size = 1; nb_comp = 1; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERBVEC2:  data_comp_size = 1; nb_comp = 2; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERBVEC3:  data_comp_size = 1; nb_comp = 3; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERBVEC4:  data_comp_size = 1; nb_comp = 4; comp_type = GL_BYTE;              break;
    case NGL_NODE_BUFFERINT:    data_comp_size = 4; nb_comp = 1; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERIVEC2:  data_comp_size = 4; nb_comp = 2; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERIVEC3:  data_comp_size = 4; nb_comp = 3; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERIVEC4:  data_comp_size = 4; nb_comp = 4; comp_type = GL_INT;               break;
    case NGL_NODE_BUFFERSHORT:  data_comp_size = 2; nb_comp = 1; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERSVEC2:  data_comp_size = 2; nb_comp = 2; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERSVEC3:  data_comp_size = 2; nb_comp = 3; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERSVEC4:  data_comp_size = 2; nb_comp = 4; comp_type = GL_SHORT;             break;
    case NGL_NODE_BUFFERUBYTE:  data_comp_size = 1; nb_comp = 1; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUBVEC2: data_comp_size = 1; nb_comp = 2; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUBVEC3: data_comp_size = 1; nb_comp = 3; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUBVEC4: data_comp_size = 1; nb_comp = 4; comp_type = GL_UNSIGNED_BYTE;     break;
    case NGL_NODE_BUFFERUINT:   data_comp_size = 4; nb_comp = 1; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUIVEC2: data_comp_size = 4; nb_comp = 2; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUIVEC3: data_comp_size = 4; nb_comp = 3; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUIVEC4: data_comp_size = 4; nb_comp = 4; comp_type = GL_UNSIGNED_INT;      break;
    case NGL_NODE_BUFFERUSHORT: data_comp_size = 2; nb_comp = 1; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERUSVEC2: data_comp_size = 2; nb_comp = 2; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERUSVEC3: data_comp_size = 2; nb_comp = 3; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERUSVEC4: data_comp_size = 2; nb_comp = 4; comp_type = GL_UNSIGNED_SHORT;    break;
    case NGL_NODE_BUFFERFLOAT:  data_comp_size = 4; nb_comp = 1; comp_type = GL_FLOAT;             break;
    case NGL_NODE_BUFFERVEC2:   data_comp_size = 4; nb_comp = 2; comp_type = GL_FLOAT;             break;
    case NGL_NODE_BUFFERVEC3:   data_comp_size = 4; nb_comp = 3; comp_type = GL_FLOAT;             break;
    case NGL_NODE_BUFFERVEC4:   data_comp_size = 4; nb_comp = 4; comp_type = GL_FLOAT;             break;
    default:
        ngli_assert(0);
    }

    s->data_comp_type = comp_type;
#endif

    s->data_comp = nb_comp;

    if (!s->data_stride)
        s->data_stride = s->data_comp * data_comp_size;

    if (s->data)
        ret = buffer_init_from_data(node);
    else if (s->filename)
        ret = buffer_init_from_filename(node);
    else
        ret = buffer_init_from_count(node);
    if (ret < 0)
        return ret;

    if (s->generate_gl_buffer) {
        struct ngl_ctx *ctx = node->ctx;

#ifdef VULKAN_BACKEND
        struct glcontext *vk = ctx->glcontext;

        VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = s->data_size,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                   | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                   | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, // XXX: allow more usage / filter them?
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if (vkCreateBuffer(vk->device, &buffer_create_info, NULL, &s->vkbuf) != VK_SUCCESS)
            return -1;

        /* alloc madness */
        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(vk->device, s->vkbuf, &mem_req);
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = mem_req.size,
            .memoryTypeIndex = find_memory_type(vk, mem_req.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        VkResult vkret = vkAllocateMemory(vk->device, &alloc_info, NULL, &s->vkmem);
        if (vkret != VK_SUCCESS)
            return -1;
        vkBindBufferMemory(vk->device, s->vkbuf, s->vkmem, 0);

        /* transfer cpu data to gpu with a memory map */
        void *mapped_mem;
        vkMapMemory(vk->device, s->vkmem, 0, s->data_size, 0, &mapped_mem);
        memcpy(mapped_mem, s->data, s->data_size);
        vkUnmapMemory(vk->device, s->vkmem);

#else
        struct glcontext *gl = ctx->glcontext;

        ngli_glGenBuffers(gl, 1, &s->buffer_id);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->buffer_id);
        ngli_glBufferData(gl, GL_ARRAY_BUFFER, s->data_size, s->data, s->usage);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, 0);
#endif
    }

    return 0;
}

static void buffer_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct buffer *s = node->priv_data;

    if (s->filename && s->fd) {
        int ret = close(s->fd);
        if (ret < 0) {
            LOG(ERROR, "could not properly close '%s'", s->filename);
        }
    }

#ifdef VULKAN_BACKEND
    struct glcontext *vk = ctx->glcontext;
    vkDestroyBuffer(vk->device, s->vkbuf, NULL);
    vkFreeMemory(vk->device, s->vkmem, NULL);
#else
    struct glcontext *gl = ctx->glcontext;
    ngli_glDeleteBuffers(gl, 1, &s->buffer_id);
#endif
}

#define DEFINE_BUFFER_CLASS(class_id, class_name, type)     \
const struct node_class ngli_buffer##type##_class = {       \
    .id        = class_id,                                  \
    .name      = class_name,                                \
    .init      = buffer_init,                               \
    .uninit    = buffer_uninit,                             \
    .priv_size = sizeof(struct buffer),                     \
    .params    = buffer_params,                             \
    .params_id = "Buffer",                                  \
    .file      = __FILE__,                                  \
};

DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBYTE,    "BufferByte",    byte)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC2,   "BufferBVec2",   bvec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC3,   "BufferBVec3",   bvec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERBVEC4,   "BufferBVec4",   bvec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERINT,     "BufferInt",     int)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC2,   "BufferIVec2",   ivec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC3,   "BufferIVec3",   ivec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERIVEC4,   "BufferIVec4",   ivec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSHORT,   "BufferShort",   short)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC2,   "BufferSVec2",   svec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC3,   "BufferSVec3",   svec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERSVEC4,   "BufferSVec4",   svec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBYTE,   "BufferUByte",   ubyte)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC2,  "BufferUBVec2",  ubvec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC3,  "BufferUBVec3",  ubvec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUBVEC4,  "BufferUBVec4",  ubvec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUINT,    "BufferUInt",    uint)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC2,  "BufferUIVec2",  uivec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC3,  "BufferUIVec3",  uivec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUIVEC4,  "BufferUIVec4",  uivec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSHORT,  "BufferUShort",  ushort)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC2,  "BufferUSVec2",  usvec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC3,  "BufferUSVec3",  usvec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERUSVEC4,  "BufferUSVec4",  usvec4)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERFLOAT,   "BufferFloat",   float)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC2,    "BufferVec2",    vec2)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC3,    "BufferVec3",    vec3)
DEFINE_BUFFER_CLASS(NGL_NODE_BUFFERVEC4,    "BufferVec4",    vec4)
