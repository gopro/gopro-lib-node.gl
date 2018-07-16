/*
 * Copyright 2018 GoPro Inc.
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

#ifndef BACKEND_H
#define BACKEND_H

#include "nodes.h"

struct backend {
    const char *name;
    int (*reconfigure)(struct ngl_ctx *s, const struct ngl_config *config);
    int (*configure)(struct ngl_ctx *s, const struct ngl_config *config);
    int (*pre_draw)(struct ngl_ctx *s);
    int (*post_draw)(struct ngl_ctx *s, double t, int ret);
    int (*destroy)(struct ngl_ctx *s);
    int int_cfg_dp; // set if internal config dispatch
};

#endif
