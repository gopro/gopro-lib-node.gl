/*
 * Copyright 2020-2021 GoPro Inc.
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

#include <string.h>

#include "darray.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "path.h"

#define SEGMENT_FLAG_NEW_ORIGIN (1 << 0) /* the current segment does not overlap with the previous one */
#define SEGMENT_FLAG_LINE       (1 << 1) /* the current segment is a simple line (not a curve) */

struct path_segment {
    float poly_x[4];
    float poly_y[4];
    float poly_z[4];
    int step_start;
    float time_scale;
    uint32_t flags;
};

#define STEP_FLAG_DISCONTINUITY (1 << 0) /* a discontinuity happens after this step */

struct path_step {
    float position[3];
    int segment_id;
    uint32_t flags;
};

struct path {
    int precision;
    int current_arc;            /* cached arc index */
    int *arc_to_segment;        /* map arc indexes to segment indexes */
    struct darray segments;     /* array of struct path_segment */
    struct darray steps;        /* array of struct path_step */
    struct darray steps_dist;   /* array of floats */
    float cursor[3];            /* temporary cursor used during path construction */
    uint32_t segment_flags;     /* temporary segment flags used during path construction */
};

/*
 * Interpolate a 3D point using the polynomials
 */
static void poly_eval(float t, float *dst, const struct path_segment *segment)
{
    const float *x = segment->poly_x;
    const float *y = segment->poly_y;
    const float *z = segment->poly_z;
    dst[0] = NGLI_POLY3(x[0], x[1], x[2], x[3], t);
    dst[1] = NGLI_POLY3(y[0], y[1], y[2], y[3], t);
    dst[2] = NGLI_POLY3(z[0], z[1], z[2], z[3], t);
}

/*
 * Return the index of the vector where `value` belongs, starting the search
 * from index `*cache` (and looping back to 0 if necessary). A vector is
 * defined by 2 consecutive points in the `values` array, with `values`
 * composed of monotonically increasing values.
 *
 * The range of the returned index is within [0;nb_values-2].
 *
 * Example:
 *   values:  2 3 5 8 9
 *   indexes: |0|1|2|3|
 *
 *    input   | output  |
 *    value   | index   | comment
 *   -------- | ------- | -------
 *       6    |   2     | value is between 5 and 8
 *       1    |   0     | before start value, clamped to index 0
 *      15    |   3     | after end value, clamped to last index
 *
 */
static int get_vector_id(const float *values, int nb_values, int *cache, float value)
{
    int ret = -1;
    const int nb_indexes = nb_values - 1;
    const int start = *cache;

    for (int i = start; i < nb_indexes; i++) {
        if (values[i] > value)
            break;
        ret = i;
    }
    if (ret < 0) {
        for (int i = 0; i < nb_indexes; i++) {
            if (values[i] > value)
                break;
            ret = i;
        }
    }
    /*
     * We only need to clamp the negative boundary because ret can never reach
     * nb_indexes, meaning the maximum value is nb_indexes-1, or nb_values-2.
     */
    ret = NGLI_MAX(ret, 0);
    *cache = ret;
    return ret;
}

/* Remap x from [c;d] to [a;b] */
static float remap(float a, float b, float c, float d, float x)
{
    const float ratio = NGLI_LINEAR_INTERP(c, d, x);
    return NGLI_MIX(a, b, ratio);
}

/*
 * Convert the requested path distance into a segment curve "time" using the
 * estimated arc lengths, and use it to evaluate the polynomial of the current
 * segment.
 *
 * We want the time parameter of B(t) to be correlated to the traveled distance
 * on the bézier curves. Not doing this remapping would cause a non-linear
 * traveling of a point on that curve: the more a curve is twisted, the more
 * steps it contains, and thus the slower the movement would get.
 *
 * Unfortunately, there is no magic formula to get the length of a bézier
 * curve, so we have to rely on a simple approximation by dividing the segments
 * into many small arcs.
 *
 * https://pomax.github.io/bezierinfo/#arclength
 * https://pomax.github.io/bezierinfo/#arclengthapprox
 * https://pomax.github.io/bezierinfo/#tracing
 */
void ngli_path_evaluate(struct path *s, float *dst, float distance)
{
    const float *distances = ngli_darray_data(&s->steps_dist);
    const int nb_dists = ngli_darray_count(&s->steps_dist);
    const int arc_id = get_vector_id(distances, nb_dists, &s->current_arc, distance);
    const int segment_id = s->arc_to_segment[arc_id];
    const struct path_segment *segments = ngli_darray_data(&s->segments);
    const struct path_segment *segment = &segments[segment_id];
    const int step0 = arc_id;
    const int step1 = arc_id + 1;
    const float t0 = (step0 - segment->step_start) * segment->time_scale;
    const float t1 = (step1 - segment->step_start) * segment->time_scale;
    const float d0 = distances[step0];
    const float d1 = distances[step1];
    const float t = remap(t0, t1, d0, d1, distance);
    poly_eval(t, dst, segment);
}

struct path *ngli_path_create(void)
{
    struct path *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    ngli_darray_init(&s->steps, sizeof(struct path_step), 0);
    ngli_darray_init(&s->steps_dist, sizeof(float), 0);
    ngli_darray_init(&s->segments, sizeof(struct path_segment), 0);
    return s;
}

/*
 * Convert from original bézier quadratic form:
 *   B₂(t) = (1-t)² p0 + 2(1-t)t p1 + t² p2
 * To polynomial form:
 *   B₂(t) = at² + bt + c
 */
static void poly_from_bezier2(float *dst, float p0, float p1, float p2)
{
    const float a =      p0 - 2.f*p1 + p2;
    const float b = -2.f*p0 + 2.f*p1;
    const float c =      p0;
    dst[0] = 0.f;
    dst[1] = a;
    dst[2] = b;
    dst[3] = c;
}

/*
 * Convert from original bézier cubic form:
 *   B₃(t) = (1-t)³ p0 + 3(1-t)²t p1 + 3(1-t)t² p2 + t³ p3
 * To polynomial form:
 *   B₃(t) = at³ + bt² + ct + d
 */
static void poly_from_bezier3(float *dst, float p0, float p1, float p2, float p3)
{
    const float a =     -p0 + 3.f*p1 - 3.f*p2 + p3;
    const float b =  3.f*p0 - 6.f*p1 + 3.f*p2;
    const float c = -3.f*p0 + 3.f*p1;
    const float d =      p0;
    dst[0] = a;
    dst[1] = b;
    dst[2] = c;
    dst[3] = d;
}

int ngli_path_move_to(struct path *s, const float *to)
{
    memcpy(s->cursor, to, sizeof(s->cursor));
    s->segment_flags |= SEGMENT_FLAG_NEW_ORIGIN;
    return 0;
}

static int add_segment_and_move(struct path *s, const struct path_segment *segment, const float *to)
{
    if (!ngli_darray_push(&s->segments, segment))
        return NGL_ERROR_MEMORY;
    memcpy(s->cursor, to, sizeof(s->cursor));
    s->segment_flags &= ~SEGMENT_FLAG_NEW_ORIGIN;
    return 0;
}

int ngli_path_line_to(struct path *s, const float *to)
{
    const struct path_segment segment = {
        .poly_x = {0.f, 0.f, to[0] - s->cursor[0], s->cursor[0]},
        .poly_y = {0.f, 0.f, to[1] - s->cursor[1], s->cursor[1]},
        .poly_z = {0.f, 0.f, to[2] - s->cursor[2], s->cursor[2]},
        .flags  = s->segment_flags | SEGMENT_FLAG_LINE,
    };
    return add_segment_and_move(s, &segment, to);
}

int ngli_path_bezier2_to(struct path *s, const float *ctl, const float *to)
{
    struct path_segment segment = {.flags = s->segment_flags};
    poly_from_bezier2(segment.poly_x, s->cursor[0], ctl[0], to[0]);
    poly_from_bezier2(segment.poly_y, s->cursor[1], ctl[1], to[1]);
    poly_from_bezier2(segment.poly_z, s->cursor[2], ctl[2], to[2]);
    return add_segment_and_move(s, &segment, to);
}

int ngli_path_bezier3_to(struct path *s, const float *ctl0, const float *ctl1, const float *to)
{
    struct path_segment segment = {.flags = s->segment_flags};
    poly_from_bezier3(segment.poly_x, s->cursor[0], ctl0[0], ctl1[0], to[0]);
    poly_from_bezier3(segment.poly_y, s->cursor[1], ctl0[1], ctl1[1], to[1]);
    poly_from_bezier3(segment.poly_z, s->cursor[2], ctl0[2], ctl1[2], to[2]);
    return add_segment_and_move(s, &segment, to);
}

/*
 * Lexicon:
 *
 * - path: a set of segments, eventually with discontinuities (in case a move
 *   event occurred during the path construction)
 * - segment: the curve between 2 points forms a segment. Each segment can be a
 *   line, a quadratic bézier (1 control point) or a cubic bézier curve (2
 *   control points), all of them respectively expressed as a polynomials of
 *   the 1st, 2nd and 3rd degree. The segments form a chain where the end
 *   coordinate of one segment overlaps with the starting point of the next
 *   segment.
 * - step: a step is a coordinate on the curve; every segment is divided
 *   into an arbitrary number of `precision` steps.
 * - dist: growing distance between the origin of the path up to a given step:
 *   those are approximations of an arc length.
 * - arc: 2 steps form an arc, it represents a (usually small) chunk of a
 *   segment which can be approximated into a small straight line instead of a
 *   curved one.
 * - time: the "time" does *NOT* correspond to the time of a potential
 *   animation, it corresponds to the parameter passed down to the polynomial
 *   evaluation. With curves, this time is *NOT* correlated with the real clock
 *   time at all. See ngli_path_evaluate() for more information.
 */
int ngli_path_init(struct path *s, int precision)
{
    if (precision < 1) {
        LOG(ERROR, "precision must be 1 or superior");
        return NGL_ERROR_INVALID_ARG;
    }
    s->precision = precision;

    const int nb_segments = ngli_darray_count(&s->segments);
    if (nb_segments < 1) {
        LOG(ERROR, "at least one segment must be defined");
        return NGL_ERROR_INVALID_ARG;
    }

    /*
     * Build a temporary lookup table of data points ("steps") that will be
     * used for estimating the length (growing distances more specifically) of
     * the curve.
     */
    struct path_segment *segments = ngli_darray_data(&s->segments);
    for (int i = 0; i < nb_segments; i++) {
        struct path_segment *segment = &segments[i];

        /*
         * Compared to curves, straight lines do not need to be divided into
         * small chunks because their length can be calculated exactly.
         */
        const int precision = (segment->flags & SEGMENT_FLAG_LINE) ? 1 : s->precision;

        /*
         * We're not using 1/(P-1) but 1/P for the scale because each segment is
         * composed of P+1 step points.
         */
        segment->step_start = ngli_darray_count(&s->steps);
        segment->time_scale = 1.f / precision;

        /*
         * This loop only calculates P step coordinates per segment instead of
         * P+1 because the last step of a segment (at t=1) overlaps with the
         * first step of the next segment (t=0). The two exceptions to this are
         * handled in the next block.
         */
        for (int k = 0; k < precision; k++) {
            const float t = k * segment->time_scale;
            struct path_step step = {.segment_id=i};
            poly_eval(t, step.position, segment);
            if (!ngli_darray_push(&s->steps, &step))
                return NGL_ERROR_MEMORY;
        }

        /*
         * We check if we are at the very last step of the last segment, or in
         * the situation where a move order occurred between the current
         * segment and the next one. These 2 scenarios imply that we compute
         * the last point coordinate (t=1) of the current segment because there
         * won't be an overlap with the next segment (if any).
         */
        if (i == nb_segments - 1 || (segments[i + 1].flags & SEGMENT_FLAG_NEW_ORIGIN)) {
            struct path_step step = {.segment_id=i, .flags = STEP_FLAG_DISCONTINUITY};
            poly_eval(1.f, step.position, &segments[i]);
            if (!ngli_darray_push(&s->steps, &step))
                return NGL_ERROR_MEMORY;
        }
    }

    /*
     * Build the growing distance (from step 0) of steps (including step 0).
     */
    float total_length = 0.f;

    if (!ngli_darray_push(&s->steps_dist, &total_length))
        return NGL_ERROR_MEMORY;

    const struct path_step *steps = ngli_darray_data(&s->steps);
    for (int i = 1; i < ngli_darray_count(&s->steps); i++) {
        const struct path_step *prv_step = &steps[i - 1];
        const struct path_step *cur_step = &steps[i];

        const int skip_distance = (prv_step->flags & STEP_FLAG_DISCONTINUITY);
        if (!skip_distance) {
            const float arc_vec[3] = NGLI_VEC3_SUB(cur_step->position, prv_step->position);
            const float arc_length = ngli_vec3_length(arc_vec);
            total_length += arc_length;
        }
        if (!ngli_darray_push(&s->steps_dist, &total_length))
            return NGL_ERROR_MEMORY;
    }

    /*
     * We have the same number of steps and distances between these steps
     * because the first step starts with a distance of 0.
     */
    ngli_assert(ngli_darray_count(&s->steps) == ngli_darray_count(&s->steps_dist));

    /*
     * Sanity check for get_vector_id(). We have it here to avoid having the
     * assert called redundantly in the inner loop.
     */
    ngli_assert(ngli_darray_count(&s->steps_dist) - 1 >= 1); // checks if number of arcs >= 1

    /* Normalize distances (relative to the total length of the path) */
    float *steps_dist = ngli_darray_data(&s->steps_dist);
    const float scale = total_length ? 1.f / total_length : 0.f;
    for (int i = 0; i < ngli_darray_count(&s->steps_dist); i++)
        steps_dist[i] *= scale;

    /* Build a lookup table associating an arc to its segment */
    const int nb_arcs = ngli_darray_count(&s->steps) - 1;
    s->arc_to_segment = ngli_calloc(nb_arcs, sizeof(*s->arc_to_segment));
    if (!s->arc_to_segment)
        return NGL_ERROR_MEMORY;
    for (int i = 0; i < nb_arcs; i++)
        s->arc_to_segment[i] = steps[i].segment_id;

    /* We don't need to store all the intermediate positions anymore */
    ngli_darray_reset(&s->steps);

    return 0;
}

void ngli_path_freep(struct path **sp)
{
    struct path *s = *sp;
    if (!s)
        return;
    ngli_freep(&s->arc_to_segment);
    ngli_darray_reset(&s->segments);
    ngli_darray_reset(&s->steps);
    ngli_darray_reset(&s->steps_dist);
    ngli_freep(sp);
}
