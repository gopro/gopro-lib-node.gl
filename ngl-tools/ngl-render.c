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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <nodegl.h>

#include "common.h"
#include "opts.h"
#include "wsi.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

static struct ngl_node *get_scene(const char *filename)
{
    char *buf = get_text_file_content(filename);
    if (!buf)
        return NULL;
    struct ngl_node *scene = ngl_node_deserialize(buf);
    free(buf);
    return scene;
}

struct range {
    float start;
    float duration;
    int freq;
};

struct ctx {
    /* options */
    int log_level;
    struct ngl_config cfg;
    int debug;
    const char *input;
    const char *output;
    struct range *ranges;
    int nb_ranges;
    int aspect[2];
};

static int opt_timerange(const char *arg, void *dst)
{
    struct range r;
    if (sscanf(arg, "%f:%f:%d", &r.start, &r.duration, &r.freq) != 3) {
        fprintf(stderr, "Invalid range format: \"%s\" "
                "is not following \"start:duration:freq\"\n", arg);
        return EXIT_FAILURE;
    }

    uint8_t *cur_ranges_p = dst;
    uint8_t *nb_cur_ranges_p = cur_ranges_p + sizeof(struct range *);
    struct range *cur_ranges = *(struct range **)cur_ranges_p;
    const int nb_cur_ranges = *(int *)nb_cur_ranges_p;
    const int nb_new_ranges = nb_cur_ranges + 1;
    struct range *new_ranges = realloc(cur_ranges, nb_new_ranges * sizeof(*new_ranges));
    if (!new_ranges)
        return NGL_ERROR_MEMORY;
    new_ranges[nb_cur_ranges] = r;
    memcpy(dst, &new_ranges, sizeof(new_ranges));
    *(int *)nb_cur_ranges_p = nb_new_ranges;
    return 0;
}

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-d", "--debug",         OPT_TYPE_TOGGLE,   .offset=OFFSET(debug)},
    {"-w", "--show_window",   OPT_TYPE_TOGGLE,   .offset=OFFSET(cfg.offscreen)},
    {"-i", "--input",         OPT_TYPE_STR,      .offset=OFFSET(input)},
    {"-o", "--output",        OPT_TYPE_STR,      .offset=OFFSET(output)},
    {"-t", "--timerange",     OPT_TYPE_CUSTOM,   .offset=OFFSET(ranges), .func=opt_timerange},
    {"-l", "--loglevel",      OPT_TYPE_LOGLEVEL, .offset=OFFSET(log_level)},
    {"-b", "--backend",       OPT_TYPE_BACKEND,  .offset=OFFSET(cfg.backend)},
    {"-s", "--size",          OPT_TYPE_RATIONAL, .offset=OFFSET(cfg.width)},
    {"-a", "--aspect",        OPT_TYPE_RATIONAL, .offset=OFFSET(aspect)},
    {"-z", "--swap_interval", OPT_TYPE_INT,      .offset=OFFSET(cfg.swap_interval)},
    {"-c", "--clear_color",   OPT_TYPE_COLOR,    .offset=OFFSET(cfg.clear_color)},
    {"-m", "--samples",       OPT_TYPE_INT,      .offset=OFFSET(cfg.samples)},
};

int main(int argc, char *argv[])
{
    struct ctx s = {
        .log_level          = NGL_LOG_INFO,
        .input              = NULL,
        .output             = NULL,
        .cfg.width          = DEFAULT_WIDTH,
        .cfg.height         = DEFAULT_HEIGHT,
        .cfg.offscreen      = 1,
        .cfg.swap_interval  = -1,
        .cfg.clear_color[3] = 1.f,
        .aspect[0]          = 1,
        .aspect[1]          = 1,
    };

    SDL_Window *window = NULL;

    int ret = opts_parse(argc, argc, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), NULL);
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    ngl_log_set_min_level(s.log_level);

    if (!s.nb_ranges) {
        fprintf(stderr, "At least one range needs to be specified (-t start:duration:freq)\n");
        return EXIT_FAILURE;
    }

    printf("%s -> %s %dx%d\n", s.input ? s.input : "<stdin>", s.output ? s.output : "-", s.cfg.width, s.cfg.height);

    if (!s.cfg.offscreen) {
        if (init_window() < 0)
            return EXIT_FAILURE;

        window = get_window("ngl-render", s.cfg.width, s.cfg.height);
        if (!window) {
            SDL_Quit();
            return EXIT_FAILURE;
        }
    }

    int fd = -1;
    struct ngl_ctx *ctx = NULL;
    uint8_t *capture_buffer = NULL;

    struct ngl_node *scene = get_scene(s.input);
    if (!scene) {
        ret = EXIT_FAILURE;
        goto end;
    }

    if (s.output) {
        const int stdout_output = !strcmp(s.output, "-");
        if (stdout_output) {
            fd = dup(STDOUT_FILENO);
            if (fd < 0 || dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
                ret = EXIT_FAILURE;
                goto end;
            }
        } else {
            fd = open(s.output, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
            if (fd == -1) {
                fprintf(stderr, "Unable to open %s\n", s.output);
                ret = EXIT_FAILURE;
                goto end;
            }
        }
        capture_buffer = calloc(s.cfg.width * s.cfg.height, 4);
        if (!capture_buffer)
            goto end;
    }

    ctx = ngl_create();
    if (!ctx) {
        ngl_node_unrefp(&scene);
        goto end;
    }

    get_viewport(s.cfg.width, s.cfg.height, s.aspect, s.cfg.viewport);
    s.cfg.capture_buffer = capture_buffer;

    if (!s.cfg.offscreen) {
        ret = wsi_set_ngl_config(&s.cfg, window);
        if (ret < 0) {
            ngl_node_unrefp(&scene);
            return ret;
        }
    }

    ret = ngl_configure(ctx, &s.cfg);
    if (ret < 0) {
        ngl_node_unrefp(&scene);
        goto end;
    }

    ret = ngl_set_scene(ctx, scene);
    ngl_node_unrefp(&scene);
    if (ret < 0)
        goto end;

    for (int i = 0; i < s.nb_ranges; i++) {
        int k = 0;
        const struct range *r = &s.ranges[i];
        const float t0 = r->start;
        const float t1 = r->start + r->duration;

        const int64_t start = gettime();

        for (;;) {
            const float t = t0 + k*1./r->freq;
            if (t >= t1)
                break;
            if (s.debug)
                printf("draw @ t=%f [range %d/%d: %g-%g @ %dHz]\n",
                       t, i + 1, s.nb_ranges, t0, t1, r->freq);
            ret = ngl_draw(ctx, t);
            if (ret < 0) {
                fprintf(stderr, "Unable to draw @ t=%g\n", t);
                goto end;
            }
            if (capture_buffer)
                write(fd, capture_buffer, 4 * s.cfg.width * s.cfg.height);
            if (!s.cfg.offscreen) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                }
            }

            k++;
        }

        const double tdiff = (gettime() - start) / 1000000.;
        printf("Rendered %d frames in %g (FPS=%g)\n", k, tdiff, k / tdiff);
    }

end:
    ngl_freep(&ctx);

    if (fd != -1)
        close(fd);

    free(capture_buffer);
    free(s.ranges);

    if (!s.cfg.offscreen) {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    return ret;
}
