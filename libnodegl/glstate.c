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

#include <stdlib.h>
#include <string.h>
#include "glcontext.h"
#include "glincludes.h"
#include "glstate.h"

void ngli_glstate_probe(const struct glcontext *gl, struct glstate *state)
{
    /* Blend */
    ngli_glGetIntegerv(gl, GL_BLEND,                   (GLint *)&state->blend);
    ngli_glGetIntegerv(gl, GL_BLEND_SRC_RGB,           (GLint *)&state->blend_src_factor);
    ngli_glGetIntegerv(gl, GL_BLEND_DST_RGB,           (GLint *)&state->blend_dst_factor);
    ngli_glGetIntegerv(gl, GL_BLEND_SRC_ALPHA,         (GLint *)&state->blend_src_factor_a);
    ngli_glGetIntegerv(gl, GL_BLEND_DST_ALPHA,         (GLint *)&state->blend_dst_factor_a);
    ngli_glGetIntegerv(gl, GL_BLEND_EQUATION_RGB,      (GLint *)&state->blend_op);
    ngli_glGetIntegerv(gl, GL_BLEND_EQUATION_ALPHA,    (GLint *)&state->blend_op_a);

    /* Color */
    ngli_glGetBooleanv(gl, GL_COLOR_WRITEMASK,         state->color_write_mask);

    /* Depth */
    ngli_glGetIntegerv(gl, GL_DEPTH_TEST,              (GLint *)&state->depth_test);
    ngli_glGetBooleanv(gl, GL_DEPTH_WRITEMASK ,        &state->depth_write_mask);
    ngli_glGetIntegerv(gl, GL_DEPTH_FUNC,              (GLint *)&state->depth_func);

    /* Stencil */
    ngli_glGetIntegerv(gl, GL_STENCIL_TEST,            (GLint *)&state->stencil_test);
    ngli_glGetIntegerv(gl, GL_STENCIL_WRITEMASK,       (GLint *)&state->stencil_write_mask);
    ngli_glGetIntegerv(gl, GL_STENCIL_FUNC,            (GLint *)&state->stencil_func);
    ngli_glGetIntegerv(gl, GL_STENCIL_REF,             &state->stencil_ref);
    ngli_glGetIntegerv(gl, GL_STENCIL_VALUE_MASK,      (GLint *)&state->stencil_read_mask);
    ngli_glGetIntegerv(gl, GL_STENCIL_FAIL,            (GLint *)&state->stencil_fail);
    ngli_glGetIntegerv(gl, GL_STENCIL_PASS_DEPTH_FAIL, (GLint *)&state->stencil_depth_fail);
    ngli_glGetIntegerv(gl, GL_STENCIL_PASS_DEPTH_PASS, (GLint *)&state->stencil_depth_pass);
}

void ngli_glstate_honor_state(const struct glcontext *gl,
                              const struct glstate *next,
                              const struct glstate *prev)
{
    /* Blend */
    if (next->blend != prev->blend) {
        if (next->blend)
            ngli_glEnable(gl, GL_BLEND);
        else
            ngli_glDisable(gl, GL_BLEND);
    }

    if (next->blend_dst_factor   != prev->blend_dst_factor   ||
        next->blend_src_factor   != prev->blend_src_factor   ||
        next->blend_dst_factor_a != prev->blend_dst_factor_a ||
        next->blend_src_factor_a != prev->blend_src_factor_a) {
        ngli_glBlendFuncSeparate(gl,
                                 next->blend_src_factor,
                                 next->blend_dst_factor,
                                 next->blend_src_factor_a,
                                 next->blend_dst_factor_a);
    }

    if (next->blend_op   != prev->blend_op ||
        next->blend_op_a != prev->blend_op_a) {
        ngli_glBlendEquationSeparate(gl,
                                     next->blend_op,
                                     next->blend_op_a);
    }

    /* Color */
    if (memcmp(next->color_write_mask, prev->color_write_mask, sizeof(prev->color_write_mask))) {
        ngli_glColorMask(gl,
                         next->color_write_mask[0],
                         next->color_write_mask[1],
                         next->color_write_mask[2],
                         next->color_write_mask[3]);
    }

    /* Depth */
    if (next->depth_test != prev->depth_test) {
        if (next->depth_test)
            ngli_glEnable(gl, GL_DEPTH_TEST);
        else
            ngli_glDisable(gl, GL_DEPTH_TEST);
    }

    if (next->depth_write_mask != prev->depth_write_mask) {
        ngli_glDepthMask(gl, next->depth_write_mask);
    }

    if (next->depth_func != prev->depth_func) {
        ngli_glDepthFunc(gl, next->depth_func);
    }

    /* Stencil */
    if (next->stencil_test != prev->stencil_test) {
        if (next->stencil_test)
            ngli_glEnable(gl, GL_STENCIL_TEST);
        else
            ngli_glDisable(gl, GL_STENCIL_TEST);
    }

    if (next->stencil_write_mask != prev->stencil_write_mask) {
        ngli_glStencilMask(gl, next->stencil_write_mask);
    }

    if (next->stencil_func      != prev->stencil_func ||
        next->stencil_ref       != prev->stencil_ref  ||
        next->stencil_read_mask != prev->stencil_read_mask) {
        ngli_glStencilFunc(gl,
                           next->stencil_func,
                           next->stencil_ref,
                           next->stencil_read_mask);
    }

    if (next->stencil_fail       != prev->stencil_fail       ||
        next->stencil_depth_fail != prev->stencil_depth_fail ||
        next->stencil_depth_pass != prev->stencil_depth_pass) {
        ngli_glStencilOp(gl,
                         next->stencil_fail,
                         next->stencil_depth_fail,
                         next->stencil_depth_pass);
    }
}
