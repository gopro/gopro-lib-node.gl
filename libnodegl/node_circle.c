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
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "topology.h"
#include "utils.h"

#define OFFSET(x) offsetof(struct geometry_priv, x)
static const struct node_param circle_params[] = {
    {"radius",  PARAM_TYPE_DBL, OFFSET(radius),  {.dbl=1.0},
                .desc=NGLI_DOCSTRING("circle radius")},
    {"npoints", PARAM_TYPE_INT, OFFSET(npoints), {.i64=16},
                .desc=NGLI_DOCSTRING("number of points")},
    {NULL}
};

static int circle_init(struct ngl_node *node)
{
    int ret = 0;
    struct geometry_priv *s = node->priv_data;

    if (s->npoints < 3) {
        LOG(ERROR, "invalid number of points (%d < 3)", s->npoints);
        return NGL_ERROR_INVALID_ARG;
    }
    const int nb_vertices = s->npoints + 1;
    const int nb_indices  = s->npoints * 3;

    float *vertices  = ngli_calloc(nb_vertices, sizeof(*vertices)  * 3);
    float *uvcoords  = ngli_calloc(nb_vertices, sizeof(*uvcoords)  * 2);
    float *normals   = ngli_calloc(nb_vertices, sizeof(*normals)   * 3);
    uint16_t *indices = ngli_calloc(nb_indices, sizeof(*indices));

    if (!vertices || !uvcoords || !normals || !indices) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    const double step = 2.0 * M_PI / s->npoints;

    vertices[0] = 0.0;
    vertices[1] = 0.0;
    uvcoords[0] = 0.5;
    uvcoords[1] = 0.5;
    for (int i = 1; i < nb_vertices; i++) {
        const double angle = (i - 1) * -step;
        const double x = sin(angle) * s->radius;
        const double y = cos(angle) * s->radius;
        vertices[i*3 + 0] = x;
        vertices[i*3 + 1] = y;
        uvcoords[i*2 + 0] = (x + 1.0) / 2.0;
        uvcoords[i*2 + 1] = (1.0 - y) / 2.0;
        indices[(i - 1) * 3 + 0]  = 0; // point to center coordinate
        indices[(i - 1) * 3 + 1]  = i;
        indices[(i - 1) * 3 + 2]  = i + 1;
    }
    /* Fix overflowing vertex reference back to the start for sealing the
     * circle */
    indices[nb_indices - 1] = 1;

    ngli_vec3_normalvec(normals, vertices, vertices + 3, vertices + 6);
    for (int i = 1; i < nb_vertices; i++)
        memcpy(normals + (i * 3), normals, 3 * sizeof(*normals));

    s->vertices_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                            NGL_NODE_BUFFERVEC3,
                                                            nb_vertices,
                                                            nb_vertices * sizeof(*vertices) * 3,
                                                            vertices);

    s->uvcoords_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                            NGL_NODE_BUFFERVEC2,
                                                            nb_vertices,
                                                            nb_vertices * sizeof(*uvcoords) * 2,
                                                            uvcoords);

    s->normals_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                           NGL_NODE_BUFFERVEC3,
                                                           nb_vertices,
                                                           nb_vertices * sizeof(*normals) * 3,
                                                           normals);

    s->indices_buffer = ngli_node_geometry_generate_buffer(node->ctx,
                                                           NGL_NODE_BUFFERUSHORT,
                                                           nb_indices,
                                                           nb_indices * sizeof(*indices),
                                                           (void *)indices);

    if (!s->vertices_buffer || !s->uvcoords_buffer || !s->normals_buffer || !s->indices_buffer) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    s->topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

end:
    ngli_free(vertices);
    ngli_free(uvcoords);
    ngli_free(normals);
    ngli_free(indices);
    return ret;
}

#define NODE_UNREFP(node) do {                    \
    if (node) {                                   \
        ngli_node_detach_ctx(node, node->ctx);    \
        ngl_node_unrefp(&node);                   \
    }                                             \
} while (0)

static void circle_uninit(struct ngl_node *node)
{
    struct geometry_priv *s = node->priv_data;

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
    .priv_size = sizeof(struct geometry_priv),
    .params    = circle_params,
    .file      = __FILE__,
};
