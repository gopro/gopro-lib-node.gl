/*
 * Copyright 2019 GoPro Inc.
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

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "type.h"

#define OFFSET(x) offsetof(struct variable_priv, x)

#define DECLARE_STREAMED_PARAMS(name, allowed_node)                                                       \
static const struct node_param streamed##name##_params[] = {                                              \
    {"timestamps", PARAM_TYPE_NODE, OFFSET(timestamps), .flags=PARAM_FLAG_NON_NULL,                       \
                   .node_types=(const int[]){NGL_NODE_BUFFERINT64, -1},                                   \
                   .desc=NGLI_DOCSTRING("timestamps associated with each chunk of data to stream")},      \
    {"buffer",     PARAM_TYPE_NODE, OFFSET(buffer), .flags=PARAM_FLAG_NON_NULL,                           \
                   .node_types=(const int[]){allowed_node, -1},                                           \
                   .desc=NGLI_DOCSTRING("buffer containing the data to stream")},                         \
    {"timebase",   PARAM_TYPE_RATIONAL, OFFSET(timebase), {.r={1, 1000000}},                              \
                   .desc=NGLI_DOCSTRING("time base in which the `timestamps` are represented")},          \
    {"time_anim",  PARAM_TYPE_NODE, OFFSET(time_anim),                                                    \
                   .node_types=(const int[]){NGL_NODE_ANIMATEDTIME, -1},                                  \
                   .desc=NGLI_DOCSTRING("time remapping animation (must use a `linear` interpolation)")}, \
    {NULL}                                                                                                \
};

DECLARE_STREAMED_PARAMS(int,    NGL_NODE_BUFFERINT)
DECLARE_STREAMED_PARAMS(ivec2,  NGL_NODE_BUFFERIVEC2)
DECLARE_STREAMED_PARAMS(ivec3,  NGL_NODE_BUFFERIVEC3)
DECLARE_STREAMED_PARAMS(ivec4,  NGL_NODE_BUFFERIVEC4)
DECLARE_STREAMED_PARAMS(uint,   NGL_NODE_BUFFERUINT)
DECLARE_STREAMED_PARAMS(uivec2, NGL_NODE_BUFFERUIVEC2)
DECLARE_STREAMED_PARAMS(uivec3, NGL_NODE_BUFFERUIVEC3)
DECLARE_STREAMED_PARAMS(uivec4, NGL_NODE_BUFFERUIVEC4)
DECLARE_STREAMED_PARAMS(float,  NGL_NODE_BUFFERFLOAT)
DECLARE_STREAMED_PARAMS(vec2,   NGL_NODE_BUFFERVEC2)
DECLARE_STREAMED_PARAMS(vec3,   NGL_NODE_BUFFERVEC3)
DECLARE_STREAMED_PARAMS(vec4,   NGL_NODE_BUFFERVEC4)
DECLARE_STREAMED_PARAMS(mat4,   NGL_NODE_BUFFERMAT4)

static int get_data_index(const struct ngl_node *node, int start, int64_t t64)
{
    const struct variable_priv *s = node->priv_data;
    const struct buffer_priv *timestamps_priv = s->timestamps->priv_data;
    const int64_t *timestamps = (int64_t *)timestamps_priv->data;
    const int nb_timestamps = timestamps_priv->count;

    int ret = -1;
    for (int i = start; i < nb_timestamps; i++) {
        const int64_t ts = timestamps[i];
        if (ts > t64)
            break;
        ret = i;
    }
    return ret;
}

static int streamed_update(struct ngl_node *node, double t)
{
    struct variable_priv *s = node->priv_data;
    struct ngl_node *time_anim = s->time_anim;

    double rt = t;
    if (time_anim) {
        struct variable_priv *anim = time_anim->priv_data;

        if (anim->nb_animkf >= 1) {
            const struct animkeyframe_priv *kf0 = anim->animkf[0]->priv_data;
            if (anim->nb_animkf == 1) {
                rt = NGLI_MAX(0, t - kf0->time) + kf0->scalar;
            } else {
                int ret = ngli_node_update(time_anim, t);
                if (ret < 0)
                    return ret;
                rt = anim->dval;
            }

            TRACE("remapped time f(%g)=%g", t, rt);
            if (rt < 0) {
                LOG(ERROR, "invalid remapped time %g", rt);
                return NGL_ERROR_INVALID_ARG;
            }
        }
    }

    const int64_t t64 = llrint(rt * s->timebase[1] / (double)s->timebase[0]);
    int index = get_data_index(node, s->last_index, t64);
    if (index < 0) {
        index = get_data_index(node, 0, t64);
        if (index < 0) // the requested time `t` is before the first user timestamp
            index = 0;
    }
    s->last_index = index;

    const struct buffer_priv *buffer_priv = s->buffer->priv_data;
    const uint8_t *datap = buffer_priv->data + buffer_priv->data_stride * index;
    memcpy(s->data, datap, s->data_size);

    return 0;
}

static int check_timestamps_buffer(const struct ngl_node *node)
{
    const struct variable_priv *s = node->priv_data;
    const struct buffer_priv *timestamps_priv = s->timestamps->priv_data;
    const int64_t *timestamps = (int64_t *)timestamps_priv->data;
    const int nb_timestamps = timestamps_priv->count;

    if (!nb_timestamps) {
        LOG(ERROR, "timestamps buffer must not be empty");
        return NGL_ERROR_INVALID_ARG;
    }

    const struct buffer_priv *buffer_priv = s->buffer->priv_data;
    if (nb_timestamps != buffer_priv->count) {
        LOG(ERROR, "timestamps count must match buffer data count: %d != %d", nb_timestamps, buffer_priv->count);
        return NGL_ERROR_INVALID_ARG;
    }

    int64_t last_ts = timestamps[0];
    for (int i = 1; i < nb_timestamps; i++) {
        const int64_t ts = timestamps[i];
        if (ts < 0) {
            LOG(ERROR, "timestamps must be positive: %" PRId64, ts);
            return NGL_ERROR_INVALID_ARG;
        }
        if (ts < last_ts) {
            LOG(ERROR, "timestamps must be monotically increasing: %" PRId64 " < %" PRId64, ts, last_ts);
            return NGL_ERROR_INVALID_ARG;
        }
        last_ts = ts;
    }

    return 0;
}

static int streamed_init(struct ngl_node *node)
{
    struct variable_priv *s = node->priv_data;

    if (!s->timebase[1]) {
        LOG(ERROR, "invalid timebase: %d/%d", s->timebase[0], s->timebase[1]);
        return NGL_ERROR_INVALID_ARG;
    }

    return check_timestamps_buffer(node);
}

#define DECLARE_STREAMED_INIT(suffix, class_data, class_data_size, class_data_type) \
static int streamed##suffix##_init(struct ngl_node *node)                           \
{                                                                                   \
    struct variable_priv *s = node->priv_data;                                      \
    s->data = class_data;                                                           \
    s->data_size = class_data_size;                                                 \
    s->data_type = class_data_type;                                                 \
    s->dynamic = 1;                                                                 \
    return streamed_init(node);                                                     \
}                                                                                   \

DECLARE_STREAMED_INIT(int,    s->ivector, sizeof(*s->ivector),     NGLI_TYPE_INT)
DECLARE_STREAMED_INIT(ivec2,  s->ivector, 2 * sizeof(*s->ivector), NGLI_TYPE_IVEC2)
DECLARE_STREAMED_INIT(ivec3,  s->ivector, 3 * sizeof(*s->ivector), NGLI_TYPE_IVEC3)
DECLARE_STREAMED_INIT(ivec4,  s->ivector, 4 * sizeof(*s->ivector), NGLI_TYPE_IVEC4)
DECLARE_STREAMED_INIT(uint,   s->uvector, sizeof(*s->uvector),     NGLI_TYPE_UINT)
DECLARE_STREAMED_INIT(uivec2, s->uvector, 2 * sizeof(*s->uvector), NGLI_TYPE_UIVEC2)
DECLARE_STREAMED_INIT(uivec3, s->uvector, 3 * sizeof(*s->uvector), NGLI_TYPE_UIVEC3)
DECLARE_STREAMED_INIT(uivec4, s->uvector, 4 * sizeof(*s->uvector), NGLI_TYPE_UIVEC4)
DECLARE_STREAMED_INIT(float,  &s->scalar, sizeof(s->scalar),       NGLI_TYPE_FLOAT)
DECLARE_STREAMED_INIT(vec2,   s->vector,  2 * sizeof(*s->vector),  NGLI_TYPE_VEC2)
DECLARE_STREAMED_INIT(vec3,   s->vector,  3 * sizeof(*s->vector),  NGLI_TYPE_VEC3)
DECLARE_STREAMED_INIT(vec4,   s->vector,  4 * sizeof(*s->vector),  NGLI_TYPE_VEC4)
DECLARE_STREAMED_INIT(mat4,   s->matrix,  sizeof(s->matrix),       NGLI_TYPE_MAT4)

#define DECLARE_STREAMED_CLASS(class_id, class_name, class_suffix)          \
const struct node_class ngli_streamed##class_suffix##_class = {             \
    .id        = class_id,                                                  \
    .category  = NGLI_NODE_CATEGORY_UNIFORM,                                \
    .name      = class_name,                                                \
    .init      = streamed##class_suffix##_init,                             \
    .update    = streamed_update,                                           \
    .priv_size = sizeof(struct variable_priv),                              \
    .params    = streamed##class_suffix##_params,                           \
    .file      = __FILE__,                                                  \
};                                                                          \

DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDINT,    "StreamedInt",    int)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDIVEC2,  "StreamedIVec2",  ivec2)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDIVEC3,  "StreamedIVec3",  ivec3)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDIVEC4,  "StreamedIVec4",  ivec4)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDUINT,   "StreamedUInt",   uint)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDUIVEC2, "StreamedUIVec2", uivec2)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDUIVEC3, "StreamedUIVec3", uivec3)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDUIVEC4, "StreamedUIVec4", uivec4)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDFLOAT,  "StreamedFloat",  float)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDVEC2,   "StreamedVec2",   vec2)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDVEC3,   "StreamedVec3",   vec3)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDVEC4,   "StreamedVec4",   vec4)
DECLARE_STREAMED_CLASS(NGL_NODE_STREAMEDMAT4,   "StreamedMat4",   mat4)
