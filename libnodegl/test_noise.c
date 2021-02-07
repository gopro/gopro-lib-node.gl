/*
 * Copyright 2021 GoPro Inc.
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
#include <stdlib.h>
#include <math.h>

#include "noise.h"
#include "utils.h"

static const struct noise_test {
    struct noise_params p;
    float expected_values[30]; // 3s at 10Hz
} noise_tests[] = {
    {
        /* Simple signal with linear interpolation */
        .p = {
            .amplitude  = 1.,
            .octaves    = 1,
            .lacunarity = 2.,
            .gain       = .5,
            .seed       = 0x1234567,
            .function   = NGLI_NOISE_LINEAR,
        },
        .expected_values = {
            0.000000,-0.126938,-0.225668,-0.296189,-0.338502,-0.352606,-0.338502,-0.296189,-0.225668,-0.126938,
            0.000000, 0.020963, 0.037267, 0.048913, 0.055901, 0.058230, 0.055901, 0.048913, 0.037267, 0.020963,
            0.000000, 0.067128, 0.119338, 0.156632, 0.179008, 0.186466, 0.179008, 0.156632, 0.119338, 0.067128,
        },
    }, {
        /*
         * Check cyclicity: even with multiple octaves, if the lacunarity is
         * round, we will observe the cycle through 0 at every second with the
         * gradient noise (first column).
         */
        .p = {
            .amplitude  = 1.2,
            .octaves    = 8,
            .lacunarity = 2.,
            .gain       = .5,
            .seed       = 0xc474fe39,
            .function   = NGLI_NOISE_CUBIC,
        },
        .expected_values = {
            0.000000,-0.043850,-0.030711,-0.032577, 0.014327,-0.008308, 0.006247, 0.023553, 0.081410,-0.002214,
            0.000000,-0.046281,-0.008353, 0.006055,-0.001224, 0.025979, 0.104520, 0.056266,-0.048626, 0.021999,
            0.000000,-0.042351,-0.045388, 0.039898, 0.029366, 0.109615, 0.037113, 0.015866,-0.016738, 0.048401,
        },
    }, {
        /* Lacunarity and gain slightly offset */
        .p = {
            .amplitude  = 1.,
            .octaves    = 4,
            .lacunarity = 1.98,
            .gain       = .56,
            .seed       = 0,
            .function   = NGLI_NOISE_QUINTIC,
        },
        .expected_values = {
            0.000000,-0.276658,-0.356364,-0.290270,-0.319912,-0.226213,-0.071865,-0.144200,-0.138108,-0.150140,
           -0.054478, 0.166980, 0.080542, 0.006882,-0.130795,-0.248364,-0.203026,-0.315853,-0.274210,-0.239689,
            0.018803, 0.170394, 0.346100, 0.345984, 0.245240, 0.206418, 0.207302, 0.204406, 0.008591,-0.074062,
        },
    },
};

static int run_test(void)
{
    int ret = 0;

    for (int k = 0; k < NGLI_ARRAY_NB(noise_tests); k++) {
        const struct noise_test *test = &noise_tests[k];
        const struct noise_params *np = &test->p;

        printf("testing amp:%g oct:%d lac:%g gain:%g seed:0x%08x fn:%d\n",
               np->amplitude, np->octaves, np->lacunarity, np->gain, np->seed, np->function);

        struct noise noise;
        if (ngli_noise_init(&noise, &test->p) < 0)
            return EXIT_FAILURE;

        const int nb_values = NGLI_ARRAY_NB(test->expected_values);
        for (int i = 0; i < nb_values; i++) {
            const float t = i / 10.f;
            const float gv = ngli_noise_get(&noise, t);
            const float ev = test->expected_values[i];
            if (fabs(gv - ev) > 0.0001) {
                fprintf(stderr, "noise(%f)=%g but expected %g [err:%g]\n", t, gv, ev, fabs(gv - ev));
                ret = EXIT_FAILURE;
            }
        }
    }

    return ret;
}

static const struct noise_params default_params = {
    .amplitude  = 1.0,
    .octaves    = 8,
    .lacunarity = 2.,
    .gain       = .5,
    .seed       = 0x70a21519,
    .function   = NGLI_NOISE_CUBIC,
};

int main(int ac, char **av)
{
    if (ac == 1)
        return run_test();

    const float duration = ac > 1 ? atof(av[1]) : 3.f;
    const float frequency = ac > 2 ? atof(av[2]) : 10.f;
    const struct noise_params np = {
        .amplitude  = ac > 3 ? atof(av[3]) : default_params.amplitude,
        .octaves    = ac > 4 ? atoi(av[4]) : default_params.octaves,
        .lacunarity = ac > 5 ? atof(av[5]) : default_params.lacunarity,
        .gain       = ac > 6 ? atof(av[6]) : default_params.gain,
        .seed       = ac > 7 ? strtol(av[7], NULL, 0) : default_params.seed,
        .function   = ac > 8 ? atoi(av[8]) : default_params.function,
    };

    /* data can be piped to Gnuplot, for example:
     *   test_noise 15 100 1 1 2 .5 0x50726e67 2 | gnuplot -p -e "p '-' using 1:2 with lines" */
    printf("# duration:%g frq:%g amp:%g oct:%d lac:%g gain:%g seed:0x%08x fn:%d\n",
           duration, frequency, np.amplitude, np.octaves, np.lacunarity, np.gain, np.seed, np.function);

    struct noise noise;
    if (ngli_noise_init(&noise, &np) < 0)
        return EXIT_FAILURE;

    const int nb_values = (int)(duration * frequency);
    for (int i = 0; i < nb_values; i++) {
        const float t = i / frequency;
        printf("%f %f\n", t, ngli_noise_get(&noise, t));
    }

    return 0;
}
