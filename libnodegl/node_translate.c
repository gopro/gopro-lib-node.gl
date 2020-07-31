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
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "transforms.h"

struct translate_priv {
    struct transform_priv trf;
    float vector[3];
    struct ngl_node *anim;
};

static void update_trf_matrix(struct ngl_node *node, const float *vec)
{
    struct translate_priv *s = node->priv_data;
    struct transform_priv *trf = &s->trf;
    ngli_mat4_translate(trf->matrix, vec[0], vec[1], vec[2]);
}

static int update_vector(struct ngl_node *node)
{
    struct translate_priv *s = node->priv_data;
    if (s->anim) {
        LOG(ERROR, "updating vector while the animation is set is unsupported");
        return NGL_ERROR_INVALID_USAGE;
    }
    update_trf_matrix(node, s->vector);
    return 0;
}

static int translate_init(struct ngl_node *node)
{
    struct translate_priv *s = node->priv_data;
    if (!s->anim)
        update_trf_matrix(node, s->vector);
    return 0;
}

static int translate_update(struct ngl_node *node, double t)
{
    struct translate_priv *s = node->priv_data;
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

#define OFFSET(x) offsetof(struct translate_priv, x)
static const struct node_param translate_params[] = {
    {"child",  PARAM_TYPE_NODE, OFFSET(trf.child),
               .flags=PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to translate")},
    {"vector", PARAM_TYPE_VEC3, OFFSET(vector),
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .update_func=update_vector,
               .desc=NGLI_DOCSTRING("translation vector")},
    {"anim",   PARAM_TYPE_NODE, OFFSET(anim),
               .node_types=(const int[]){NGL_NODE_ANIMATEDVEC3, NGL_NODE_STREAMEDVEC3, -1},
               .desc=NGLI_DOCSTRING("`vector` animation")},
    {NULL}
};

NGLI_STATIC_ASSERT(trf_on_top_of_translate, OFFSET(trf) == 0);

const struct node_class ngli_translate_class = {
    .id        = NGL_NODE_TRANSLATE,
    .name      = "Translate",
    .init      = translate_init,
    .update    = translate_update,
    .draw      = ngli_transform_draw,
    .priv_size = sizeof(struct translate_priv),
    .params    = translate_params,
    .file      = __FILE__,
};
