/*
 * Copyright 2016 GoPro Inc.
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
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define SET_INDICES(type, count, data) do {                                    \
    type *indices = (type *)(data);                                            \
    for (int i = 0; i < (count); i++) {                                        \
        indices[i] = i;                                                        \
    }                                                                          \
} while (0)

struct ngl_node *ngli_geometry_generate_buffer(struct ngl_ctx *ctx, int type, int count, int size, void *data)
{
    struct ngl_node *node = ngl_node_create(type, count);
    if (!node)
        return NULL;

    struct buffer *buffer = node->priv_data;
    buffer->generate_gl_buffer = 1;

    if (data)
        ngl_node_param_set(node, "data", size, data);

    int ret = ngli_node_attach_ctx(node, ctx);
    if (ret < 0)
        goto fail;

    ret = ngli_node_init(node);
    if (ret < 0)
        goto fail;

    return node;
fail:
    ngli_node_detach_ctx(node);
    ngl_node_unrefp(&node);
    return NULL;
}

struct ngl_node *ngli_geometry_generate_indices_buffer(struct ngl_ctx *ctx, int count)
{
    int size = count * sizeof(short);
    uint8_t *data = calloc(count, sizeof(short));
    if (!data)
        return NULL;

    SET_INDICES(short, count, data);

    struct ngl_node *node = ngli_geometry_generate_buffer(ctx,
                                                          NGL_NODE_BUFFERUSHORT,
                                                          count,
                                                          size,
                                                          data);
    free(data);
    return node;
}

//VK_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
//VK_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
//VK_PRIMITIVE_TOPOLOGY_LINE_STRIP = 2,
//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3,
//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4,
//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN = 5,
//VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 6,
//VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 7,
//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 8,
//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9,
//VK_PRIMITIVE_TOPOLOGY_PATCH_LIST = 10,

static const struct param_choices draw_mode_choices = {
    .name = "draw_mode",
    .consts = {
#ifdef VULKAN_BACKEND
        {"points",         VK_PRIMITIVE_TOPOLOGY_POINT_LIST,         .desc=NGLI_DOCSTRING("points")},
        {"line_strip",     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,         .desc=NGLI_DOCSTRING("line strip")},
        //{"line_loop",      VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,      .desc=NGLI_DOCSTRING("line loop")},
        {"lines",          VK_PRIMITIVE_TOPOLOGY_LINE_LIST,          .desc=NGLI_DOCSTRING("lines")},
        {"triangle_strip", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,     .desc=NGLI_DOCSTRING("triangle strip")},
        {"triangle_fan",   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,       .desc=NGLI_DOCSTRING("triangle fan")},
        {"triangles",      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,      .desc=NGLI_DOCSTRING("triangles")},
#else
        {"points",         GL_POINTS,         .desc=NGLI_DOCSTRING("points")},
        {"line_strip",     GL_LINE_STRIP,     .desc=NGLI_DOCSTRING("line strip")},
        {"line_loop",      GL_LINE_LOOP,      .desc=NGLI_DOCSTRING("line loop")},
        {"lines",          GL_LINES,          .desc=NGLI_DOCSTRING("lines")},
        {"triangle_strip", GL_TRIANGLE_STRIP, .desc=NGLI_DOCSTRING("triangle strip")},
        {"triangle_fan",   GL_TRIANGLE_FAN,   .desc=NGLI_DOCSTRING("triangle fan")},
        {"triangles",      GL_TRIANGLES,      .desc=NGLI_DOCSTRING("triangles")},
#endif
        {NULL}
    }
};

#define TEXCOORDS_TYPES_LIST (const int[]){NGL_NODE_BUFFERFLOAT,            \
                                           NGL_NODE_BUFFERVEC2,             \
                                           NGL_NODE_BUFFERVEC3,             \
                                           NGL_NODE_ANIMATEDBUFFERFLOAT,    \
                                           NGL_NODE_ANIMATEDBUFFERVEC2,     \
                                           NGL_NODE_ANIMATEDBUFFERVEC3,     \
                                           -1}

#define OFFSET(x) offsetof(struct geometry, x)
static const struct node_param geometry_params[] = {
    {"vertices",  PARAM_TYPE_NODE, OFFSET(vertices_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, -1},
                  .flags=PARAM_FLAG_CONSTRUCTOR | PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("vertice coordinates defining the geometry")},
    {"uvcoords",  PARAM_TYPE_NODE, OFFSET(uvcoords_buffer),
                  .node_types=TEXCOORDS_TYPES_LIST,
                  .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("coordinates used for UV mapping of each `vertices`")},
    {"normals",   PARAM_TYPE_NODE, OFFSET(normals_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, NGL_NODE_ANIMATEDBUFFERVEC3, -1},
                  .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("normal vectors of each `vertices`")},
    {"indices",   PARAM_TYPE_NODE, OFFSET(indices_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERUBYTE, NGL_NODE_BUFFERUINT, NGL_NODE_BUFFERUSHORT, -1},
                  .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("indices defining the drawing order of the `vertices`, auto-generated if not set")},
    // XXX: rename to "topology"?
#ifdef VULKAN_BACKEND
    {"draw_mode", PARAM_TYPE_SELECT, OFFSET(draw_mode), {.i64=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
                  .choices=&draw_mode_choices,
                  .desc=NGLI_DOCSTRING("drawing mode")},
#else
    {"draw_mode", PARAM_TYPE_SELECT, OFFSET(draw_mode), {.i64=GL_TRIANGLES},
                  .choices=&draw_mode_choices,
                  .desc=NGLI_DOCSTRING("drawing mode")},
#endif
    {NULL}
};

static int geometry_init(struct ngl_node *node)
{
    struct geometry *s = node->priv_data;

    struct buffer *vertices = s->vertices_buffer->priv_data;
    vertices->generate_gl_buffer = 1;

    int ret = ngli_node_init(s->vertices_buffer);
    if (ret < 0)
        return ret;

    if (s->uvcoords_buffer) {
        struct buffer *buffer = s->uvcoords_buffer->priv_data;
        buffer->generate_gl_buffer = 1;

        int ret = ngli_node_init(s->uvcoords_buffer);
        if (ret < 0)
            return ret;

        if (buffer->count != vertices->count) {
            LOG(ERROR,
                "uvcoords count (%d) does not match vertices count (%d)",
                buffer->count,
                vertices->count);
            return -1;
        }
    }

    if (s->normals_buffer) {
        struct buffer *buffer = s->normals_buffer->priv_data;
        buffer->generate_gl_buffer = 1;

        int ret = ngli_node_init(s->normals_buffer);
        if (ret < 0)
            return ret;

        if (buffer->count != vertices->count) {
            LOG(ERROR,
                "normals count (%d) does not match vertices count (%d)",
                buffer->count,
                vertices->count);
            return -1;
        }
    }

    if (s->indices_buffer) {
        struct buffer *buffer = s->indices_buffer->priv_data;
        buffer->generate_gl_buffer = 1;

        int ret = ngli_node_init(s->indices_buffer);
        if (ret < 0)
            return ret;
    } else {
        s->indices_buffer = ngli_geometry_generate_indices_buffer(node->ctx,
                                                                  vertices->count);
        if (!s->indices_buffer)
            return -1;
    }

    return 0;
}

static int geometry_update(struct ngl_node *node, double t)
{
    struct geometry *s = node->priv_data;

    int ret = ngli_node_update(s->vertices_buffer, t);
    if (ret < 0)
        return ret;

    if (s->uvcoords_buffer) {
        ret = ngli_node_update(s->uvcoords_buffer, t);
        if (ret < 0)
            return ret;
    }

    if (s->normals_buffer) {
        ret = ngli_node_update(s->normals_buffer, t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

const struct node_class ngli_geometry_class = {
    .id        = NGL_NODE_GEOMETRY,
    .name      = "Geometry",
    .init      = geometry_init,
    .update    = geometry_update,
    .priv_size = sizeof(struct geometry),
    .params    = geometry_params,
    .file      = __FILE__,
};
