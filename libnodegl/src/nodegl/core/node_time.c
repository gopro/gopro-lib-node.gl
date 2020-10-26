/*
 * Copyright 2020 GoPro Inc.
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

#include "nodegl.h"
#include "nodes.h"
#include "type.h"

static int time_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = &s->scalar;
    s->data_size = sizeof(s->scalar);
    s->data_type = NGLI_TYPE_FLOAT;
    s->dynamic = 1;
    return 0;
}

static int time_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    s->scalar = t;
    return 0;
}

const struct node_class ngli_time_class = {
    .id        = NGL_NODE_TIME,
    .category  = NGLI_NODE_CATEGORY_UNIFORM,
    .name      = "Time",
    .init      = time_init,
    .update    = time_update,
    .priv_size = sizeof(struct variable_priv),
    .file      = __FILE__,
};
