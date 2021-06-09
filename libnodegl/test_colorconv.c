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

#include <stdio.h>
#include <math.h>

#include "image.h"
#include "colorconv.h"

static const struct {
    int val;
    const char *name;
} ranges[] = {
    {SXPLAYER_COL_RNG_LIMITED, "limited range"},
    {SXPLAYER_COL_RNG_FULL,    "full range"},
};

static const struct {
    int val;
    const char *name;
} spaces[] = {
    {SXPLAYER_COL_SPC_BT470BG,    "bt601"},
    {SXPLAYER_COL_SPC_BT709,      "bt709"},
    {SXPLAYER_COL_SPC_BT2020_NCL, "bt2020"},
};

static const float expected_colormatrices[2][3][4 * 4]= {
    [0] = {
        [0] = {
                       85/73.,                85/73.,            85/73., 0.,
                           0.,     -1287801/3287200.,      22593/11200., 0.,
                 35751/22400.,   -10689549/13148800.,                0., 0.,
              -167519/191625.,   59804057/112483875.,   -208034/191625., 1.,
        },
        [1] = {
                       85/73.,                85/73.,            85/73., 0.,
                           0.,  -28469543/133504000.,    236589/112000., 0.,
               200787/112000.,  -71145527/133504000.,                0., 0.,
              -932203/958125.,   34431883/114208500.,  -1085941/958125., 1.,
        },
        [2] = {
                       85/73.,                85/73.,            85/73., 0.,
                           0.,  -94831967/506240000.,    479757/224000., 0.,
               376023/224000., -329270807/506240000.,                0., 0.,
            -1754687/1916250.,  250791201/721787500., -2200133/1916250., 1.,
        },
    },
    [1] = {
        [0] = {
                           1.,                    1.,                1., 0.,
                           0.,         -25251/73375.,          443/250., 0.,
                     701/500.,       -209599/293500.,                0., 0.,
                -22432/31875.,     9939296/18710625.,     -28352/31875., 1.,
        },
        [1] = {
                           1.,                    1.,                1., 0.,
                           0.,     -1674679/8940000.,        4639/2500., 0.,
                   3937/2500.,     -4185031/8940000.,                0., 0.,
              -125984/159375.,     4687768/14248125.,   -148448/159375., 1.,
        },
        [2] = {
                           1.,                    1.,                1., 0.,
                           0.,    -5578351/33900000.,        9407/5000., 0.,
                   7373/5000.,   -19368871/33900000.,                0., 0.,
              -117968/159375.,   99788888/270140625.,   -150512/159375., 1.,
        },
    },
};

static int compare_matrices(const float *a, const float *b)
{
    int fail = 0;
    float diff[4 * 4];
    for (int i = 0; i < NGLI_ARRAY_NB(diff); i++) {
        diff[i] = fabs(a[i] - b[i]);
        fail += diff[i] > 1e-6;
    }
    printf("diff:\n" NGLI_FMT_MAT4 "\n\n", NGLI_ARG_MAT4(diff));
    return fail ? -fail : 0;
}

int main(void)
{
    int fail = 0;
    float mat[4 * 4];
    struct color_info cinfo = NGLI_COLOR_INFO_DEFAULTS;

    for (int r = 0; r < NGLI_ARRAY_NB(ranges); r++) {
        cinfo.range = ranges[r].val;
        for (int s = 0; s < NGLI_ARRAY_NB(spaces); s++) {
            cinfo.space = spaces[s].val;
            if (ngli_colorconv_get_ycbcr_to_rgb_color_matrix(mat, &cinfo, 1.f) < 0)
                return 1;
            printf("%s %s:\n" NGLI_FMT_MAT4 "\n\n", spaces[s].name, ranges[r].name, NGLI_ARG_MAT4(mat));
            if (compare_matrices(mat, expected_colormatrices[r][s]) < 0) {
                printf(">>>> DIFF IS TOO HIGH <<<<\n\n");
                fail++;
            }
        }
    }
    return fail;
}
