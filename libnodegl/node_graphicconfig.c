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

#include <stddef.h>
#include <string.h>

#include "glincludes.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

static const struct param_choices blend_factor_choices = {
    .name = "blend_factor",
    .consts = {
        {"unset",               -1,                     .desc=NGLI_DOCSTRING("unset")},
        {"zero",                GL_ZERO,                .desc=NGLI_DOCSTRING("`0`")},
        {"one",                 GL_ONE,                 .desc=NGLI_DOCSTRING("`1`")},
        {"src_color",           GL_SRC_COLOR,           .desc=NGLI_DOCSTRING("`src_color`")},
        {"one_minus_src_color", GL_ONE_MINUS_SRC_COLOR, .desc=NGLI_DOCSTRING("`1 - src_color`")},
        {"dst_color",           GL_DST_COLOR,           .desc=NGLI_DOCSTRING("`dst_color`")},
        {"one_minus_dst_color", GL_ONE_MINUS_DST_COLOR, .desc=NGLI_DOCSTRING("`1 - dst_color`")},
        {"src_alpha",           GL_SRC_ALPHA,           .desc=NGLI_DOCSTRING("`src_alpha`")},
        {"one_minus_src_alpha", GL_ONE_MINUS_SRC_ALPHA, .desc=NGLI_DOCSTRING("`1 - src_alpha`")},
        {"dst_alpha",           GL_DST_ALPHA,           .desc=NGLI_DOCSTRING("`dst_alpha`")},
        {"one_minus_dst_alpha", GL_ONE_MINUS_DST_ALPHA, .desc=NGLI_DOCSTRING("`1 - dst_alpha`")},
        {NULL}
    }
};

static const struct param_choices blend_op_choices = {
    .name = "blend_operation",
    .consts = {
        {"unset",  -1,                       .desc=NGLI_DOCSTRING("unset")},
        {"add",    GL_FUNC_ADD,              .desc=NGLI_DOCSTRING("`src + dst`")},
        {"sub",    GL_FUNC_SUBTRACT,         .desc=NGLI_DOCSTRING("`src - dst`")},
        {"revsub", GL_FUNC_REVERSE_SUBTRACT, .desc=NGLI_DOCSTRING("`dst - src`")},
        {"min",    GL_MIN,                   .desc=NGLI_DOCSTRING("`min(src, dst)`")},
        {"max",    GL_MAX,                   .desc=NGLI_DOCSTRING("`max(src, dst)`")},
        {NULL}
    }
};

static const struct param_choices component_choices = {
    .name = "component",
    .consts = {
        {"r", 1<<0, .desc=NGLI_DOCSTRING("red")},
        {"g", 1<<1, .desc=NGLI_DOCSTRING("green")},
        {"b", 1<<2, .desc=NGLI_DOCSTRING("blue")},
        {"a", 1<<3, .desc=NGLI_DOCSTRING("alpha")},
        {NULL}
    }
};

static const struct param_choices func_choices = {
    .name = "function",
    .consts = {
        {"unset",    -1,          .desc=NGLI_DOCSTRING("unset")},
        {"never",    GL_NEVER,    .desc=NGLI_DOCSTRING("`f(a,b) = 0`")},
        {"less",     GL_LESS,     .desc=NGLI_DOCSTRING("`f(a,b) = a < b`")},
        {"equal",    GL_EQUAL,    .desc=NGLI_DOCSTRING("`f(a,b) = a == b`")},
        {"lequal",   GL_LEQUAL,   .desc=NGLI_DOCSTRING("`f(a,b) = a ≤ b`")},
        {"greater",  GL_GREATER,  .desc=NGLI_DOCSTRING("`f(a,b) = a > b`")},
        {"notequal", GL_NOTEQUAL, .desc=NGLI_DOCSTRING("`f(a,b) = a ≠ b`")},
        {"gequal",   GL_GEQUAL,   .desc=NGLI_DOCSTRING("`f(a,b) = a ≥ b`")},
        {"always",   GL_ALWAYS,   .desc=NGLI_DOCSTRING("`f(a,b) = 1`")},
        {NULL}
    }
};

static const struct param_choices stencil_op_choices = {
    .name = "stencil_operation",
    .consts = {
        {"unset",       -1,           .desc=NGLI_DOCSTRING("unset")},
        {"keep",        GL_KEEP,      .desc=NGLI_DOCSTRING("keeps the current value")},
        {"zero",        GL_ZERO,      .desc=NGLI_DOCSTRING("sets the stencil buffer value to 0")},
        {"replace",     GL_REPLACE,   .desc=NGLI_DOCSTRING("sets the stencil buffer value to ref, as specified by the stencil function")},
        {"incr",        GL_INCR,      .desc=NGLI_DOCSTRING("increments the current stencil buffer value and clamps it")},
        {"incr_wrap",   GL_INCR_WRAP, .desc=NGLI_DOCSTRING("increments the current stencil buffer value and wraps it")},
        {"decr",        GL_DECR,      .desc=NGLI_DOCSTRING("decrements the current stencil buffer value and clamps it")},
        {"decr_wrap",   GL_DECR_WRAP, .desc=NGLI_DOCSTRING("decrements the current stencil buffer value and wraps it")},
        {"decr_invert", GL_INVERT,    .desc=NGLI_DOCSTRING("bitwise inverts the current stencil buffer value")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct graphicconfig, x)
static const struct node_param graphicconfig_params[] = {
    {"child",              PARAM_TYPE_NODE,   OFFSET(child),              .flags=PARAM_FLAG_CONSTRUCTOR,
                           .desc=NGLI_DOCSTRING("scene to which the graphic configuration will be applied")},
    {"blend",              PARAM_TYPE_BOOL,   OFFSET(blend),              {.i64=-1},
                           .desc=NGLI_DOCSTRING("enable blending")},
    {"blend_src_factor",   PARAM_TYPE_SELECT, OFFSET(blend_src_factor),   {.i64=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("blend source factor")},
    {"blend_dst_factor",   PARAM_TYPE_SELECT, OFFSET(blend_dst_factor),   {.i64=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("blend destination factor")},
    {"blend_src_factor_a", PARAM_TYPE_SELECT, OFFSET(blend_src_factor_a), {.i64=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("alpha blend source factor")},
    {"blend_dst_factor_a", PARAM_TYPE_SELECT, OFFSET(blend_dst_factor_a), {.i64=-1},
                           .choices=&blend_factor_choices,
                           .desc=NGLI_DOCSTRING("alpha blend destination factor")},
    {"blend_op",           PARAM_TYPE_SELECT, OFFSET(blend_op),           {.i64=-1},
                           .choices=&blend_op_choices,
                           .desc=NGLI_DOCSTRING("blend operation")},
    {"blend_op_a",         PARAM_TYPE_SELECT, OFFSET(blend_op_a),         {.i64=-1},
                           .choices=&blend_op_choices,
                           .desc=NGLI_DOCSTRING("alpha blend operation")},
    {"color_write_mask",   PARAM_TYPE_FLAGS,  OFFSET(color_write_mask),   {.i64=-1},
                           .choices=&component_choices,
                           .desc=NGLI_DOCSTRING("color write mask")},
    {"depth_test",         PARAM_TYPE_BOOL,   OFFSET(depth_test),         {.i64=-1},
                           .desc=NGLI_DOCSTRING("enable depth testing")},
    {"depth_write_mask",   PARAM_TYPE_BOOL,   OFFSET(depth_write_mask),   {.i64=-1},
                           .desc=NGLI_DOCSTRING("depth write mask")},
    {"depth_func",         PARAM_TYPE_SELECT, OFFSET(depth_func),         {.i64=-1},
                           .desc=NGLI_DOCSTRING("passes if `<function>(depth, stored_depth)`"),
                           .choices=&func_choices},
    {"stencil_test",       PARAM_TYPE_BOOL,   OFFSET(stencil_test),       {.i64=-1},
                           .desc=NGLI_DOCSTRING("enable stencil testing")},
    {"stencil_write_mask", PARAM_TYPE_INT,    OFFSET(stencil_write_mask), {.i64=-1},
                           .desc=NGLI_DOCSTRING("stencil write mask")},
    {"stencil_func",       PARAM_TYPE_SELECT, OFFSET(stencil_func),       {.i64=-1},
                           .desc=NGLI_DOCSTRING("passes if `<function>(stencil_ref & stencil_read_mask, stencil & stencil_read_mask)`"),
                           .choices=&func_choices},
    {"stencil_ref",        PARAM_TYPE_INT,    OFFSET(stencil_ref),        {.i64=-1},
                           .desc=NGLI_DOCSTRING("stencil reference value to compare against")},
    {"stencil_read_mask",  PARAM_TYPE_INT,    OFFSET(stencil_read_mask),  {.i64=-1},
                           .desc=NGLI_DOCSTRING("stencil read mask")},
    {"stencil_fail",       PARAM_TYPE_SELECT, OFFSET(stencil_fail),       {.i64=-1},
                           .choices=&stencil_op_choices,
                           .desc=NGLI_DOCSTRING("operation to execute if stencil test fails")},
    {"stencil_depth_fail", PARAM_TYPE_SELECT, OFFSET(stencil_depth_fail), {.i64=-1},
                           .choices=&stencil_op_choices,
                           .desc=NGLI_DOCSTRING("operation to execute if depth test fails")},
    {"stencil_depth_pass", PARAM_TYPE_SELECT, OFFSET(stencil_depth_pass), {.i64=-1},
                           .choices=&stencil_op_choices,
                           .desc=NGLI_DOCSTRING("operation to execute if stencil and depth test pass")},
    {NULL}
};

static int graphicconfig_update(struct ngl_node *node, double t)
{
    struct graphicconfig *s = node->priv_data;
    struct ngl_node *child = s->child;

    ngli_node_transfer_matrices(child, node);
    return ngli_node_update(child, t);
}

#define COPY_PARAM(name) do {        \
    if (s->name != -1) {             \
        next->name = s->name;        \
    }                                \
} while (0)                          \

static void honor_config(struct ngl_node *node, int restore)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct graphicconfig *s = node->priv_data;
    struct glstate *prev = restore ? &s->states[0] : &s->states[1];
    struct glstate *next = restore ? &s->states[1] : &s->states[0];

    if (!restore) {
        *next = ctx->glstate;

        COPY_PARAM(blend);
        COPY_PARAM(blend_dst_factor);
        COPY_PARAM(blend_src_factor);
        COPY_PARAM(blend_dst_factor_a);
        COPY_PARAM(blend_src_factor_a);
        COPY_PARAM(blend_op);
        COPY_PARAM(blend_op_a);

        if (s->color_write_mask != -1) {
            for (int i = 0; i < 4; i++)
                next->color_write_mask[i] = s->color_write_mask >> i & 1;
        }

        COPY_PARAM(depth_test);
        COPY_PARAM(depth_write_mask);
        COPY_PARAM(depth_func);

        COPY_PARAM(stencil_test);
        COPY_PARAM(stencil_write_mask);
        COPY_PARAM(stencil_func);
        COPY_PARAM(stencil_ref);
        COPY_PARAM(stencil_read_mask);
        COPY_PARAM(stencil_fail);
        COPY_PARAM(stencil_depth_fail);
        COPY_PARAM(stencil_depth_pass);
    }

    *prev = ctx->glstate;
    ctx->glstate = *next;

    ngli_glstate_honor_state(gl, next, prev);
}

static void graphicconfig_draw(struct ngl_node *node)
{
    struct graphicconfig *s = node->priv_data;

    honor_config(node, 0);
    ngli_node_draw(s->child);
    honor_config(node, 1);
}

const struct node_class ngli_graphicconfig_class = {
    .id        = NGL_NODE_GRAPHICCONFIG,
    .name      = "GraphicConfig",
    .update    = graphicconfig_update,
    .draw      = graphicconfig_draw,
    .priv_size = sizeof(struct graphicconfig),
    .params    = graphicconfig_params,
    .file      = __FILE__,
};
