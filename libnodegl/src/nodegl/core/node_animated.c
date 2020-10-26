/*
 * Copyright 2017 GoPro Inc.
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

#include <float.h>
#include <stddef.h>
#include <string.h>
#include "animation.h"
#include "log.h"
#include "math_utils.h"
#include "nodegl.h"
#include "nodes.h"
#include "type.h"

#define OFFSET(x) offsetof(struct variable_priv, x)
static const struct node_param animatedtime_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEFLOAT, -1},
                  .desc=NGLI_DOCSTRING("time key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedfloat_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEFLOAT, -1},
                  .desc=NGLI_DOCSTRING("float key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedvec2_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC2, -1},
                  .desc=NGLI_DOCSTRING("vec2 key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedvec3_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC3, -1},
                  .desc=NGLI_DOCSTRING("vec3 key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedvec4_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEVEC4, -1},
                  .desc=NGLI_DOCSTRING("vec4 key frames to interpolate from")},
    {NULL}
};

static const struct node_param animatedquat_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf), .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEQUAT, -1},
                  .desc=NGLI_DOCSTRING("quaternion key frames to interpolate from")},
    {"as_mat4",   PARAM_TYPE_BOOL, OFFSET(as_mat4), {.i64=0},
                  .desc=NGLI_DOCSTRING("exposed as a 4x4 rotation matrix in the program")},
    {NULL}
};

static void mix_time(void *user_arg, void *dst,
                     const struct animkeyframe_priv *kf0,
                     const struct animkeyframe_priv *kf1,
                     double ratio)
{
    double *dstd = dst;
    dstd[0] = NGLI_MIX(kf0->scalar, kf1->scalar, ratio);
}

static void mix_float(void *user_arg, void *dst,
                      const struct animkeyframe_priv *kf0,
                      const struct animkeyframe_priv *kf1,
                      double ratio)
{
    float *dstd = dst;
    dstd[0] = NGLI_MIX(kf0->scalar, kf1->scalar, ratio);
}

static void mix_quat(void *user_arg, void *dst,
                     const struct animkeyframe_priv *kf0,
                     const struct animkeyframe_priv *kf1,
                     double ratio)
{
    ngli_quat_slerp(dst, kf0->value, kf1->value, ratio);
}

static void mix_vector(void *user_arg, void *dst,
                       const struct animkeyframe_priv *kf0,
                       const struct animkeyframe_priv *kf1,
                       double ratio, int len)
{
    float *dstf = dst;
    for (int i = 0; i < len; i++)
        dstf[i] = NGLI_MIX(kf0->value[i], kf1->value[i], ratio);
}

#define DECLARE_VEC_MIX_AND_CPY_FUNCS(len)                      \
static void mix_vec##len(void *user_arg, void *dst,             \
                         const struct animkeyframe_priv *kf0,   \
                         const struct animkeyframe_priv *kf1,   \
                         double ratio)                          \
{                                                               \
    return mix_vector(user_arg, dst, kf0, kf1, ratio, len);     \
}                                                               \
                                                                \
static void cpy_vec##len(void *user_arg, void *dst,             \
                         const struct animkeyframe_priv *kf)    \
{                                                               \
    memcpy(dst, kf->value, len * sizeof(*kf->value));           \
}                                                               \

DECLARE_VEC_MIX_AND_CPY_FUNCS(2)
DECLARE_VEC_MIX_AND_CPY_FUNCS(3)
DECLARE_VEC_MIX_AND_CPY_FUNCS(4)

static void cpy_time(void *user_arg, void *dst,
                     const struct animkeyframe_priv *kf)
{
    memcpy(dst, &kf->scalar, sizeof(kf->scalar));
}

static void cpy_scalar(void *user_arg, void *dst,
                       const struct animkeyframe_priv *kf)
{
    *(float *)dst = kf->scalar;  // double → float
}

static ngli_animation_mix_func_type get_mix_func(int node_class)
{
    switch (node_class) {
        case NGL_NODE_ANIMATEDTIME:  return mix_time;
        case NGL_NODE_ANIMATEDFLOAT: return mix_float;
        case NGL_NODE_ANIMATEDVEC2:  return mix_vec2;
        case NGL_NODE_ANIMATEDVEC3:  return mix_vec3;
        case NGL_NODE_ANIMATEDVEC4:  return mix_vec4;
        case NGL_NODE_ANIMATEDQUAT:  return mix_quat;
    }
    return NULL;
}

static ngli_animation_cpy_func_type get_cpy_func(int node_class)
{
    switch (node_class) {
        case NGL_NODE_ANIMATEDTIME:  return cpy_time;
        case NGL_NODE_ANIMATEDFLOAT: return cpy_scalar;
        case NGL_NODE_ANIMATEDVEC2:  return cpy_vec2;
        case NGL_NODE_ANIMATEDVEC3:  return cpy_vec3;
        case NGL_NODE_ANIMATEDVEC4:  return cpy_vec4;
        case NGL_NODE_ANIMATEDQUAT:  return cpy_vec4;
    }
    return NULL;
}

int ngl_anim_evaluate(struct ngl_node *node, void *dst, double t)
{
    if (node->class->id != NGL_NODE_ANIMATEDFLOAT &&
        node->class->id != NGL_NODE_ANIMATEDVEC2 &&
        node->class->id != NGL_NODE_ANIMATEDVEC3 &&
        node->class->id != NGL_NODE_ANIMATEDVEC4 &&
        node->class->id != NGL_NODE_ANIMATEDQUAT)
        return NGL_ERROR_INVALID_ARG;

    struct variable_priv *s = node->priv_data;
    if (!s->nb_animkf)
        return NGL_ERROR_INVALID_ARG;

    if (node->class->id == NGL_NODE_ANIMATEDQUAT && s->as_mat4) {
        LOG(ERROR, "evaluating an AnimatedQuat to a mat4 is not supported");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (!s->anim_eval.kfs) {
        int ret = ngli_animation_init(&s->anim_eval, NULL,
                                      s->animkf, s->nb_animkf,
                                      get_mix_func(node->class->id),
                                      get_cpy_func(node->class->id));
        if (ret < 0)
            return ret;
    }

    struct animkeyframe_priv *kf0 = s->animkf[0]->priv_data;
    if (!kf0->function) {
        for (int i = 0; i < s->nb_animkf; i++) {
            int ret = s->animkf[i]->class->init(s->animkf[i]);
            if (ret < 0)
                return ret;
        }
    }

    return ngli_animation_evaluate(&s->anim_eval, dst, t);
}

static int animation_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->dynamic = 1;
    return ngli_animation_init(&s->anim, NULL,
                               s->animkf, s->nb_animkf,
                               get_mix_func(node->class->id),
                               get_cpy_func(node->class->id));
}

#define DECLARE_INIT_FUNC(suffix, class_data, class_data_size, class_data_type) \
static int animated##suffix##_init(struct ngl_node *node)                       \
{                                                                               \
    struct variable_priv *s = node->priv_data;                                  \
    s->data = class_data;                                                       \
    s->data_size = class_data_size;                                             \
    s->data_type = class_data_type;                                             \
    return animation_init(node);                                                \
}

DECLARE_INIT_FUNC(float, &s->scalar, sizeof(s->scalar),      NGLI_TYPE_FLOAT)
DECLARE_INIT_FUNC(vec2,  s->vector,  2 * sizeof(*s->vector), NGLI_TYPE_VEC2)
DECLARE_INIT_FUNC(vec3,  s->vector,  3 * sizeof(*s->vector), NGLI_TYPE_VEC3)
DECLARE_INIT_FUNC(vec4,  s->vector,  4 * sizeof(*s->vector), NGLI_TYPE_VEC4)

static int animatedtime_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = &s->dval;
    s->data_size = sizeof(s->dval);
    s->data_type = NGLI_TYPE_NONE;

    // Sanity checks for time animation keyframe
    double prev_time = 0;
    for (int i = 0; i < s->nb_animkf; i++) {
        const struct animkeyframe_priv *kf = s->animkf[i]->priv_data;
        if (kf->easing != EASING_LINEAR) {
            LOG(ERROR, "only linear interpolation is allowed for time animation");
            return NGL_ERROR_INVALID_ARG;
        }
        if (kf->scalar < prev_time) {
            LOG(ERROR, "times must be positive and monotically increasing: %g < %g",
                kf->scalar, prev_time);
            return NGL_ERROR_INVALID_ARG;
        }
        prev_time = kf->scalar;
    }

    return animation_init(node);
}

static int animatedquat_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;
    s->data = s->vector;
    s->data_size = 4 * sizeof(*s->vector);
    s->data_type = NGLI_TYPE_VEC4;
    if (s->as_mat4) {
        s->data = s->matrix;
        s->data_size = sizeof(s->matrix);
        s->data_type = NGLI_TYPE_MAT4;
    }
    return animation_init(node);
}

static int animation_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    return ngli_animation_evaluate(&s->anim, s->data, t);
}

#define animatedtime_update  animation_update
#define animatedfloat_update animation_update
#define animatedvec2_update  animation_update
#define animatedvec3_update  animation_update
#define animatedvec4_update  animation_update

static int animatedquat_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    int ret = ngli_animation_evaluate(&s->anim, s->vector, t);
    if (ret < 0)
        return ret;
    if (s->as_mat4)
        ngli_mat4_rotate_from_quat(s->matrix, s->vector);
    return 0;
}

#define DEFINE_ANIMATED_CLASS(class_id, class_name, type)       \
const struct node_class ngli_animated##type##_class = {         \
    .id        = class_id,                                      \
    .category  = NGLI_NODE_CATEGORY_UNIFORM,                    \
    .name      = class_name,                                    \
    .init      = animated##type##_init,                         \
    .update    = animated##type##_update,                       \
    .priv_size = sizeof(struct variable_priv),                  \
    .params    = animated##type##_params,                       \
    .file      = __FILE__,                                      \
};

DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDTIME,  "AnimatedTime",  time)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDFLOAT, "AnimatedFloat", float)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDVEC2,  "AnimatedVec2",  vec2)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDVEC3,  "AnimatedVec3",  vec3)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDVEC4,  "AnimatedVec4",  vec4)
DEFINE_ANIMATED_CLASS(NGL_NODE_ANIMATEDQUAT,  "AnimatedQuat",  quat)
