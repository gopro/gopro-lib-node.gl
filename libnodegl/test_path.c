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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "path.h"
#include "utils.h"

#define NB_REFS (16 + 2)
#define MAX_ERR 1e-5

static int check_value(struct path *path, float t, const float *ref)
{
    float value[3];
    ngli_path_evaluate(path, value, t);
    if (!ref) {
        fprintf(stderr, "got:("NGLI_FMT_VEC3")\n", NGLI_ARG_VEC3(value));
        return 1;
    }
    const float err[3] = {
        fabs(value[0] - ref[0]),
        fabs(value[1] - ref[1]),
        fabs(value[2] - ref[2]),
    };
    int ret = err[0] > MAX_ERR || err[1] > MAX_ERR || err[2] > MAX_ERR ||
              isnan(err[0]) || isnan(err[1]) || isnan(err[2]);
    fprintf(stderr, "%c t:%9f ref:("NGLI_FMT_VEC3") got:("NGLI_FMT_VEC3") err:("NGLI_FMT_VEC3")\n",
            ret ? '!' : ' ', t, NGLI_ARG_VEC3(ref), NGLI_ARG_VEC3(value), NGLI_ARG_VEC3(err));
    return ret;
}

/* Set refs=NULL to get the references if you don't have any yet */
static int evaluate_points(struct path *path, const float *refs, const char *title)
{
    printf("test: %s\n", title);

    int ret = 0;
    for (int i = 0; i < NB_REFS; i++) {
        /* We make sure t starts before 0 and ends after 1 to check for
         * outbounds */
        const float t = (i - 1) / ((NB_REFS - 2) - 1.f);
        ret |= check_value(path, t, refs ? &refs[i * 3] : NULL);
    }

    if (ret) {
        fprintf(stderr, "%s failed\n", title);
        return -1;
    }

    return 0;
}

static int test_bezier3_vec3(void)
{
    struct path *path = ngli_path_create();
    if (!path)
        return -1;

    static const float points[][3] = {
        {-0.7,  0.0,  0.3},
        { 0.8,  0.1, -0.1},
    };

    static const float controls[][3] = {
        {-0.2, -0.3,  0.2},
        { 0.2,  0.8,  0.4},
    };

    static const float refs[] = {
       -0.819247,  0.0975617,   0.329433,
            -0.7,        0.0,        0.3,
       -0.584421, -0.0462202,   0.281569,
       -0.471648, -0.0502734,   0.271273,
       -0.360823, -0.0213343,   0.266244,
       -0.251085,  0.0314227,   0.263616,
       -0.151888,  0.0921073,   0.260911,
      -0.0559586,   0.155526,   0.255954,
       0.0409618,   0.217673,   0.246764,
        0.139444,   0.272461,   0.231438,
        0.240059,     0.3138,   0.208073,
        0.328808,   0.333964,    0.18007,
        0.417249,   0.336013,    0.14468,
        0.508256,   0.317196,   0.100088,
        0.602161,   0.273958,  0.0451851,
        0.699298,   0.202744, -0.0211406,
             0.8,        0.1,       -0.1,
        0.904601, -0.0378311,  -0.192504,
    };

    int ret;
    if ((ret = ngli_path_move_to(path, points[0])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[0], controls[1], points[1])) < 0)
        goto end;

    ret = ngli_path_init(path, 3);
    if (ret < 0)
        goto end;

    ret = evaluate_points(path, refs, "3D cubic bezier");

end:
    ngli_path_freep(&path);
    return ret;
}

static int test_poly_bezier3(void)
{
    static const float points[][3] = {
        {-0.70,  0.08, 0.0},
        {-0.15,  0.06, 0.0},
        {-0.24,  0.52, 0.0},
        { 0.23,  0.15, 0.0},
        { 0.05, -0.25, 0.0},
    };

    static const float controls[][3] = {
        { 0.45, -0.59, 0.0},
        {-1.1,  -0.47, 0.0},
        { 0.25,  0.29, 0.0},
        {-0.19, -1.1,  0.0},
        {-0.25,  1.1,  0.0},
        { 0.19, -0.75, 0.0},
        { 0.0,   0.96, 0.0},
        { 1.1,  -0.86, 0.0},
    };

    static const float refs[] = {
         -1.1044,   0.292227, 0.0,
            -0.7,       0.08, 0.0,
       -0.396405,   -0.12542, 0.0,
       -0.339952,  -0.380683, 0.0,
        -0.39831,  -0.117848, 0.0,
      -0.0945053,  0.0833274, 0.0,
      -0.0141996,  -0.206802, 0.0,
       -0.190295, -0.0496436, 0.0,
       -0.230724,   0.314766, 0.0,
       -0.192237,   0.608198, 0.0,
      -0.0468001,   0.271875, 0.0,
            0.11, -0.0588156, 0.0,
        0.223503,   0.173489, 0.0,
        0.331497,   0.245757, 0.0,
        0.486813, -0.0851905, 0.0,
        0.390109,   -0.37754, 0.0,
            0.05,  -0.250001, 0.0,
        -0.35465,  0.0274324, 0.0,
    };

    struct path *path = ngli_path_create();
    if (!path)
        return -1;

    int ret;
    if ((ret = ngli_path_move_to(path, points[0])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[0], controls[1], points[1])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[2], controls[3], points[2])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[4], controls[5], points[3])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[6], controls[7], points[4])) < 0)
        goto end;

    ret = ngli_path_init(path, 64);
    if (ret < 0)
        goto end;

    ret = evaluate_points(path, refs, "cubic poly-bezier");

end:
    ngli_path_freep(&path);
    return ret;
}

static int test_composition(void)
{
    static const float points[][3] = {
        {-0.6,  0.2, 0.0},
        {-0.2,  0.7, 0.0},
        { 0.5,  0.3, 0.0},
        {-0.3,  0.1, 0.0},
        { 0.1, -0.2, 0.0},
        {-0.2, -0.4, 0.0},
        { 0.3, -0.6, 0.0},
        { 0.7, -0.2, 0.0},
        {-0.4, -0.1, 0.0},
        {-0.8, -0.1, 0.0},
        {-0.6,  0.2, 0.0},
    };

    static const float controls[][3] = {
        { 0.4,  0.8, 0.0},
        { 0.0,  0.1, 0.0},
        {-0.1,  0.2, 0.0},
        { 0.6, -0.6, 0.0},
        {-0.6, -0.2, 0.0},
        {-0.8,  0.4, 0.0},
        {-1.2,  0.5, 0.0},
    };

    static const float refs[] = {
        -0.812889,   -0.066111, 0.0,
             -0.6,         0.2, 0.0,
        -0.387111,    0.466111, 0.0,
        -0.158955,     0.70579, 0.0,
         0.139265,    0.592457, 0.0,
          0.27384,    0.284733, 0.0,
        -0.197802,     0.11846, 0.0,
        0.0472765,    -0.10324, 0.0,
       -0.0918672,   -0.327911, 0.0,
      -0.00425009,     -0.4783, 0.0,
         0.313111,   -0.599806, 0.0,
         0.599048,    -0.44062, 0.0,
        -0.476377,   -0.107769, 0.0,
        -0.741631,    0.100257, 0.0,
         -0.86002, -0.00575656, 0.0,
        -0.921486,    0.295537, 0.0,
             -0.6,         0.2, 0.0,
        -0.224002,  -0.0146044, 0.0,
    };

    struct path *path = ngli_path_create();
    if (!path)
        return -1;

    int ret;
    if ((ret = ngli_path_move_to(path, points[0])) < 0 ||
        (ret = ngli_path_line_to(path, points[1])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[0], controls[1], points[2])) < 0 ||
        (ret = ngli_path_move_to(path, points[3])) < 0 ||
        (ret = ngli_path_bezier2_to(path, controls[2], points[4])) < 0 ||
        (ret = ngli_path_line_to(path, points[5])) < 0 ||
        (ret = ngli_path_line_to(path, points[6])) < 0 ||
        (ret = ngli_path_bezier2_to(path, controls[3], points[7])) < 0 ||
        (ret = ngli_path_move_to(path, points[8])) < 0 ||
        (ret = ngli_path_bezier3_to(path, controls[4], controls[5], points[9])) < 0 ||
        (ret = ngli_path_bezier2_to(path, controls[6], points[10])) < 0)
        goto end;

    ret = ngli_path_init(path, 64);
    if (ret < 0)
        goto end;

    ret = evaluate_points(path, refs, "lines/bezier2/bezier3 with discontinuities");

end:
    ngli_path_freep(&path);
    return ret;
}

int main(int ac, char **av)
{
    int ret;

    if ((ret = test_bezier3_vec3()) < 0 ||
        (ret = test_poly_bezier3()) < 0 ||
        (ret = test_composition()) < 0)
        return 1;
    return 0;
}
