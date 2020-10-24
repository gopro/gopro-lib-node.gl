/*
 * Copyright 2017-2019 GoPro Inc.
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

#ifndef ANIMATION_H
#define ANIMATION_H

#include "nodegl.h"

#ifdef __cplusplus
extern "C" {
#endif

struct animkeyframe_priv;

typedef void (*ngli_animation_mix_func_type)(void *user_arg, void *dst,
                                             const struct animkeyframe_priv *kf0,
                                             const struct animkeyframe_priv *kf1,
                                             double ratio);

typedef void (*ngli_animation_cpy_func_type)(void *user_arg, void *dst,
                                             const struct animkeyframe_priv *kf);

struct animation {
    struct ngl_node * const *kfs;
    int nb_kfs;
    int current_kf;
    void *user_arg;
    ngli_animation_mix_func_type mix_func;
    ngli_animation_cpy_func_type cpy_func;
};

int ngli_animation_init(struct animation *s, void *user_arg,
                        struct ngl_node * const *kfs, int nb_kfs,
                        ngli_animation_mix_func_type mix_func,
                        ngli_animation_cpy_func_type cpy_func);

int ngli_animation_evaluate(struct animation *s, void *dst, double t);

#ifdef __cplusplus
}
#endif

#endif
