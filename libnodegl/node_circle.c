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

#include <math.h>
#include <stddef.h>
#include <string.h>
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct geometry, x)
static const struct node_param circle_params[] = {
    {"radius",  PARAM_TYPE_DBL, OFFSET(radius),  {.dbl=1.0},
                .desc=NGLI_DOCSTRING("circle radius")},
    {"npoints", PARAM_TYPE_INT, OFFSET(npoints), {.i64=16},
                .desc=NGLI_DOCSTRING("number of points")},
    {NULL}
};

static int circle_init(struct ngl_node *node)
{
    int ret = -1;
    struct geometry *s = node->priv_data;

    if (s->npoints < 3) {
        LOG(ERROR, "invalid number of points (%d < 3)", s->npoints);
        return -1;
    }
    const int nb_vertices = s->npoints + 2;

    float *vertices  = calloc(nb_vertices, sizeof(*vertices)  * 3);
    float *uvcoords  = calloc(nb_vertices, sizeof(*uvcoords)  * 2);
    float *normals   = calloc(nb_vertices, sizeof(*normals)   * 3);

    if (!vertices || !uvcoords || !normals)
        goto end;


    int i;
    const double step = 2.0 * M_PI / s->npoints;

    uvcoords[0] = 0.5;
    uvcoords[1] = 0.5;
    for (i = 1; i < (nb_vertices - 1); i++) {
        const double angle = (i - 1) * step;
        const double x = sin(angle) * s->radius;
        const double y = cos(angle) * s->radius;
        vertices[i*3 + 0] = x;
        vertices[i*3 + 1] = y;
        uvcoords[i*2 + 0] = (x + 1.0) / 2.0;
        uvcoords[i*2 + 1] = (1.0 - y) / 2.0;
    }
    vertices[i*3 + 0] = vertices[3 + 0];
    vertices[i*3 + 1] = vertices[3 + 1];
    uvcoords[i*2 + 0] = uvcoords[2 + 0];
    uvcoords[i*2 + 1] = uvcoords[2 + 1];

    static const float center[3] = {0};
    ngli_vec3_normalvec(normals, (float *)center, vertices, vertices + 3);
    for (int i = 1; i < nb_vertices; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    s->vertices_buffer = ngli_geometry_generate_buffer(node->ctx,
                                                       NGL_NODE_BUFFERVEC3,
                                                       nb_vertices,
                                                       nb_vertices * sizeof(*vertices) * 3,
                                                       vertices);

    s->uvcoords_buffer = ngli_geometry_generate_buffer(node->ctx,
                                                       NGL_NODE_BUFFERVEC2,
                                                       nb_vertices,
                                                       nb_vertices * sizeof(*uvcoords) * 2,
                                                       uvcoords);

    s->normals_buffer = ngli_geometry_generate_buffer(node->ctx,
                                                      NGL_NODE_BUFFERVEC3,
                                                      nb_vertices,
                                                      nb_vertices * sizeof(*normals) * 3,
                                                      normals);

    s->indices_buffer = ngli_geometry_generate_indices_buffer(node->ctx, nb_vertices);

    if (!s->vertices_buffer || !s->uvcoords_buffer || !s->indices_buffer || !s->normals_buffer)
        goto end;

#ifdef VULKAN_BACKEND
        // TODO
#else
    s->draw_mode = GL_TRIANGLE_FAN;
#endif

    ret = 0;

end:
    free(vertices);
    free(uvcoords);
    free(normals);
    return ret;
}

#define NODE_UNREFP(node) do {                    \
    if (node) {                                   \
        ngli_node_detach_ctx(node);               \
        ngl_node_unrefp(&node);                   \
    }                                             \
} while (0)

static void circle_uninit(struct ngl_node *node)
{
    struct geometry *s = node->priv_data;

    NODE_UNREFP(s->vertices_buffer);
    NODE_UNREFP(s->uvcoords_buffer);
    NODE_UNREFP(s->normals_buffer);
    NODE_UNREFP(s->indices_buffer);
}

const struct node_class ngli_circle_class = {
    .id        = NGL_NODE_CIRCLE,
    .name      = "Circle",
    .init      = circle_init,
    .uninit    = circle_uninit,
    .priv_size = sizeof(struct geometry),
    .params    = circle_params,
    .file      = __FILE__,
};
