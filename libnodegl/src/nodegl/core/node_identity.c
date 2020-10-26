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

#include <string.h>

#include "darray.h"
#include "nodegl.h"
#include "nodes.h"

static void identity_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct identity_priv *s = node->priv_data;
    const float *matrix = ngli_darray_tail(&ctx->modelview_matrix_stack);
    memcpy(s->modelview_matrix, matrix, sizeof(s->modelview_matrix));
}

const struct node_class ngli_identity_class = {
    .id        = NGL_NODE_IDENTITY,
    .name      = "Identity",
    .draw      = identity_draw,
    .priv_size = sizeof(struct identity_priv),
    .file      = __FILE__,
};
