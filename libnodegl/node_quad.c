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
#include "glincludes.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"
#include "log.h"

#define OFFSET(x) offsetof(struct geometry, x)
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
    LOG(ERROR, "quad init");
    struct geometry *s = node->priv_data;

    const GLfloat vertices[] = {
        C(0),               C(1),               C(2),
        C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
        C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
        C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
    };

    const GLfloat uvs[] = {
        UV_C(0),                     1.0f - UV_C(1),
        UV_C(0) + UV_W(0),           1.0f - UV_C(1) - UV_W(1),
        UV_C(0) + UV_H(0) + UV_W(0), 1.0f - UV_C(1) - UV_H(1) - UV_W(1),
        UV_C(0) + UV_H(0),           1.0f - UV_C(1) - UV_H(1),
    };

    s->vertices_buffer = ngli_geometry_generate_buffer(node->ctx,
                                                       NGL_NODE_BUFFERVEC3,
                                                       NB_VERTICES,
                                                       sizeof(vertices),
                                                       (void *)vertices);
    if (!s->vertices_buffer) {
        LOG(ERROR, "no vertices");
        return -1;
    }

    s->uvcoords_buffer = ngli_geometry_generate_buffer(node->ctx,
                                                       NGL_NODE_BUFFERVEC2,
                                                       NB_VERTICES,
                                                       sizeof(uvs),
                                                       (void *)uvs);
    if (!s->uvcoords_buffer) {
        LOG(ERROR, "no uv coords");
        return -1;
    }

    float normals[3 * NB_VERTICES];
    ngli_vec3_normalvec(normals,
                        (GLfloat *)s->triangle_edges,
                        (GLfloat *)s->triangle_edges + 3,
                        (GLfloat *)s->triangle_edges + 6);

    for (int i = 1; i < NB_VERTICES; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    s->normals_buffer = ngli_geometry_generate_buffer(node->ctx,
                                                      NGL_NODE_BUFFERVEC3,
                                                      NB_VERTICES,
                                                      sizeof(normals),
                                                      normals);
    if (!s->normals_buffer) {
        LOG(ERROR, "no normals");
        return -1;
    }


#ifndef VULKAN_BACKEND
    // FIXME
    s->indices_buffer = ngli_geometry_generate_indices_buffer(node->ctx,
                                                              NB_VERTICES);
    if (!s->indices_buffer) {
        LOG(ERROR, "no indices");
        return -1;
    }
#endif

#ifndef VULKAN_BACKEND
    s->draw_mode = GL_TRIANGLE_FAN;
#endif

    return 0;
}

#define NODE_UNREFP(node) do {                    \
    if (node) {                                   \
        ngli_node_detach_ctx(node);               \
        ngl_node_unrefp(&node);                   \
    }                                             \
} while (0)

static void quad_uninit(struct ngl_node *node)
{
    struct geometry *s = node->priv_data;

    NODE_UNREFP(s->vertices_buffer);
    NODE_UNREFP(s->uvcoords_buffer);
    NODE_UNREFP(s->normals_buffer);
    NODE_UNREFP(s->indices_buffer);
}

const struct node_class ngli_quad_class = {
    .id        = NGL_NODE_QUAD,
    .name      = "Quad",
    .init      = quad_init,
    .uninit    = quad_uninit,
    .priv_size = sizeof(struct geometry),
    .params    = quad_params,
    .file      = __FILE__,
};
