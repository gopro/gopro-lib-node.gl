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

#include "gctx.h"
#include "graphicstate.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

struct graphicconfig_priv {
    struct ngl_node *child;

    int blend;
    int blend_src_factor;
    int blend_dst_factor;
    int blend_src_factor_a;
    int blend_dst_factor_a;
    int blend_op;
    int blend_op_a;

    int color_write_mask;

    int depth_test;
    int depth_write_mask;
    int depth_func;

    int stencil_test;
    int stencil_write_mask;
    int stencil_func;
    int stencil_ref;
    int stencil_read_mask;
    int stencil_fail;
    int stencil_depth_fail;
    int stencil_depth_pass;

    int cull_mode;

    int scissor_test;
    float scissor_f[4];

    struct graphicstate graphicstate;
    int use_scissor;
    int scissor[4];
};

#define DEFAULT_SCISSOR_F {-1.0f, -1.0f, -1.0f, -1.0f}

static const struct param_choices blend_factor_choices = {
    .name = "blend_factor",
    .consts = {
        {"unset",               -1,                     .desc=NGLI_DOCSTRING("unset")},
        {"zero",                NGLI_BLEND_FACTOR_ZERO,                .desc=NGLI_DOCSTRING("`0`")},
        {"one",                 NGLI_BLEND_FACTOR_ONE,                 .desc=NGLI_DOCSTRING("`1`")},
        {"src_color",           NGLI_BLEND_FACTOR_SRC_COLOR,           .desc=NGLI_DOCSTRING("`src_color`")},
        {"one_minus_src_color", NGLI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, .desc=NGLI_DOCSTRING("`1 - src_color`")},
        {"dst_color",           NGLI_BLEND_FACTOR_DST_COLOR,           .desc=NGLI_DOCSTRING("`dst_color`")},
        {"one_minus_dst_color", NGLI_BLEND_FACTOR_ONE_MINUS_DST_COLOR, .desc=NGLI_DOCSTRING("`1 - dst_color`")},
        {"src_alpha",           NGLI_BLEND_FACTOR_SRC_ALPHA,           .desc=NGLI_DOCSTRING("`src_alpha`")},
        {"one_minus_src_alpha", NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .desc=NGLI_DOCSTRING("`1 - src_alpha`")},
        {"dst_alpha",           NGLI_BLEND_FACTOR_DST_ALPHA,           .desc=NGLI_DOCSTRING("`dst_alpha`")},
        {"one_minus_dst_alpha", NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, .desc=NGLI_DOCSTRING("`1 - dst_alpha`")},
        {NULL}
    }
};

static const struct param_choices blend_op_choices = {
    .name = "blend_operation",
    .consts = {
        {"unset",  -1,                             .desc=NGLI_DOCSTRING("unset")},
        {"add",    NGLI_BLEND_OP_ADD,              .desc=NGLI_DOCSTRING("`src + dst`")},
        {"sub",    NGLI_BLEND_OP_SUBTRACT,         .desc=NGLI_DOCSTRING("`src - dst`")},
        {"revsub", NGLI_BLEND_OP_REVERSE_SUBTRACT, .desc=NGLI_DOCSTRING("`dst - src`")},
        {"min",    NGLI_BLEND_OP_MIN,              .desc=NGLI_DOCSTRING("`min(src, dst)`")},
        {"max",    NGLI_BLEND_OP_MAX,              .desc=NGLI_DOCSTRING("`max(src, dst)`")},
        {NULL}
    }
};

static const struct param_choices component_choices = {
    .name = "component",
    .consts = {
        {"r", NGLI_COLOR_COMPONENT_R_BIT, .desc=NGLI_DOCSTRING("red")},
        {"g", NGLI_COLOR_COMPONENT_G_BIT, .desc=NGLI_DOCSTRING("green")},
        {"b", NGLI_COLOR_COMPONENT_B_BIT, .desc=NGLI_DOCSTRING("blue")},
        {"a", NGLI_COLOR_COMPONENT_A_BIT, .desc=NGLI_DOCSTRING("alpha")},
        {NULL}
    }
};

static const struct param_choices func_choices = {
    .name = "function",
    .consts = {
        {"unset",    -1,                               .desc=NGLI_DOCSTRING("unset")},
        {"never",    NGLI_COMPARE_OP_NEVER,            .desc=NGLI_DOCSTRING("`f(a,b) = 0`")},
        {"less",     NGLI_COMPARE_OP_LESS,             .desc=NGLI_DOCSTRING("`f(a,b) = a < b`")},
        {"equal",    NGLI_COMPARE_OP_EQUAL,            .desc=NGLI_DOCSTRING("`f(a,b) = a == b`")},
        {"lequal",   NGLI_COMPARE_OP_LESS_OR_EQUAL,    .desc=NGLI_DOCSTRING("`f(a,b) = a ≤ b`")},
        {"greater",  NGLI_COMPARE_OP_GREATER,          .desc=NGLI_DOCSTRING("`f(a,b) = a > b`")},
        {"notequal", NGLI_COMPARE_OP_NOT_EQUAL,        .desc=NGLI_DOCSTRING("`f(a,b) = a ≠ b`")},
        {"gequal",   NGLI_COMPARE_OP_GREATER_OR_EQUAL, .desc=NGLI_DOCSTRING("`f(a,b) = a ≥ b`")},
        {"always",   NGLI_COMPARE_OP_ALWAYS,           .desc=NGLI_DOCSTRING("`f(a,b) = 1`")},
        {NULL}
    }
};

static const struct param_choices stencil_op_choices = {
    .name = "stencil_operation",
    .consts = {
        {"unset",       -1,                                  .desc=NGLI_DOCSTRING("unset")},
        {"keep",        NGLI_STENCIL_OP_KEEP,                .desc=NGLI_DOCSTRING("keeps the current value")},
        {"zero",        NGLI_STENCIL_OP_ZERO,                .desc=NGLI_DOCSTRING("sets the stencil buffer value to 0")},
        {"replace",     NGLI_STENCIL_OP_REPLACE,             .desc=NGLI_DOCSTRING("sets the stencil buffer value to ref, as specified by the stencil function")},
        {"incr",        NGLI_STENCIL_OP_INCREMENT_AND_CLAMP, .desc=NGLI_DOCSTRING("increments the current stencil buffer value and clamps it")},
        {"incr_wrap",   NGLI_STENCIL_OP_INCREMENT_AND_WRAP,  .desc=NGLI_DOCSTRING("increments the current stencil buffer value and wraps it")},
        {"decr",        NGLI_STENCIL_OP_DECREMENT_AND_CLAMP, .desc=NGLI_DOCSTRING("decrements the current stencil buffer value and clamps it")},
        {"decr_wrap",   NGLI_STENCIL_OP_DECREMENT_AND_WRAP,  .desc=NGLI_DOCSTRING("decrements the current stencil buffer value and wraps it")},
        {"decr_invert", NGLI_STENCIL_OP_INVERT,              .desc=NGLI_DOCSTRING("bitwise inverts the current stencil buffer value")},
        {NULL}
    }
};

static const struct param_choices cull_mode_choices = {
    .name = "cull_mode",
    .consts = {
        {"unset", -1,                       .desc=NGLI_DOCSTRING("unset")},
        {"none",  NGLI_CULL_MODE_NONE,      .desc=NGLI_DOCSTRING("no facets are discarded")},
        {"front", NGLI_CULL_MODE_FRONT_BIT, .desc=NGLI_DOCSTRING("cull front-facing facets")},
        {"back",  NGLI_CULL_MODE_BACK_BIT,  .desc=NGLI_DOCSTRING("cull back-facing facets")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct graphicconfig_priv, x)
static const struct node_param graphicconfig_params[] = {
    {"child",              PARAM_TYPE_NODE,   OFFSET(child),              .flags=PARAM_FLAG_NON_NULL,
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
    {"cull_mode",          PARAM_TYPE_SELECT, OFFSET(cull_mode),          {.i64=-1},
                           .choices=&cull_mode_choices,
                           .desc=NGLI_DOCSTRING("face culling mode")},
    {"scissor_test",       PARAM_TYPE_BOOL,   OFFSET(scissor_test),       {.i64=-1},
                           .desc=NGLI_DOCSTRING("enable scissor testing")},
    {"scissor",            PARAM_TYPE_VEC4, OFFSET(scissor_f), {.vec=DEFAULT_SCISSOR_F},
                           .desc=NGLI_DOCSTRING("define an area where all pixels outside are discarded")},
    {NULL}
};

static int graphicconfig_init(struct ngl_node *node)
{
    struct graphicconfig_priv *s = node->priv_data;

    static const float default_scissor[4] = DEFAULT_SCISSOR_F;
    s->use_scissor = memcmp(s->scissor_f, default_scissor, sizeof(s->scissor_f));
    const float *sf = s->scissor_f;
    const int scissor[4] = {sf[0], sf[1], sf[2], sf[3]};
    memcpy(s->scissor, scissor, sizeof(s->scissor));

    return 0;
}

static int graphicconfig_update(struct ngl_node *node, double t)
{
    struct graphicconfig_priv *s = node->priv_data;
    struct ngl_node *child = s->child;

    return ngli_node_update(child, t);
}

#define COPY_PARAM(name) do {        \
    if (s->name != -1) {             \
        pending->name = s->name;     \
    }                                \
} while (0)                          \

static void honor_config(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct graphicconfig_priv *s = node->priv_data;
    struct graphicstate *pending = &rnode->graphicstate;

    s->graphicstate = *pending;

    COPY_PARAM(blend);
    COPY_PARAM(blend_dst_factor);
    COPY_PARAM(blend_src_factor);
    COPY_PARAM(blend_dst_factor_a);
    COPY_PARAM(blend_src_factor_a);
    COPY_PARAM(blend_op);
    COPY_PARAM(blend_op_a);

    COPY_PARAM(color_write_mask);

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

    if (s->cull_mode != -1)
        pending->cull_mode = ngli_gctx_transform_cull_mode(gctx, s->cull_mode);

    COPY_PARAM(scissor_test);
}

static int graphicconfig_prepare(struct ngl_node *node)
{
    struct graphicconfig_priv *s = node->priv_data;
    struct ngl_node *child = s->child;

    honor_config(node);
    return ngli_node_prepare(child);
}

static void graphicconfig_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct graphicconfig_priv *s = node->priv_data;
    struct ngl_node *child = s->child;

    int prev_scissor[4];
    if (s->use_scissor) {
        ngli_gctx_get_scissor(gctx, prev_scissor);
        ngli_gctx_set_scissor(gctx, s->scissor);
    }

    ngli_node_draw(child);

    if (s->use_scissor)
        ngli_gctx_set_scissor(gctx, prev_scissor);
}

const struct node_class ngli_graphicconfig_class = {
    .id        = NGL_NODE_GRAPHICCONFIG,
    .name      = "GraphicConfig",
    .init      = graphicconfig_init,
    .prepare   = graphicconfig_prepare,
    .update    = graphicconfig_update,
    .draw      = graphicconfig_draw,
    .priv_size = sizeof(struct graphicconfig_priv),
    .params    = graphicconfig_params,
    .file      = __FILE__,
};
