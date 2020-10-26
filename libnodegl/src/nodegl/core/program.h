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

#ifndef PROGRAM_H
#define PROGRAM_H

#include "hmap.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gctx;

#define MAX_ID_LEN 128

struct program_variable_info {
    int binding;
    int location;
    int stages;
    int access;
};

enum {
    NGLI_PROGRAM_SHADER_VERT,
    NGLI_PROGRAM_SHADER_FRAG,
    NGLI_PROGRAM_SHADER_COMP,
    NGLI_PROGRAM_SHADER_NB
};

struct program {
    struct gctx *gctx;
    struct hmap *uniforms;
    struct hmap *attributes;
    struct hmap *buffer_blocks;
};

struct program *ngli_program_create(struct gctx *gctx);
int ngli_program_init(struct program *s, const char *vertex, const char *fragment, const char *compute);
void ngli_program_freep(struct program **sp);

#ifdef __cplusplus
}
#endif

#endif
