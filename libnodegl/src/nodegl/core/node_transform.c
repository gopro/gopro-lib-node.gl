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

#include <stddef.h>
#include <stdio.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "transforms.h"

#define OFFSET(x) offsetof(struct transform_priv, x)
static const struct node_param transform_params[] = {
    {"child",  PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to apply the transform to")},
    {"matrix", PARAM_TYPE_MAT4, OFFSET(matrix), {.mat=NGLI_MAT4_IDENTITY},
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("transformation matrix")},
    {NULL}
};

static int transform_update(struct ngl_node *node, double t)
{
    struct transform_priv *s = node->priv_data;
    struct ngl_node *child = s->child;
    return ngli_node_update(child, t);
}

const struct node_class ngli_transform_class = {
    .id        = NGL_NODE_TRANSFORM,
    .name      = "Transform",
    .update    = transform_update,
    .draw      = ngli_transform_draw,
    .priv_size = sizeof(struct transform_priv),
    .params    = transform_params,
    .file      = __FILE__,
};
