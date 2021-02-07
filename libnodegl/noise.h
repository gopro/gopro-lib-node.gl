/*
 * Copyright 2021 GoPro Inc.
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

#ifndef NOISE_H
#define NOISE_H

#include <stdint.h>

enum {
    NGLI_NOISE_LINEAR,
    NGLI_NOISE_CUBIC,
    NGLI_NOISE_QUINTIC,
    NGLI_NOISE_NB
};

typedef float (*interp_func_type)(float t);

struct noise_params {
    double amplitude;
    int octaves;
    double lacunarity;
    double gain;
    uint32_t seed;
    int function;
};

struct noise {
    struct noise_params params;
    interp_func_type interp_func;
};

int ngli_noise_init(struct noise *s, const struct noise_params *params);
float ngli_noise_get(const struct noise *s, float t);

#endif
