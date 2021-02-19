/*
 * Copyright 2021 GoPro Inc.
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
#include <string.h>

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "path.h"

struct smoothpath_priv {
    struct path *path;

    struct ngl_node *points_buffer;
    float control1[3];
    float control2[3];
    int precision;
    double tension;
};

#define OFFSET(x) offsetof(struct smoothpath_priv, x)
static const struct node_param smoothpath_params[] = {
    {"points",    NGLI_PARAM_TYPE_NODE, OFFSET(points_buffer),
                  .node_types=(const int[]){NGL_NODE_BUFFERVEC3, -1},
                  .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                  .desc=NGLI_DOCSTRING("anchor points the path go through")},
    {"control1",  NGLI_PARAM_TYPE_VEC3, OFFSET(control1),
                  .desc=NGLI_DOCSTRING("initial control point")},
    {"control2",  NGLI_PARAM_TYPE_VEC3, OFFSET(control2),
                  .desc=NGLI_DOCSTRING("final control point")},
    {"precision", NGLI_PARAM_TYPE_INT, OFFSET(precision), {.i64=64},
                  .desc=NGLI_DOCSTRING("number of divisions per curve segment")},
    {"tension",   NGLI_PARAM_TYPE_DBL, OFFSET(tension), {.dbl=0.5},
                  .desc=NGLI_DOCSTRING("tension between points")},
    {NULL}
};

/* We must have the struct path in 1st position for AnimatedPath */
NGLI_STATIC_ASSERT(path_1st_field, OFFSET(path) == 0);

static int smoothpath_init(struct ngl_node *node)
{
    struct smoothpath_priv *s = node->priv_data;

    if (s->tension <= 0) {
        LOG(ERROR, "tension must be strictly positive");
        return NGL_ERROR_INVALID_ARG;
    }

    const struct buffer_priv *points = s->points_buffer->priv_data;

    if (points->count < 2) {
        LOG(ERROR, "at least 2 points must be defined");
        return NGL_ERROR_INVALID_ARG;
    }

    s->path = ngli_path_create();
    if (!s->path)
        return NGL_ERROR_MEMORY;

    const float *p = (const float *)points->data;
    int ret = ngli_path_move_to(s->path, p); // initial starting point
    if (ret < 0)
        return ret;

    /*
     * The following code translates the catmull-rom user points and controls
     * into bezier cubic curves
     *
     * See https://pomax.github.io/bezierinfo/#catmullconv
     */
    const float scale = 1.f / (s->tension * 6.f);
    const int nb_segments = points->count - 1;
    for (int i = 0; i < nb_segments; i++) {
        const float *p0 = i == 0 ? s->control1 : &p[(i - 1) * 3];
        const float *p1 = &p[i * 3];
        const float *p2 = &p[(i + 1) * 3];
        const float *p3 = i == nb_segments - 1 ? s->control2 : &p[(i + 2) * 3];
        const float bezier3_control1[3] = {
            p1[0] + (p2[0] - p0[0]) * scale,
            p1[1] + (p2[1] - p0[1]) * scale,
            p1[2] + (p2[2] - p0[2]) * scale,
        };
        const float bezier3_control2[3] = {
            p2[0] - (p3[0] - p1[0]) * scale,
            p2[1] - (p3[1] - p1[1]) * scale,
            p2[2] - (p3[2] - p1[2]) * scale,
        };
        ret = ngli_path_bezier3_to(s->path, bezier3_control1, bezier3_control2, p2);
        if (ret < 0)
            return ret;
    }

    return ngli_path_init(s->path, s->precision);
}

static void smoothpath_uninit(struct ngl_node *node)
{
    struct smoothpath_priv *s = node->priv_data;
    ngli_path_freep(&s->path);
}

const struct node_class ngli_smoothpath_class = {
    .id        = NGL_NODE_SMOOTHPATH,
    .name      = "SmoothPath",
    .init      = smoothpath_init,
    .uninit    = smoothpath_uninit,
    .priv_size = sizeof(struct smoothpath_priv),
    .params    = smoothpath_params,
    .file      = __FILE__,
};
