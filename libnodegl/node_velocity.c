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
#include <string.h>

#include "animation.h"
#include "math_utils.h"
#include "nodes.h"
#include "type.h"

#define OFFSET(x) offsetof(struct variable_priv, x)
static const struct node_param velocityfloat_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const int[]){NGL_NODE_ANIMATEDFLOAT, -1},
                  .desc=NGLI_DOCSTRING("1D animation to analyze the velocity from")},
    {NULL}
};

static const struct node_param velocityvec2_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const int[]){NGL_NODE_ANIMATEDVEC2, -1},
                  .desc=NGLI_DOCSTRING("2D animation to analyze the velocity from")},
    {NULL}
};

static const struct node_param velocityvec3_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const int[]){NGL_NODE_ANIMATEDVEC3, -1},
                  .desc=NGLI_DOCSTRING("3D animation to analyze the velocity from")},
    {NULL}
};

static const struct node_param velocityvec4_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const int[]){NGL_NODE_ANIMATEDVEC4, -1},
                  .desc=NGLI_DOCSTRING("4D animation to analyze the velocity from")},
    {NULL}
};

static void mix_velocity_float(void *user_arg, void *dst,
                               const struct animkeyframe_priv *kf0,
                               const struct animkeyframe_priv *kf1,
                               double ratio)
{
    float *dstf = dst;
    dstf[0] = (kf1->scalar - kf0->scalar) * ratio;
}

static void cpy_velocity_float(void *user_arg, void *dst,
                               const struct animkeyframe_priv *kf)
{
    float *dstf = dst;
    dstf[0] = 0;
}

#define DECLARE_VELOCITY_FUNCS(len)                                     \
static void mix_velocity_vec##len(void *user_arg, void *dst,            \
                                  const struct animkeyframe_priv *kf0,  \
                                  const struct animkeyframe_priv *kf1,  \
                                  double ratio)                         \
{                                                                       \
    float *dstf = dst;                                                  \
    ngli_vec##len##_sub(dstf, kf1->value, kf0->value);                  \
    ngli_vec##len##_norm(dstf, dstf);                                   \
    ngli_vec##len##_scale(dstf, dstf, ratio);                           \
}                                                                       \
                                                                        \
static void cpy_velocity_vec##len(void *user_arg, void *dst,            \
                                  const struct animkeyframe_priv *kf)   \
{                                                                       \
    memset(dst, 0, len * sizeof(*kf->value));                           \
}                                                                       \

DECLARE_VELOCITY_FUNCS(2)
DECLARE_VELOCITY_FUNCS(3)
DECLARE_VELOCITY_FUNCS(4)

static ngli_animation_mix_func_type get_mix_func(int node_class)
{
    switch (node_class) {
    case NGL_NODE_VELOCITYFLOAT: return mix_velocity_float;
    case NGL_NODE_VELOCITYVEC2:  return mix_velocity_vec2;
    case NGL_NODE_VELOCITYVEC3:  return mix_velocity_vec3;
    case NGL_NODE_VELOCITYVEC4:  return mix_velocity_vec4;
    }
    return NULL;
}

static ngli_animation_cpy_func_type get_cpy_func(int node_class)
{
    switch (node_class) {
    case NGL_NODE_VELOCITYFLOAT: return cpy_velocity_float;
    case NGL_NODE_VELOCITYVEC2:  return cpy_velocity_vec2;
    case NGL_NODE_VELOCITYVEC3:  return cpy_velocity_vec3;
    case NGL_NODE_VELOCITYVEC4:  return cpy_velocity_vec4;
    }
    return NULL;
}

static int velocity_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    struct variable_priv *anim = s->anim_node->priv_data;
    s->dynamic = 1;
    return ngli_animation_init(&s->anim, NULL,
                               anim->animkf, anim->nb_animkf,
                               get_mix_func(node->cls->id),
                               get_cpy_func(node->cls->id));
}

static int velocity_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    return ngli_animation_derivate(&s->anim, s->data, t);
}

#define DEFINE_VELOCITY_CLASS(class_id, class_name, type, dtype, count, dst)    \
static int velocity##type##_init(struct ngl_node *node)                         \
{                                                                               \
    struct variable_priv *s = node->priv_data;                                  \
    s->data = dst;                                                              \
    s->data_size = count * sizeof(float);                                       \
    s->data_type = dtype;                                                       \
    return velocity_init(node);                                                 \
}                                                                               \
                                                                                \
const struct node_class ngli_velocity##type##_class = {                         \
    .id        = class_id,                                                      \
    .category  = NGLI_NODE_CATEGORY_UNIFORM,                                    \
    .name      = class_name,                                                    \
    .init      = velocity##type##_init,                                         \
    .update    = velocity_update,                                               \
    .priv_size = sizeof(struct variable_priv),                                  \
    .params    = velocity##type##_params,                                       \
    .file      = __FILE__,                                                      \
};

DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYFLOAT, "VelocityFloat", float, NGLI_TYPE_FLOAT, 1, &s->scalar)
DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYVEC2,  "VelocityVec2",  vec2,  NGLI_TYPE_VEC2,  2, s->vector)
DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYVEC3,  "VelocityVec3",  vec3,  NGLI_TYPE_VEC3,  3, s->vector)
DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYVEC4,  "VelocityVec4",  vec4,  NGLI_TYPE_VEC4,  4, s->vector)
