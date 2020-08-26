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

#include <stdlib.h>
#include <string.h>

#include "rnode.h"

void ngli_rnode_init(struct rnode *s)
{
    memset(s, 0, sizeof(*s));
    ngli_darray_init(&s->children, sizeof(*s), 0);
}

void ngli_rnode_clear(struct rnode *s)
{
    ngli_rnode_reset(s);
    ngli_rnode_init(s);
}

void ngli_rnode_reset(struct rnode *s)
{
    struct rnode *children = ngli_darray_data(&s->children);
    for (int i = 0; i < ngli_darray_count(&s->children); i++)
        ngli_rnode_reset(&children[i]);
    ngli_darray_reset(&s->children);
    memset(s, 0, sizeof(*s));
}

struct rnode *ngli_rnode_add_child(struct rnode *s)
{
    struct rnode *child = ngli_darray_push(&s->children, NULL);
    ngli_rnode_init(child);
    child->graphicstate = s->graphicstate;
    child->rendertarget_desc = s->rendertarget_desc;
    return child;
}
