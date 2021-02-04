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
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "type.h"

#define OFFSET(x) offsetof(struct buffer_priv, x)
static const struct node_param animatedbuffer_params[] = {
    {"keyframes", PARAM_TYPE_NODELIST, OFFSET(animkf),
                  .node_types=(const int[]){NGL_NODE_ANIMKEYFRAMEBUFFER, -1},
                  .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .desc=NGLI_DOCSTRING("key frame buffers to interpolate from")},
    {NULL}
};

static void mix_buffer(void *user_arg, void *dst,
                       const struct animkeyframe_priv *kf0,
                       const struct animkeyframe_priv *kf1,
                       double ratio)
{
    float *dstf = dst;
    const struct buffer_priv *s = user_arg;
    const float *d1 = (const float *)kf0->data;
    const float *d2 = (const float *)kf1->data;
    for (int k = 0; k < s->count; k++)
        for (int i = 0; i < s->data_comp; i++)
            dstf[k*s->data_comp + i] = NGLI_MIX(d1[k*s->data_comp + i], d2[k*s->data_comp + i], ratio);
}

static void cpy_buffer(void *user_arg, void *dst,
                       const struct animkeyframe_priv *kf)
{
    const struct buffer_priv *s = user_arg;
    memcpy(dst, kf->data, s->data_size);
}

static int animatedbuffer_update(struct ngl_node *node, double t)
{
    struct buffer_priv *s = node->priv_data;
    return ngli_animation_evaluate(&s->anim, s->data, t);
}

static int animatedbuffer_init(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    s->dynamic = 1;
    s->usage = NGLI_BUFFER_USAGE_DYNAMIC_BIT | NGLI_BUFFER_USAGE_TRANSFER_DST_BIT;
    s->data_comp = ngli_format_get_nb_comp(s->data_format);
    s->data_stride = ngli_format_get_bytes_per_pixel(s->data_format);

    int ret = ngli_animation_init(&s->anim, s,
                                  s->animkf, s->nb_animkf,
                                  mix_buffer, cpy_buffer);
    if (ret < 0)
        return ret;

    for (int i = 0; i < s->nb_animkf; i++) {
        const struct animkeyframe_priv *kf = s->animkf[i]->priv_data;
        const int data_count = kf->data_size / s->data_stride;
        const int data_pad   = kf->data_size % s->data_stride;

        if (s->count && s->count != data_count) {
            static const char *types[] = {"float", "vec2", "vec3", "vec4"};
            LOG(ERROR, "the number of %s in buffer key frame %d "
                "does not match the previous ones (%d vs %d)",
                types[s->data_comp - 1], i, data_count, s->count);
            return NGL_ERROR_INVALID_ARG;
        }

        if (data_pad)
            LOG(WARNING, "the data buffer has %d trailing bytes", data_pad);

        s->count = data_count;
    }

    if (!s->count)
        return NGL_ERROR_INVALID_ARG;

    s->data = ngli_calloc(s->count, s->data_stride);
    if (!s->data)
        return NGL_ERROR_MEMORY;
    s->data_size = s->count * s->data_stride;

    return 0;
}

static void animatedbuffer_uninit(struct ngl_node *node)
{
    struct buffer_priv *s = node->priv_data;

    ngli_freep(&s->data);
}

#define DEFINE_ABUFFER_CLASS(class_id, class_name, type, class_data_type, format)  \
static int animatedbuffer##type##_init(struct ngl_node *node)                      \
{                                                                                  \
    struct buffer_priv *s = node->priv_data;                                       \
    s->data_format = format;                                                       \
    s->data_type   = class_data_type;                                              \
    return animatedbuffer_init(node);                                              \
}                                                                                  \
                                                                                   \
const struct node_class ngli_animatedbuffer##type##_class = {                      \
    .id        = class_id,                                                         \
    .category  = NGLI_NODE_CATEGORY_BUFFER,                                        \
    .name      = class_name,                                                       \
    .init      = animatedbuffer##type##_init,                                      \
    .update    = animatedbuffer_update,                                            \
    .uninit    = animatedbuffer_uninit,                                            \
    .priv_size = sizeof(struct buffer_priv),                                       \
    .params    = animatedbuffer_params,                                            \
    .params_id = "AnimatedBuffer",                                                 \
    .file      = __FILE__,                                                         \
};                                                                                 \

DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERFLOAT, "AnimatedBufferFloat", float, NGLI_TYPE_FLOAT, NGLI_FORMAT_R32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC2,  "AnimatedBufferVec2",  vec2,  NGLI_TYPE_VEC2,  NGLI_FORMAT_R32G32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC3,  "AnimatedBufferVec3",  vec3,  NGLI_TYPE_VEC3,  NGLI_FORMAT_R32G32B32_SFLOAT)
DEFINE_ABUFFER_CLASS(NGL_NODE_ANIMATEDBUFFERVEC4,  "AnimatedBufferVec4",  vec4,  NGLI_TYPE_VEC4,  NGLI_FORMAT_R32G32B32A32_SFLOAT)
