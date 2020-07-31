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
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "transforms.h"

struct scale_priv {
    struct transform_priv trf;
    float factors[3];
    float anchor[3];
    struct ngl_node *anim;
    int use_anchor;
};

static void update_trf_matrix(struct ngl_node *node, const float *f)
{
    struct scale_priv *s = node->priv_data;
    struct transform_priv *trf = &s->trf;
    float *matrix = trf->matrix;

    ngli_mat4_scale(matrix, f[0], f[1], f[2]);

    if (s->use_anchor) {
        const float *a = s->anchor;
        NGLI_ALIGNED_MAT(tm);
        ngli_mat4_translate(tm, a[0], a[1], a[2]);
        ngli_mat4_mul(matrix, tm, matrix);
        ngli_mat4_translate(tm, -a[0], -a[1], -a[2]);
        ngli_mat4_mul(matrix, matrix, tm);
    }
}

static int scale_init(struct ngl_node *node)
{
    struct scale_priv *s = node->priv_data;
    static const float zero_anchor[3] = {0};
    s->use_anchor = memcmp(s->anchor, zero_anchor, sizeof(s->anchor));
    if (!s->anim)
        update_trf_matrix(node, s->factors);
    return 0;
}

static int update_factors(struct ngl_node *node)
{
    struct scale_priv *s = node->priv_data;
    if (s->anim) {
        LOG(ERROR, "updating factors while the animation is set is unsupported");
        return NGL_ERROR_INVALID_USAGE;
    }
    update_trf_matrix(node, s->factors);
    return 0;
}

static int scale_update(struct ngl_node *node, double t)
{
    struct scale_priv *s = node->priv_data;
    struct transform_priv *trf = &s->trf;
    struct ngl_node *child = trf->child;
    if (s->anim) {
        struct ngl_node *anim_node = s->anim;
        struct variable_priv *anim = anim_node->priv_data;
        int ret = ngli_node_update(anim_node, t);
        if (ret < 0)
            return ret;
        update_trf_matrix(node, anim->vector);
    }
    return ngli_node_update(child, t);
}

#define OFFSET(x) offsetof(struct scale_priv, x)
static const struct node_param scale_params[] = {
    {"child",   PARAM_TYPE_NODE, OFFSET(trf.child),
                .flags=PARAM_FLAG_NON_NULL,
                .desc=NGLI_DOCSTRING("scene to scale")},
    {"factors", PARAM_TYPE_VEC3, OFFSET(factors),
                {.vec={1.0, 1.0, 1.0}},
                .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
                .update_func=update_factors,
                .desc=NGLI_DOCSTRING("scaling factors (how much to scale on each axis)")},
    {"anchor",  PARAM_TYPE_VEC3, OFFSET(anchor),
                .desc=NGLI_DOCSTRING("vector to the center point of the scale")},
    {"anim",    PARAM_TYPE_NODE, OFFSET(anim),
                .node_types=(const int[]){NGL_NODE_ANIMATEDVEC3, NGL_NODE_STREAMEDVEC3, -1},
                .desc=NGLI_DOCSTRING("`factors` animation")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_scale, OFFSET(trf) == 0);

const struct node_class ngli_scale_class = {
    .id        = NGL_NODE_SCALE,
    .name      = "Scale",
    .init      = scale_init,
    .update    = scale_update,
    .draw      = ngli_transform_draw,
    .priv_size = sizeof(struct scale_priv),
    .params    = scale_params,
    .file      = __FILE__,
};
