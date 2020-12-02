/*
 * Copyright 2016-2017 GoPro Inc.
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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "gctx.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "transforms.h"

struct camera_priv {
    struct ngl_node *child;
    float eye[3];
    float center[3];
    float up[3];
    float perspective[2];
    float orthographic[4];
    float clipping[2];

    struct ngl_node *eye_transform;
    struct ngl_node *center_transform;
    struct ngl_node *up_transform;

    struct ngl_node *fov_anim;

    int use_perspective;
    int use_orthographic;

    const float *eye_transform_matrix;
    const float *center_transform_matrix;
    const float *up_transform_matrix;

    float ground[3];

    NGLI_ALIGNED_MAT(modelview_matrix);
    NGLI_ALIGNED_MAT(projection_matrix);
};

#define OFFSET(x) offsetof(struct camera_priv, x)
static const struct node_param camera_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_NON_NULL,
              .desc=NGLI_DOCSTRING("scene to observe through the lens of the camera")},
    {"eye", PARAM_TYPE_VEC3,  OFFSET(eye), {.vec={0.0f, 0.0f, 0.0f}},
            .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
            .desc=NGLI_DOCSTRING("eye position")},
    {"center", PARAM_TYPE_VEC3,  OFFSET(center), {.vec={0.0f, 0.0f, -1.0f}},
               .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("center position")},
    {"up", PARAM_TYPE_VEC3,  OFFSET(up), {.vec={0.0f, 1.0f, 0.0f}},
           .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
           .desc=NGLI_DOCSTRING("up vector")},
    {"perspective", PARAM_TYPE_VEC2,  OFFSET(perspective),
                    .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
                    .desc=NGLI_DOCSTRING("the 2 following values: *fov*, *aspect*")},
    {"orthographic", PARAM_TYPE_VEC4, OFFSET(orthographic),
                     .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("the 4 following values: *left*, *right*, *bottom*, *top*")},
    {"clipping", PARAM_TYPE_VEC2, OFFSET(clipping),
                 .flags=PARAM_FLAG_ALLOW_LIVE_CHANGE,
                 .desc=NGLI_DOCSTRING("the 2 following values: *near clipping plane*, *far clipping plane*")},
    {"eye_transform", PARAM_TYPE_NODE, OFFSET(eye_transform),
                     .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .node_types=TRANSFORM_TYPES_LIST,
                     .desc=NGLI_DOCSTRING("`eye` transformation chain")},
    {"center_transform", PARAM_TYPE_NODE, OFFSET(center_transform),
                         .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                         .node_types=TRANSFORM_TYPES_LIST,
                         .desc=NGLI_DOCSTRING("`center` transformation chain")},
    {"up_transform", PARAM_TYPE_NODE, OFFSET(up_transform),
                     .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                     .node_types=TRANSFORM_TYPES_LIST,
                     .desc=NGLI_DOCSTRING("`up` transformation chain")},
    {"fov_anim", PARAM_TYPE_NODE, OFFSET(fov_anim),
                 .node_types=(const int[]){NGL_NODE_ANIMATEDFLOAT, -1},
                 .desc=NGLI_DOCSTRING("field of view animation (first field of `perspective`)")},
    {NULL}
};

static int camera_init(struct ngl_node *node)
{
    struct camera_priv *s = node->priv_data;

    ngli_vec3_norm(s->up, s->up);
    ngli_vec3_sub(s->ground, s->eye, s->center);
    ngli_vec3_norm(s->ground, s->ground);
    ngli_vec3_cross(s->ground, s->ground, s->up);

    if (!s->ground[0] && !s->ground[1] && !s->ground[2]) {
        LOG(ERROR, "view and up are collinear");
        return NGL_ERROR_INVALID_ARG;
    }

    static const float zvec[4];
    s->use_perspective = memcmp(s->perspective, zvec, sizeof(s->perspective));
    s->use_orthographic = memcmp(s->orthographic, zvec, sizeof(s->orthographic));

    if ((s->use_perspective || s->use_orthographic) &&
        !memcmp(s->clipping, zvec, sizeof(s->clipping))) {
        LOG(ERROR, "clipping must be set when perspective or orthographic is used");
        return NGL_ERROR_INVALID_ARG;
    }

    s->eye_transform_matrix    = ngli_get_last_transformation_matrix(s->eye_transform);
    s->center_transform_matrix = ngli_get_last_transformation_matrix(s->center_transform);
    s->up_transform_matrix     = ngli_get_last_transformation_matrix(s->up_transform);

    return 0;
}

static int camera_update(struct ngl_node *node, double t)
{
    struct ngl_ctx *ctx = node->ctx;
    struct camera_priv *s = node->priv_data;
    struct ngl_node *child = s->child;

    NGLI_ALIGNED_VEC(eye)    = {0.0f, 0.0f, 0.0f, 1.0f};
    NGLI_ALIGNED_VEC(center) = {0.0f, 0.0f, 0.0f, 1.0f};
    NGLI_ALIGNED_VEC(up)     = {0.0f, 0.0f, 0.0f, 1.0f};
    static const NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;

#define APPLY_TRANSFORM(what) do {                                          \
    memcpy(what, s->what, sizeof(s->what));                                 \
    if (s->what##_transform) {                                              \
        int ret = ngli_node_update(s->what##_transform, t);                 \
        if (ret < 0)                                                        \
            return ret;                                                     \
        if (!ngli_darray_push(&ctx->modelview_matrix_stack, id_matrix))     \
            return NGL_ERROR_MEMORY;                                        \
        ngli_node_draw(s->what##_transform);                                \
        ngli_darray_pop(&ctx->modelview_matrix_stack);                      \
        const float *matrix = s->what##_transform_matrix;                   \
        if (matrix)                                                         \
            ngli_mat4_mul_vec4(what, matrix, what);                         \
    }                                                                       \
} while (0)

    APPLY_TRANSFORM(eye);
    APPLY_TRANSFORM(center);
    APPLY_TRANSFORM(up);

    if ((s->eye_transform || s->center_transform) && !s->up_transform) {
        ngli_vec3_sub(up, s->center, s->eye);
        ngli_vec3_norm(up, up);
        ngli_vec3_cross(up, up, s->ground);
    }

    ngli_mat4_look_at(s->modelview_matrix, eye, center, up);

    if (s->fov_anim) {
        struct ngl_node *anim_node = s->fov_anim;
        struct variable_priv *anim = anim_node->priv_data;
        int ret = ngli_node_update(anim_node, t);
        if (ret < 0)
            return ret;
        s->perspective[0] = anim->scalar;
    }

    if (s->use_perspective) {
        ngli_mat4_perspective(s->projection_matrix,
                              s->perspective[0],
                              s->perspective[1],
                              s->clipping[0],
                              s->clipping[1]);
    } else if (s->use_orthographic) {
        ngli_mat4_orthographic(s->projection_matrix,
                               s->orthographic[0],
                               s->orthographic[1],
                               s->orthographic[2],
                               s->orthographic[3],
                               s->clipping[0],
                               s->clipping[1]);
    } else {
        ngli_mat4_identity(s->projection_matrix);
    }

    struct gctx *gctx = ctx->gctx;
    ngli_gctx_transform_projection_matrix(gctx, s->projection_matrix);

    return ngli_node_update(child, t);
}

static void camera_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct camera_priv *s = node->priv_data;

    if (!ngli_darray_push(&ctx->modelview_matrix_stack, s->modelview_matrix) ||
        !ngli_darray_push(&ctx->projection_matrix_stack, s->projection_matrix))
        return;

    ngli_node_draw(s->child);

    ngli_darray_pop(&ctx->modelview_matrix_stack);
    ngli_darray_pop(&ctx->projection_matrix_stack);
}

const struct node_class ngli_camera_class = {
    .id        = NGL_NODE_CAMERA,
    .name      = "Camera",
    .init      = camera_init,
    .update    = camera_update,
    .draw      = camera_draw,
    .priv_size = sizeof(struct camera_priv),
    .params    = camera_params,
    .file      = __FILE__,
};
