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

#ifndef RNODE_H
#define RNODE_H

#include "darray.h"
#include "graphicstate.h"
#include "rendertarget.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rnode {
    int id;
    struct graphicstate graphicstate;
    struct rendertarget_desc rendertarget_desc;
    struct darray children;
};

void ngli_rnode_init(struct rnode *s);
void ngli_rnode_clear(struct rnode *s);
void ngli_rnode_reset(struct rnode *s);
struct rnode *ngli_rnode_add_child(struct rnode *s);

#ifdef __cplusplus
}
#endif

#endif
