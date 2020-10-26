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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "topology.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct geometry_priv, x)
static const struct node_param quad_params[] = {
    {"corner",    PARAM_TYPE_VEC3, OFFSET(quad_corner),    {.vec={-0.5f, -0.5f}},
                  .desc=NGLI_DOCSTRING("origin coordinates of `width` and `height` vectors")},
    {"width",     PARAM_TYPE_VEC3, OFFSET(quad_width),     {.vec={ 1.0f,  0.0f}},
                  .desc=NGLI_DOCSTRING("width vector")},
    {"height",    PARAM_TYPE_VEC3, OFFSET(quad_height),    {.vec={ 0.0f,  1.0f}},
                  .desc=NGLI_DOCSTRING("height vector")},
    {"uv_corner", PARAM_TYPE_VEC2, OFFSET(quad_uv_corner), {.vec={0.0f, 0.0f}},
                  .desc=NGLI_DOCSTRING("origin coordinates of `uv_width` and `uv_height` vectors")},
    {"uv_width",  PARAM_TYPE_VEC2, OFFSET(quad_uv_width),  {.vec={1.0f, 0.0f}},
                  .desc=NGLI_DOCSTRING("UV coordinates width vector")},
    {"uv_height", PARAM_TYPE_VEC2, OFFSET(quad_uv_height), {.vec={0.0f, 1.0f}},
                  .desc=NGLI_DOCSTRING("UV coordinates height vector")},
    {NULL}
};

#define NB_VERTICES 4

#define C(index) s->quad_corner[(index)]
#define W(index) s->quad_width[(index)]
#define H(index) s->quad_height[(index)]

#define UV_C(index) s->quad_uv_corner[(index)]
#define UV_W(index) s->quad_uv_width[(index)]
#define UV_H(index) s->quad_uv_height[(index)]

static int quad_init(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;

    const float vertices[] = {
        C(0),               C(1),               C(2),
        C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
        C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
        C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
    };

    const float uvs[] = {
        UV_C(0),                     1.0f - UV_C(1),
        UV_C(0) + UV_W(0),           1.0f - UV_C(1) - UV_W(1),
        UV_C(0) + UV_H(0),           1.0f - UV_C(1) - UV_H(1),
        UV_C(0) + UV_H(0) + UV_W(0), 1.0f - UV_C(1) - UV_H(1) - UV_W(1),
    };

    s->vertices_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                            NGL_NODE_BUFFERVEC3,
                                                            NB_VERTICES,
                                                            sizeof(vertices),
                                                            (void *)vertices);
    if (!s->vertices_buffer)
        return NGL_ERROR_MEMORY;

    s->uvcoords_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                            NGL_NODE_BUFFERVEC2,
                                                            NB_VERTICES,
                                                            sizeof(uvs),
                                                            (void *)uvs);
    if (!s->uvcoords_buffer)
        return NGL_ERROR_MEMORY;

    float normals[3 * NB_VERTICES];
    ngli_vec3_normalvec(normals, vertices, vertices + 3, vertices + 6);

    for (int i = 1; i < NB_VERTICES; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    s->normals_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                           NGL_NODE_BUFFERVEC3,
                                                           NB_VERTICES,
                                                           sizeof(normals),
                                                           normals);
    if (!s->normals_buffer)
        return NGL_ERROR_MEMORY;

    s->topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    return 0;
}

#define NODE_UNREFP(node) do {                    \
    if (node) {                                   \
        ngli_node_detach_ctx(node, node->ctx);    \
        ngl_node_unrefp(&node);                   \
    }                                             \
} while (0)

static void quad_uninit(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;

    NODE_UNREFP(s->vertices_buffer);
    NODE_UNREFP(s->uvcoords_buffer);
    NODE_UNREFP(s->normals_buffer);
}

const struct node_class ngli_quad_class = {
    .id        = NGL_NODE_QUAD,
    .name      = "Quad",
    .init      = quad_init,
    .uninit    = quad_uninit,
    .priv_size = sizeof(struct geometry_priv),
    .params    = quad_params,
    .file      = __FILE__,
};
