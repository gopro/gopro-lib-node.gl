/*
 * Copyright 2016 GoPro Inc.
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
#include <stdlib.h>
#include <string.h>

#include "bstr.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "math_utils.h"
#include "params.h"
#include "utils.h"

#define CONSTS_4_IN_OUT(name, caps_name, formula, args, argsval)                                                                                                \
        {name "_in",     EASING_##caps_name##_IN,                                                                                                               \
                         .desc=NGLI_DOCSTRING("`" name "(x" argsval ")=" formula "`")},                                                                         \
        {name "_out",    EASING_##caps_name##_OUT,                                                                                                              \
                         .desc=NGLI_DOCSTRING("`" name "_out(x" argsval ")=1-" name "(1-x" args ")`")},                                                         \
        {name "_in_out", EASING_##caps_name##_IN_OUT,                                                                                                           \
                         .desc=NGLI_DOCSTRING("`" name "_in_out(x" argsval ")=" name "(2x" args ")/2` if `x<½` else `1-" name "(2*(1-x)" args ")/2`")},         \
        {name "_out_in", EASING_##caps_name##_OUT_IN,                                                                                                           \
                         .desc=NGLI_DOCSTRING("`" name "_out_in(x" argsval ")=(1-" name "(1-2x" args "))/2` if `x<½` else `(1+" name "(2x-1" args "))/2`")}     \

static const struct param_choices easing_choices = {
    .name = "easing",
    .consts = {
        {"linear",           EASING_LINEAR,           .desc=NGLI_DOCSTRING("`linear(x)=x`")},
        CONSTS_4_IN_OUT("quadratic", QUADRATIC, "x²",                 "",   ""),
        CONSTS_4_IN_OUT("cubic",     CUBIC,     "x³",                 "",   ""),
        CONSTS_4_IN_OUT("quartic",   QUARTIC,   "x⁴",                 "",   ""),
        CONSTS_4_IN_OUT("quintic",   QUINTIC,   "x⁵",                 "",   ""),
        CONSTS_4_IN_OUT("power",     POWER,     "x^a",                ",a", ",a=1"),
        CONSTS_4_IN_OUT("sinus",     SINUS,     "1-cos(x*π/2)",       "",   ""),
        CONSTS_4_IN_OUT("exp",       EXP,       "(pow(a,x)-1)/(a-1)", ",a", ",a=1024"),
        CONSTS_4_IN_OUT("circular",  CIRCULAR,  "1-√(1-x²)",          "",   ""),
        {"bounce_in",        EASING_BOUNCE_IN,        .desc=NGLI_DOCSTRING("bouncing from right to left 4 times")},
        {"bounce_out",       EASING_BOUNCE_OUT,       .desc=NGLI_DOCSTRING("diagonally mirrored version of `bounce_in()`")},
        {"elastic_in",       EASING_ELASTIC_IN,       .desc=NGLI_DOCSTRING("elastic effect from weak to strong")},
        {"elastic_out",      EASING_ELASTIC_OUT,      .desc=NGLI_DOCSTRING("mirrored `elastic_in` effect")},
        {"back_in",          EASING_BACK_IN,          .desc=NGLI_DOCSTRING("mirrored `back_out` effect")},
        {"back_out",         EASING_BACK_OUT,         .desc=NGLI_DOCSTRING("overstep target value and smoothly converge back to it")},
        {"back_in_out",      EASING_BACK_IN_OUT,      .desc=NGLI_DOCSTRING("combination of `back_in` then `back_out`")},
        {"back_out_in",      EASING_BACK_OUT_IN,      .desc=NGLI_DOCSTRING("combination of `back_out` then `back_in`")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct animkeyframe_priv, x)

#define ANIMKEYFRAME_PARAMS(id, value_data_key, value_data_type, value_data_field)                      \
static const struct node_param animkeyframe##id##_params[] = {                                          \
    {"time",                 NGLI_PARAM_TYPE_DBL, OFFSET(time),                                         \
                             .desc=NGLI_DOCSTRING("the time key point in seconds")},                    \
    {#value_data_key,        value_data_type, OFFSET(value_data_field),                                 \
                             .desc=NGLI_DOCSTRING("the " #value_data_key " at time `time`")},           \
    {"easing",               NGLI_PARAM_TYPE_SELECT,  OFFSET(easing), {.i64=EASING_LINEAR},             \
                             .choices=&easing_choices,                                                  \
                             .desc=NGLI_DOCSTRING("easing interpolation from previous key frame")},     \
    {"easing_args",          NGLI_PARAM_TYPE_DBLLIST, OFFSET(args),                                     \
                             .desc=NGLI_DOCSTRING("a list of arguments some easings may use")},         \
    {"easing_start_offset",  NGLI_PARAM_TYPE_DBL, OFFSET(offsets[0]), {.dbl=0},                         \
                             .desc=NGLI_DOCSTRING("starting offset of the truncation of the easing")},  \
    {"easing_end_offset",    NGLI_PARAM_TYPE_DBL, OFFSET(offsets[1]), {.dbl=1},                         \
                             .desc=NGLI_DOCSTRING("ending offset of the truncation of the easing")},    \
    {NULL}                                                                                              \
}

ANIMKEYFRAME_PARAMS(float, value, NGLI_PARAM_TYPE_DBL, scalar);
ANIMKEYFRAME_PARAMS(vec2,  value, NGLI_PARAM_TYPE_VEC2, value);
ANIMKEYFRAME_PARAMS(vec3,  value, NGLI_PARAM_TYPE_VEC3, value);
ANIMKEYFRAME_PARAMS(vec4,  value, NGLI_PARAM_TYPE_VEC4, value);
ANIMKEYFRAME_PARAMS(quat,  quat,  NGLI_PARAM_TYPE_VEC4, value);
ANIMKEYFRAME_PARAMS(buffer, data, NGLI_PARAM_TYPE_DATA, data);

#define TRANSFORM_IN(f, x)     f(x, args_nb, args)
#define TRANSFORM_OUT(f, x)    (1.0 - TRANSFORM_IN(f, 1.0 - (x)))
#define TRANSFORM_IN_OUT(f, x) (((x) < 0.5 ? TRANSFORM_IN(f,  2.0 * (x)) : TRANSFORM_OUT(f, 2.0 * (x) - 1.0) + 1.0) / 2.0)
#define TRANSFORM_OUT_IN(f, x) (((x) < 0.5 ? TRANSFORM_OUT(f, 2.0 * (x)) : TRANSFORM_IN(f,  2.0 * (x) - 1.0) + 1.0) / 2.0)

#define DERIVATIVE_IN(df, x)     df(x, args_nb, args)
#define DERIVATIVE_OUT(df, x)    DERIVATIVE_IN(df, 1.0 - (x))
#define DERIVATIVE_IN_OUT(df, x) ((x) < 0.5 ? DERIVATIVE_IN(df,  2.0 * (x)) : DERIVATIVE_OUT(df, 2.0 * (x) - 1.0))
#define DERIVATIVE_OUT_IN(df, x) ((x) < 0.5 ? DERIVATIVE_OUT(df, 2.0 * (x)) : DERIVATIVE_IN(df,  2.0 * (x) - 1.0))

#define DECLARE_EASING(base_name, name, transform)                            \
static easing_type name(easing_type x, int args_nb, const easing_type *args)  \
{                                                                             \
    return transform(base_name##_helper, x);                                  \
}

#define DECLARE_HELPER(base_name, formula)                                                        \
static inline easing_type base_name##_helper(easing_type x, int args_nb, const easing_type *args) \
{                                                                                                 \
    return formula;                                                                               \
}

#define DECLARE_EASINGS(base_name, suffix, formula, type_base) \
DECLARE_HELPER(base_name##suffix, formula) \
DECLARE_EASING(base_name##suffix, base_name##_in##suffix,     type_base##_IN)       \
DECLARE_EASING(base_name##suffix, base_name##_out##suffix,    type_base##_OUT)      \
DECLARE_EASING(base_name##suffix, base_name##_in_out##suffix, type_base##_IN_OUT)   \
DECLARE_EASING(base_name##suffix, base_name##_out_in##suffix, type_base##_OUT_IN)

#define DECLARE_EASINGS_DERIVATIVES_RESOLUTION(base_name, direct_function, derivative_function, resolution_function)   \
DECLARE_EASINGS(base_name,            , direct_function,     TRANSFORM)                                                \
DECLARE_EASINGS(base_name, _derivative, derivative_function, DERIVATIVE)                                               \
DECLARE_EASINGS(base_name, _resolution, resolution_function, TRANSFORM)                                                \

#define PARAM(index, default_value) (args_nb > index ? args[index] : default_value)


/* Linear */

static easing_type linear(easing_type t, int args_nb, const easing_type *args)
{
    return t;
}

static easing_type linear_derivative(easing_type t, int args_nb, const easing_type *args)
{
    return 1.0;
}

static easing_type linear_resolution(easing_type v, int args_nb, const easing_type *args)
{
    return v;
}


DECLARE_EASINGS_DERIVATIVES_RESOLUTION(quadratic, x * x,             2 * x,             sqrt(x))
DECLARE_EASINGS_DERIVATIVES_RESOLUTION(cubic,     x * x * x,         3 * x * x,         pow(x, 1.0 / 3.0))
DECLARE_EASINGS_DERIVATIVES_RESOLUTION(quartic,   x * x * x * x,     4 * x * x * x,     pow(x, 1.0 / 4.0))
DECLARE_EASINGS_DERIVATIVES_RESOLUTION(quintic,   x * x * x * x * x, 5 * x * x * x * x, pow(x, 1.0 / 5.0))

DECLARE_EASINGS_DERIVATIVES_RESOLUTION(power,
                                       pow(x, PARAM(0, 1.0)),
                                       PARAM(0, 1.0) * pow(x, PARAM(0, 1.0) - 1.0),
                                       pow(x, 1.0 / PARAM(0, 1.0)))

DECLARE_EASINGS_DERIVATIVES_RESOLUTION(sinus,
                                       1.0 - cos(x * M_PI / 2.0),
                                       M_PI * sin(x * M_PI / 2.0) / 2.0,
                                       acos(1.0 - x) / M_PI * 2.0)

DECLARE_EASINGS_DERIVATIVES_RESOLUTION(circular,
                                       1.0 - sqrt(1.0 - x * x),
                                       x / sqrt(1.0 - x * x),
                                       sqrt(x*(2.0 - x)))


/* Exponential */

static inline easing_type exp_func(easing_type x, easing_type exp_base)
{
    return NGLI_LINEAR_INTERP(1.0, exp_base, pow(exp_base, x));
}

static inline easing_type exp_derivative(easing_type x, easing_type exp_base)
{
    return (pow(exp_base, x) * log(exp_base)) / (exp_base - 1.0);
}

static inline easing_type exp_resolution_func(easing_type x, easing_type exp_base)
{
    return log2(x * (exp_base - 1.0) + 1.0) / log2(exp_base);
}

DECLARE_EASINGS_DERIVATIVES_RESOLUTION(exp,
                                       exp_func(x, PARAM(0, 1024.0)),
                                       exp_derivative(x, PARAM(0, 1024.0)),
                                       exp_resolution_func(x, PARAM(0, 1024.0)))


/* Bounce */

static easing_type bounce_helper(easing_type t, easing_type a)
{
    if (t == 1.0) {
        return 1.0;
    } else if (t < 4.0 / 11.0) {
        return 7.5625 * t * t;
    } else if (t < 8.0 / 11.0) {
        t -= 6.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.75)) + 1.0;
    } else if (t < 10.0 / 11.0) {
        t -= 9.0 / 11.0;
        return -a * (1.0 - (7.5625 * t * t + 0.9375)) + 1.0;
    } else {
        t -= 21.0 / 22.0;
        return -a * (1.0 - (7.5625 * t * t + 0.984375)) + 1.0;
    }
}

static easing_type bounce_helper_derivative(easing_type t, easing_type a)
{
    if (t == 1.0)
        return 0.0;
    if (t < 4.0 / 11.0)
        return 7.5625 * 2 * t;
    if (t < 8.0 / 11.0)
        t -= 6.0 / 11.0;
    else if (t < 10.0 / 11.0)
        t -= 9.0 / 11.0;
    else
        t -= 21.0 / 22.0;
    return 7.5625 * 2 * a * t;
}

static easing_type bounce_in(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type a = PARAM(0, 1.70158);
    return 1.0 - bounce_helper(1.0 - t, a);
}

static easing_type bounce_in_derivative(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type a = PARAM(0, 1.70158);
    return bounce_helper_derivative(1.0 - t, a);
}

static easing_type bounce_out(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type a = PARAM(0, 1.70158);
    return bounce_helper(t, a);
}

static easing_type bounce_out_derivative(easing_type t, int args_nb, const easing_type *args)
{
    const easing_type a = PARAM(0, 1.70158);
    return bounce_helper_derivative(t, a);
}


/* Elastic */

static easing_type elastic_in(easing_type t, int args_nb, const easing_type *args)
{
    if (t <= 0.0)
        return 0.0;
    if (t >= 1.0)
        return 1.0;
    easing_type a = PARAM(0, 0.1); // amplitude
    const easing_type p = PARAM(1, 0.25); // period
    easing_type s;
    if (a < 1.0) {
        a = 1.0;
        s = p / 4.0;
    } else {
        s = p / (2.0 * M_PI) * asin(1.0 / a);
    }
    return -a * exp2(10.0 * (t - 1.0)) * sin((1.0 - t - s) * (2.0 * M_PI) / p);
}

static easing_type elastic_in_derivative(easing_type t, int args_nb, const easing_type *args)
{
    easing_type a = PARAM(0, 0.1); // amplitude
    const easing_type p = PARAM(1, 0.25); // period
    easing_type s;
    if (a < 1.0) {
        a = 1.0;
        s = p / 4.0;
    } else {
        s = p / (2.0 * M_PI) * asin(1.0 / a);
    }
    const easing_type k = (s + t - 1.0) * 2.0 * M_PI / p;
    return a * exp2(10.0 * (t - 1.0)) * (10.0 * p * log(2.0) * sin(k) + 2.0 * M_PI * cos(k)) / p;
}

static easing_type elastic_out(easing_type t, int args_nb, const easing_type *args)
{
    return TRANSFORM_OUT(elastic_in, t);
}

static easing_type elastic_out_derivative(easing_type t, int args_nb, const easing_type *args)
{
    return DERIVATIVE_OUT(elastic_in_derivative, t);
}


/* Back */

static easing_type back_func(easing_type t, easing_type s)
{
    return t * t * ((s + 1.0) * t - s);
}

static easing_type back_derivative(easing_type t, easing_type s)
{
    return s * (3.0 * t - 2.0) * t + 3.0 * t * t;
}


DECLARE_EASINGS(back,            , back_func(x, PARAM(0, 1.70158)), TRANSFORM)
DECLARE_EASINGS(back, _derivative, back_derivative(x, PARAM(0, 1.70158)), DERIVATIVE)

static const struct {
    easing_function function;
    easing_function derivative;
    easing_function resolution;
} easings[] = {
    [EASING_LINEAR]           = {linear,                 linear_derivative,             linear_resolution},
    [EASING_QUADRATIC_IN]     = {quadratic_in,           quadratic_in_derivative,       quadratic_in_resolution},
    [EASING_QUADRATIC_OUT]    = {quadratic_out,          quadratic_out_derivative,      quadratic_out_resolution},
    [EASING_QUADRATIC_IN_OUT] = {quadratic_in_out,       quadratic_in_out_derivative,   quadratic_in_out_resolution},
    [EASING_QUADRATIC_OUT_IN] = {quadratic_out_in,       quadratic_out_in_derivative,   quadratic_out_in_resolution},
    [EASING_CUBIC_IN]         = {cubic_in,               cubic_in_derivative,           cubic_in_resolution},
    [EASING_CUBIC_OUT]        = {cubic_out,              cubic_out_derivative,          cubic_out_resolution},
    [EASING_CUBIC_IN_OUT]     = {cubic_in_out,           cubic_in_out_derivative,       cubic_in_out_resolution},
    [EASING_CUBIC_OUT_IN]     = {cubic_out_in,           cubic_out_in_derivative,       cubic_out_in_resolution},
    [EASING_QUARTIC_IN]       = {quartic_in,             quartic_in_derivative,         quartic_in_resolution},
    [EASING_QUARTIC_OUT]      = {quartic_out,            quartic_out_derivative,        quartic_out_resolution},
    [EASING_QUARTIC_IN_OUT]   = {quartic_in_out,         quartic_in_out_derivative,     quartic_in_out_resolution},
    [EASING_QUARTIC_OUT_IN]   = {quartic_out_in,         quartic_out_in_derivative,     quartic_out_in_resolution},
    [EASING_QUINTIC_IN]       = {quintic_in,             quintic_in_derivative,         quintic_in_resolution},
    [EASING_QUINTIC_OUT]      = {quintic_out,            quintic_out_derivative,        quintic_out_resolution},
    [EASING_QUINTIC_IN_OUT]   = {quintic_in_out,         quintic_in_out_derivative,     quintic_in_out_resolution},
    [EASING_QUINTIC_OUT_IN]   = {quintic_out_in,         quintic_out_in_derivative,     quintic_out_in_resolution},
    [EASING_POWER_IN]         = {power_in,               power_in_derivative,           power_in_resolution},
    [EASING_POWER_OUT]        = {power_out,              power_out_derivative,          power_out_resolution},
    [EASING_POWER_IN_OUT]     = {power_in_out,           power_in_out_derivative,       power_in_out_resolution},
    [EASING_POWER_OUT_IN]     = {power_out_in,           power_out_in_derivative,       power_out_in_resolution},
    [EASING_SINUS_IN]         = {sinus_in,               sinus_in_derivative,           sinus_in_resolution},
    [EASING_SINUS_OUT]        = {sinus_out,              sinus_out_derivative,          sinus_out_resolution},
    [EASING_SINUS_IN_OUT]     = {sinus_in_out,           sinus_in_out_derivative,       sinus_in_out_resolution},
    [EASING_SINUS_OUT_IN]     = {sinus_out_in,           sinus_out_in_derivative,       sinus_out_in_resolution},
    [EASING_EXP_IN]           = {exp_in,                 exp_in_derivative,             exp_in_resolution},
    [EASING_EXP_OUT]          = {exp_out,                exp_out_derivative,            exp_out_resolution},
    [EASING_EXP_IN_OUT]       = {exp_in_out,             exp_in_out_derivative,         exp_in_out_resolution},
    [EASING_EXP_OUT_IN]       = {exp_out_in,             exp_out_in_derivative,         exp_out_in_resolution},
    [EASING_CIRCULAR_IN]      = {circular_in,            circular_in_derivative,        circular_in_resolution},
    [EASING_CIRCULAR_OUT]     = {circular_out,           circular_out_derivative,       circular_out_resolution},
    [EASING_CIRCULAR_IN_OUT]  = {circular_in_out,        circular_in_out_derivative,    circular_in_out_resolution},
    [EASING_CIRCULAR_OUT_IN]  = {circular_out_in,        circular_out_in_derivative,    circular_out_in_resolution},
    [EASING_BOUNCE_IN]        = {bounce_in,              bounce_in_derivative,          NULL},
    [EASING_BOUNCE_OUT]       = {bounce_out,             bounce_out_derivative,         NULL},
    [EASING_ELASTIC_IN]       = {elastic_in,             elastic_in_derivative,         NULL},
    [EASING_ELASTIC_OUT]      = {elastic_out,            elastic_out_derivative,        NULL},
    [EASING_BACK_IN]          = {back_in,                back_in_derivative,            NULL},
    [EASING_BACK_OUT]         = {back_out,               back_out_derivative,           NULL},
    [EASING_BACK_IN_OUT]      = {back_in_out,            back_in_out_derivative,        NULL},
    [EASING_BACK_OUT_IN]      = {back_out_in,            back_out_in_derivative,        NULL},
};

static int check_offsets(double x0, double x1)
{
    if (x0 >= x1 || x0 < 0.0 || x1 > 1.0) {
        LOG(ERROR, "Truncation offsets must met the following requirements: 0 <= off0 < off1 <= 1");
        return NGL_ERROR_INVALID_ARG;
    }
    return 0;
}

static int check_boundaries(double y0, double y1)
{
    if (y0 == y1) {
        LOG(ERROR, "Boundaries (as defined by the offsets) can not be identical");
        return NGL_ERROR_UNSUPPORTED;
    }
    return 0;
}

static int animkeyframe_init(struct ngl_node *node)
{
    struct animkeyframe_priv *s = node->priv_data;

    const int easing_id = s->easing;
    const char *easing_name = ngli_params_get_select_str(easing_choices.consts, easing_id);

    if (node->cls->id == NGL_NODE_ANIMKEYFRAMEVEC2)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f) for t=%f",
            node->cls->name, easing_name,
            s->value[0], s->value[1], s->time);
    else if (node->cls->id == NGL_NODE_ANIMKEYFRAMEVEC3)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f,%f) for t=%f",
            node->cls->name, easing_name,
            s->value[0], s->value[1], s->value[2], s->time);
    else if (node->cls->id == NGL_NODE_ANIMKEYFRAMEVEC4)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f,%f,%f) for t=%f",
            node->cls->name, easing_name,
            s->value[0], s->value[1], s->value[2], s->value[3], s->time);
    else if (node->cls->id == NGL_NODE_ANIMKEYFRAMEQUAT)
        LOG(VERBOSE, "%s of type %s starting at (%f,%f,%f,%f) for t=%f",
            node->cls->name, easing_name,
            s->value[0], s->value[1], s->value[2], s->value[3], s->time);
    else if (node->cls->id == NGL_NODE_ANIMKEYFRAMEFLOAT)
        LOG(VERBOSE, "%s of type %s starting at %f for t=%f",
            node->cls->name, easing_name,
            s->scalar, s->time);
    else if (node->cls->id == NGL_NODE_ANIMKEYFRAMEBUFFER)
        LOG(VERBOSE, "%s of type %s starting with t=%f (data size: %d)",
            node->cls->name, easing_name,
            s->time, s->data_size);
    else
        return NGL_ERROR_BUG;

    s->function   = easings[easing_id].function;
    s->derivative = easings[easing_id].derivative;
    s->resolution = easings[easing_id].resolution;

    const double x0 = s->offsets[0];
    const double x1 = s->offsets[1];
    if (x0 || x1 != 1.0) {
        int ret = check_offsets(x0, x1);
        if (ret < 0)
            return ret;
        s->scale_boundaries = 1;

        const double y0 = s->function(x0, s->nb_args, s->args);
        const double y1 = s->function(x1, s->nb_args, s->args);
        ret = check_boundaries(y0, y1);
        if (ret < 0)
            return ret;
        s->boundaries[0] = y0;
        s->boundaries[1] = y1;
        s->derivative_scale = (x1 - x0) / (y1 - y0);
    }

    return 0;
}

static char *animkeyframe_info_str(const struct ngl_node *node)
{
    const struct animkeyframe_priv *s = node->priv_data;
    const struct node_param *params = node->cls->params;
    struct bstr *b = ngli_bstr_create();

    if (!b)
        return NULL;

    const char *easing_name = ngli_params_get_select_str(easing_choices.consts, s->easing);
    ngli_bstr_printf(b, "%s @ t=%g ", easing_name, s->time);
    if (s->nb_args) {
        const struct node_param *easing_args_par = ngli_params_find(params, "easing_args");
        ngli_assert(easing_args_par);
        ngli_bstr_print(b, "(args: ");
        ngli_params_bstr_print_val(b, node->priv_data, easing_args_par);
        ngli_bstr_print(b, ") ");
    }

    if (s->offsets[0] || s->offsets[1] != 1.0) { // can not use scale_boundaries yet (not initialized)
        ngli_bstr_printf(b, "on (%g,%g) ", s->offsets[0], s->offsets[1]);
    }

    if (node->cls->id == NGL_NODE_ANIMKEYFRAMEBUFFER) {
        ngli_bstr_printf(b, "with data size of %dB", s->data_size);
    } else if (node->cls->id == NGL_NODE_ANIMKEYFRAMEQUAT) {
        ngli_bstr_printf(b, "with quat=(%g,%g,%g,%g)", NGLI_ARG_VEC4(s->value));
    } else {
        ngli_bstr_print(b, "with v=");
        const struct node_param *val_par = ngli_params_find(params, "value");
        ngli_assert(val_par);
        ngli_params_bstr_print_val(b, node->priv_data, val_par);
    }

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

int ngl_easing_evaluate(const char *name, const double *args, int nb_args,
                        const double *offsets, double t, double *v)
{
    int easing_id;
    int ret = ngli_params_get_select_val(easing_choices.consts, name, &easing_id);
    if (ret < 0)
        return ret;
    if (offsets) {
        ret = check_offsets(offsets[0], offsets[1]);
        if (ret < 0)
            return ret;
        t = NGLI_MIX(offsets[0], offsets[1], t);
    }
    const easing_function eval_func = easings[easing_id].function;
    double value = eval_func(t, nb_args, args);
    if (offsets) {
        const double y0 = eval_func(offsets[0], nb_args, args);
        const double y1 = eval_func(offsets[1], nb_args, args);
        ret = check_boundaries(y0, y1);
        if (ret < 0)
            return ret;
        value = NGLI_LINEAR_INTERP(y0, y1, value);
    }
    *v = value;
    return 0;
}

int ngl_easing_solve(const char *name, const double *args, int nb_args,
                     const double *offsets, double v, double *t)
{
    int easing_id;
    int ret = ngli_params_get_select_val(easing_choices.consts, name, &easing_id);
    if (ret < 0)
        return ret;
    if (!easings[easing_id].resolution) {
        LOG(ERROR, "no resolution available for easing %s", name);
        return NGL_ERROR_UNSUPPORTED;
    }
    if (offsets) {
        ret = check_offsets(offsets[0], offsets[1]);
        if (ret < 0)
            return ret;
        const easing_function eval_func = easings[easing_id].function;
        const double y0 = eval_func(offsets[0], nb_args, args);
        const double y1 = eval_func(offsets[1], nb_args, args);
        ret = check_boundaries(y0, y1);
        if (ret < 0)
            return ret;
        v = NGLI_MIX(y0, y1, v);
    }
    double time = easings[easing_id].resolution(v, nb_args, args);
    if (offsets)
        time = NGLI_LINEAR_INTERP(offsets[0], offsets[1], time);
    *t = time;
    return 0;
}

#define DECLARE_ANIMKF_CLASS(class_id, class_name, type)    \
const struct node_class ngli_animkeyframe##type##_class = { \
    .id        = class_id,                                  \
    .name      = class_name,                                \
    .init      = animkeyframe_init,                         \
    .info_str  = animkeyframe_info_str,                     \
    .priv_size = sizeof(struct animkeyframe_priv),          \
    .params    = animkeyframe##type##_params,               \
    .file      = __FILE__,                                  \
};

DECLARE_ANIMKF_CLASS(NGL_NODE_ANIMKEYFRAMEFLOAT,  "AnimKeyFrameFloat",  float)
DECLARE_ANIMKF_CLASS(NGL_NODE_ANIMKEYFRAMEVEC2,   "AnimKeyFrameVec2",   vec2)
DECLARE_ANIMKF_CLASS(NGL_NODE_ANIMKEYFRAMEVEC3,   "AnimKeyFrameVec3",   vec3)
DECLARE_ANIMKF_CLASS(NGL_NODE_ANIMKEYFRAMEVEC4,   "AnimKeyFrameVec4",   vec4)
DECLARE_ANIMKF_CLASS(NGL_NODE_ANIMKEYFRAMEBUFFER, "AnimKeyFrameBuffer", buffer)
DECLARE_ANIMKF_CLASS(NGL_NODE_ANIMKEYFRAMEQUAT,   "AnimKeyFrameQuat",   quat)
