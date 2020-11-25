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
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct program_priv, x)
static const struct node_param computeprogram_params[] = {
    {"compute", PARAM_TYPE_STR, OFFSET(compute), .flags=PARAM_FLAG_NON_NULL,
                .desc=NGLI_DOCSTRING("compute shader")},
    {"workgroup_size", PARAM_TYPE_IVEC3, OFFSET(workgroup_size),
                       .desc=NGLI_DOCSTRING("number of local compute instances in a work group")},
    {"properties", PARAM_TYPE_NODEDICT, OFFSET(properties),
                   .node_types=(const int[]){NGL_NODE_RESOURCEPROPS, -1},
                   .desc=NGLI_DOCSTRING("resource properties")},
    {NULL}
};

static int computeprogram_init(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;

    if (s->workgroup_size[0] <= 0 || s->workgroup_size[1] <= 0 || s->workgroup_size[2] <= 0) {
        LOG(ERROR, "work group size must be > 0 for x, y and z");
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

const struct node_class ngli_computeprogram_class = {
    .id        = NGL_NODE_COMPUTEPROGRAM,
    .name      = "ComputeProgram",
    .priv_size = sizeof(struct program_priv),
    .params    = computeprogram_params,
    .init      = computeprogram_init,
    .file      = __FILE__,
};
