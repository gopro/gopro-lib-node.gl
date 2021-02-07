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

#include <stddef.h>
#include <limits.h>

#include "nodegl.h"
#include "nodes.h"
#include "noise.h"
#include "type.h"

struct noise_priv {
    struct variable_priv var;
    double frequency;
    struct noise_params generator_params;
    struct noise generator[4];
};

const struct param_choices noise_func_choices = {
    .name = "interp_noise",
    .consts = {
        {"linear",  NGLI_NOISE_LINEAR,  .desc=NGLI_DOCSTRING("linear interpolation (not recommended), f(t)=t")},
        {"cubic",   NGLI_NOISE_CUBIC,   .desc=NGLI_DOCSTRING("cubic hermite curve, f(t)=3t²-2t³")},
        {"quintic", NGLI_NOISE_QUINTIC, .desc=NGLI_DOCSTRING("quintic curve, f(t)=6t⁵-15t⁴+10t³")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct noise_priv, x)
static const struct node_param noise_params[] = {
    {"frequency",   NGLI_PARAM_TYPE_DBL, OFFSET(frequency), {.dbl=1.},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("oscillation per second")},
    {"amplitude",   NGLI_PARAM_TYPE_DBL, OFFSET(generator_params.amplitude), {.dbl=1.},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("by how much it oscillates")},
    {"octaves",     NGLI_PARAM_TYPE_INT, OFFSET(generator_params.octaves), {.i64=3},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("number of accumulated noise layers (controls the level of details)")},
    {"lacunarity",  NGLI_PARAM_TYPE_DBL, OFFSET(generator_params.lacunarity), {.dbl=2.0},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("frequency multiplier per octave")},
    {"gain",        NGLI_PARAM_TYPE_DBL, OFFSET(generator_params.gain), {.dbl=0.5},
                    .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("amplitude multiplier per octave (also known as persistence)")},
    {"seed",        NGLI_PARAM_TYPE_UINT, OFFSET(generator_params.seed), {.i64=0},
                    .desc=NGLI_DOCSTRING("random base seed (acts as an offsetting to the time)")},
    {"interpolant", NGLI_PARAM_TYPE_SELECT, OFFSET(generator_params.function), {.i64=NGLI_NOISE_QUINTIC},
                    .choices=&noise_func_choices,
                    .desc=NGLI_DOCSTRING("interpolation function to use between noise points")},
    {NULL}
};

NGLI_STATIC_ASSERT(variable_priv_is_first, OFFSET(var) == 0);

static int noisefloat_update(struct ngl_node *node, double t)
{
    struct noise_priv *s = node->priv_data;
    s->var.scalar = ngli_noise_get(&s->generator[0], t * s->frequency);
    return 0;
}

static int noisevec_update(struct ngl_node *node, double t, int n)
{
    struct noise_priv *s = node->priv_data;
    const float v = t * s->frequency;
    for (int i = 0; i < n; i++)
        s->var.vector[i] = ngli_noise_get(&s->generator[i], v);
    return 0;
}

static int noisevec2_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 2);
}

static int noisevec3_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 3);
}

static int noisevec4_update(struct ngl_node *node, double t)
{
    return noisevec_update(node, t, 4);
}

static int init_noise_generators(struct noise_priv *s, int n)
{
    /*
     * Every generator is instanciated the same, except for the seed: the seed
     * offset is defined to create a large gap between every components to keep
     * the overlap to the minimum possible
     */
    const uint32_t seed_offset = UINT32_MAX / n;
    uint32_t seed = s->generator_params.seed;
    for (int i = 0; i < n; i++) {
        struct noise_params np = s->generator_params;
        np.seed = seed;
        int ret = ngli_noise_init(&s->generator[i], &np);
        if (ret < 0)
            return ret;
        seed += seed_offset;
    }
    return 0;
}

#define DEFINE_NOISE_CLASS(class_id, class_name, type, dtype, count, dst)   \
static int noise##type##_init(struct ngl_node *node)                        \
{                                                                           \
    struct noise_priv *s = node->priv_data;                                 \
    s->var.data = dst;                                                      \
    s->var.data_size = count * sizeof(float);                               \
    s->var.data_type = dtype;                                               \
    return init_noise_generators(s, count);                                 \
}                                                                           \
                                                                            \
const struct node_class ngli_noise##type##_class = {                        \
    .id        = class_id,                                                  \
    .category  = NGLI_NODE_CATEGORY_UNIFORM,                                \
    .name      = class_name,                                                \
    .init      = noise##type##_init,                                        \
    .update    = noise##type##_update,                                      \
    .priv_size = sizeof(struct noise_priv),                                 \
    .params    = noise_params,                                              \
    .params_id = "Noise",                                                   \
    .file      = __FILE__,                                                  \
};

DEFINE_NOISE_CLASS(NGL_NODE_NOISEFLOAT, "NoiseFloat", float, NGLI_TYPE_FLOAT, 1, &s->var.scalar)
DEFINE_NOISE_CLASS(NGL_NODE_NOISEVEC2,  "NoiseVec2",  vec2,  NGLI_TYPE_VEC2,  2, s->var.vector)
DEFINE_NOISE_CLASS(NGL_NODE_NOISEVEC3,  "NoiseVec3",  vec3,  NGLI_TYPE_VEC3,  3, s->var.vector)
DEFINE_NOISE_CLASS(NGL_NODE_NOISEVEC4,  "NoiseVec4",  vec4,  NGLI_TYPE_VEC4,  4, s->var.vector)
