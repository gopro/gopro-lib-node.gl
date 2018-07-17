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
#include <unistd.h>

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "transforms.h"

#define OFFSET(x) offsetof(struct camera, x)
static const struct node_param camera_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR,
              .desc=NGLI_DOCSTRING("scene to observe through the lens of the camera")},
    {"eye", PARAM_TYPE_VEC3,  OFFSET(eye), {.vec={0.0f, 0.0f, 0.0f}},
            .desc=NGLI_DOCSTRING("eye position")},
    {"center", PARAM_TYPE_VEC3,  OFFSET(center), {.vec={0.0f, 0.0f, -1.0f}},
               .desc=NGLI_DOCSTRING("center position")},
    {"up", PARAM_TYPE_VEC3,  OFFSET(up), {.vec={0.0f, 1.0f, 0.0f}},
           .desc=NGLI_DOCSTRING("up vector")},
    {"perspective", PARAM_TYPE_VEC4,  OFFSET(perspective),
                    .desc=NGLI_DOCSTRING("the 4 following values: *fov*, *aspect*, *near clipping plane*, *far clipping plane*")},
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
    {"pipe_fd", PARAM_TYPE_INT, OFFSET(pipe_fd),
                .desc=NGLI_DOCSTRING("pipe file descriptor where the rendered raw RGBA buffer is written")},
    {"pipe_width", PARAM_TYPE_INT, OFFSET(pipe_width),
                   .desc=NGLI_DOCSTRING("width (in pixels) of the raw image buffer when using `pipe_fd`")},
    {"pipe_height", PARAM_TYPE_INT, OFFSET(pipe_height),
                    .desc=NGLI_DOCSTRING("height (in pixels) of the raw image buffer when using `pipe_fd`")},
    {"hflip", PARAM_TYPE_BOOL, OFFSET(hflip), {.i64=-1},
              .desc=NGLI_DOCSTRING("horizontal flip")},
    {NULL}
};

static int camera_init(struct ngl_node *node)
{
    struct camera *s = node->priv_data;

    ngli_vec3_norm(s->up, s->up);
    ngli_vec3_sub(s->ground, s->eye, s->center);
    ngli_vec3_norm(s->ground, s->ground);
    ngli_vec3_cross(s->ground, s->ground, s->up);

    if (!s->ground[0] && !s->ground[1] && !s->ground[2]) {
        LOG(ERROR, "view and up are collinear");
        return -1;
    }

    if (s->pipe_fd) {
        s->pipe_buf = calloc(4 /* RGBA */, s->pipe_width * s->pipe_height);
        if (!s->pipe_buf)
            return -1;

#ifdef VULKAN_BACKEND
        // TODO
#else
#if defined(TARGET_DARWIN) || defined(TARGET_LINUX)
        struct ngl_ctx *ctx = node->ctx;
        struct glcontext *gl = ctx->glcontext;

        ngli_glGenTextures(gl, 1, &s->texture_id);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, s->texture_id);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ngli_glTexParameteri(gl, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ngli_glTexImage2D(gl, GL_TEXTURE_2D, 0, GL_RGBA, s->pipe_width, s->pipe_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);

        GLuint framebuffer_id;
        ngli_glGetIntegerv(gl, GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_id);

        ngli_glGenFramebuffers(gl, 1, &s->framebuffer_id);
        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);
        ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->texture_id, 0);
        ngli_assert(ngli_glCheckFramebufferStatus(gl, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, framebuffer_id);
#endif
#endif
    }

    return 0;
}

static int camera_update(struct ngl_node *node, double t)
{
    struct camera *s = node->priv_data;
    struct ngl_node *child = s->child;

    NGLI_ALIGNED_VEC(eye)    = {0.0f, 0.0f, 0.0f, 1.0f};
    NGLI_ALIGNED_VEC(center) = {0.0f, 0.0f, 0.0f, 1.0f};
    NGLI_ALIGNED_VEC(up)     = {0.0f, 0.0f, 0.0f, 1.0f};

    float perspective[4*4];
    float view[4*4];

    const float *matrix;

#define APPLY_TRANSFORM(what) do {                                          \
    memcpy(what, s->what, sizeof(s->what));                                 \
    if (s->what##_transform) {                                              \
        int ret = ngli_node_update(s->what##_transform, t);                 \
        if (ret < 0)                                                        \
            return ret;                                                     \
        matrix = ngli_get_last_transformation_matrix(s->what##_transform);  \
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

    ngli_mat4_look_at(
        view,
        eye,
        center,
        up
    );

    if (s->fov_anim) {
        struct ngl_node *anim_node = s->fov_anim;
        struct animation *anim = anim_node->priv_data;
        int ret = ngli_node_update(anim_node, t);
        if (ret < 0)
            return ret;
        s->perspective[0] = anim->scalar;
    }

    ngli_mat4_perspective(
        perspective,
        s->perspective[0],
        s->perspective[1],
        s->perspective[2],
        s->perspective[3]
    );

    if ((s->hflip && s->pipe_fd) || s->hflip == 1)
        perspective[5] = -perspective[5];

    memcpy(child->modelview_matrix, view, sizeof(view));
    memcpy(child->projection_matrix, perspective, sizeof(perspective));

    return ngli_node_update(child, t);
}

static void camera_draw(struct ngl_node *node)
{
    struct camera *s = node->priv_data;
    ngli_node_draw(s->child);

    if (s->pipe_fd) {
#ifdef VULKAN_BACKEND
        // TODO
#else
#if defined(TARGET_DARWIN) || defined(TARGET_LINUX)
        struct ngl_ctx *ctx = node->ctx;
        struct glcontext *gl = ctx->glcontext;

        GLint multisampling = 0;
        GLuint framebuffer_read_id;
        GLuint framebuffer_draw_id;

        ngli_glGetIntegerv(gl, GL_MULTISAMPLE, &multisampling);

        if (multisampling) {
            ngli_glGetIntegerv(gl, GL_READ_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_read_id);
            ngli_glGetIntegerv(gl, GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&framebuffer_draw_id);

            ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, framebuffer_draw_id);
            ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, s->framebuffer_id);
            ngli_glBlitFramebuffer(gl, 0, 0, s->pipe_width, s->pipe_height, 0, 0, s->pipe_width, s->pipe_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

            ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, s->framebuffer_id);
        }
#endif

        LOG(DEBUG, "write %dx%d buffer to FD=%d", s->pipe_width, s->pipe_height, s->pipe_fd);
        ngli_glReadPixels(gl, 0, 0, s->pipe_width, s->pipe_height, GL_RGBA, GL_UNSIGNED_BYTE, s->pipe_buf);
        write(s->pipe_fd, s->pipe_buf, s->pipe_width * s->pipe_height * 4);

#if defined(TARGET_DARWIN) || defined(TARGET_LINUX)
        if (multisampling) {
            ngli_glBindFramebuffer(gl, GL_READ_FRAMEBUFFER, framebuffer_read_id);
            ngli_glBindFramebuffer(gl, GL_DRAW_FRAMEBUFFER, framebuffer_draw_id);
        }
#endif
#endif
    }

}

static void camera_uninit(struct ngl_node *node)
{
    struct camera *s = node->priv_data;
    if (s->pipe_fd) {
        free(s->pipe_buf);

#ifdef VULKAN_BACKEND
        // TODO
#else
#if defined(TARGET_DARWIN) || defined(TARGET_LINUX)
        struct ngl_ctx *ctx = node->ctx;
        struct glcontext *gl = ctx->glcontext;

        ngli_glBindFramebuffer(gl, GL_FRAMEBUFFER, s->framebuffer_id);
        ngli_glFramebufferTexture2D(gl, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

        ngli_glDeleteRenderbuffers(gl, 1, &s->framebuffer_id);
        ngli_glDeleteTextures(gl, 1, &s->texture_id);
#endif
#endif
    }
}

const struct node_class ngli_camera_class = {
    .id        = NGL_NODE_CAMERA,
    .name      = "Camera",
    .init      = camera_init,
    .update    = camera_update,
    .draw      = camera_draw,
    .uninit    = camera_uninit,
    .priv_size = sizeof(struct camera),
    .params    = camera_params,
    .file      = __FILE__,
};
