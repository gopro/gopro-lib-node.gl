/*
 * Copyright 2020 GoPro Inc.
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
#include <sys/time.h>

#include <nodegl.h>

#include "common.h"
#include "opts.h"

struct s2i {
    const char *s;
    int v;
};

static int s2i(const struct s2i *map, int n, const char *s)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(map[i].s, s))
            return map[i].v;
    return NGL_ERROR_NOT_FOUND;
}

static int opt_toggle(const char *arg, void *dst)
{
    const int v = !*(int *)dst;
    memcpy(dst, &v, sizeof(v));
    return 0;
}

static int opt_int(const char *arg, void *dst)
{
    const int v = atoi(arg);
    memcpy(dst, &v, sizeof(v));
    return 0;
}

static int opt_str(const char *arg, void *dst)
{
    memcpy(dst, &arg, sizeof(arg)); // copy pointer
    return 0;
}

static int opt_time(const char *arg, void *dst)
{
    const double v = strtod(arg, NULL); // XXX: beware of locales bullshit
    memcpy(dst, &v, sizeof(v));
    return 0;
}

static int opt_loglevel(const char *arg, void *dst)
{
    static const struct s2i loglevel_map[] = {
        {"debug",   NGL_LOG_DEBUG},
        {"verbose", NGL_LOG_VERBOSE},
        {"info",    NGL_LOG_INFO},
        {"warning", NGL_LOG_WARNING},
        {"error",   NGL_LOG_ERROR},
    };
    const int lvl = s2i(loglevel_map, ARRAY_NB(loglevel_map), arg);
    if (lvl < 0) {
        fprintf(stderr, "invalid log level \"%s\"\n", arg);
        return lvl;
    }
    memcpy(dst, &lvl, sizeof(lvl));
    return 0;
}

static int opt_backend(const char *arg, void *dst)
{
    static const struct s2i backend_map[] = {
        {"auto",     NGL_BACKEND_AUTO},
        {"opengl",   NGL_BACKEND_OPENGL},
        {"opengles", NGL_BACKEND_OPENGLES},
        {"vulkan",   NGL_BACKEND_VULKAN},
    };
    const int backend = s2i(backend_map, ARRAY_NB(backend_map), arg);
    if (backend < 0) {
        fprintf(stderr, "invalid backend \"%s\"\n", arg);
        return backend;
    }
    memcpy(dst, &backend, sizeof(backend));
    return 0;
}

static int opt_rational(const char *arg, void *dst)
{
    int r[2];
    int ret = strchr(arg, '/') ? sscanf(arg, "%d/%d", r, r + 1)
                               : sscanf(arg, "%dx%d", r, r + 1);
    if (ret == 1) {
        r[1] = 1;
    } else if (ret != 2) {
        fprintf(stderr, "invalid format for \"%s\", expecting \"A/B\" or \"AxB\"\n", arg);
        return NGL_ERROR_INVALID_ARG;
    }
    memcpy(dst, r, sizeof(r));
    return 0;
}

static int opt_color(const char *arg, void *dst)
{
    int r, g, b, a;
    if (sscanf(arg, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4) {
        fprintf(stderr, "invalid color format for \"%s\", expecting \"RRGGBBAA\"\n", arg);
        return NGL_ERROR_INVALID_ARG;
    }
    const float c[4] = { r / 255., g / 255., b / 255., a / 255. };
    memcpy(dst, c, sizeof(c));
    return 0;
}

static const struct opt *find_opt(const char *arg, const struct opt *opts, int nb_opts)
{
    for (int i = 0; i < nb_opts; i++)
        if ((opts[i].short_name && !strcmp(arg, opts[i].short_name)) ||
            (opts[i].name       && !strcmp(arg, opts[i].name)))
            return &opts[i];
    return NULL;
}

typedef int (*func_type)(const char *arg, void *dst);

int opts_parse(int ac, int ac_max, char **av, const struct opt *opts, int nb_opts, void *dst)
{
    static func_type func_maps[OPT_TYPE_NB] = {
        [OPT_TYPE_TOGGLE]   = opt_toggle,
        [OPT_TYPE_INT]      = opt_int,
        [OPT_TYPE_STR]      = opt_str,
        [OPT_TYPE_TIME]     = opt_time,
        [OPT_TYPE_LOGLEVEL] = opt_loglevel,
        [OPT_TYPE_BACKEND]  = opt_backend,
        [OPT_TYPE_RATIONAL] = opt_rational,
        [OPT_TYPE_COLOR]    = opt_color,
    };

    if (ac > 1 && (!strcmp(av[1], "-h") || !strcmp(av[1], "--help")))
        return OPT_HELP;
    for (int i = 1; i < ac_max; i++) {
        const struct opt *o = find_opt(av[i], opts, nb_opts);
        if (!o) {
            fprintf(stderr, "unrecognized option \"%s\"\n", av[i]);
            return NGL_ERROR_INVALID_ARG;
        }
        const char *arg = i == ac - 1 ? NULL : av[i + 1];
        if (o->type != OPT_TYPE_TOGGLE) {
            if (!arg)
                return NGL_ERROR_INVALID_ARG;
            i++;
        }
        func_type func = o->func ? o->func : func_maps[o->type];
        int ret = func(arg, (uint8_t *)dst + o->offset);
        if (ret < 0)
            return ret;
    }
    return ac;
}

void opts_print_usage(const char *program, const struct opt *opts, int nb_opts, const char *usage_extra)
{
    static const char *types_map[OPT_TYPE_NB] = {
        [OPT_TYPE_INT]      = "integer",
        [OPT_TYPE_STR]      = "string",
        [OPT_TYPE_TIME]     = "time",
        [OPT_TYPE_LOGLEVEL] = "log_level",
        [OPT_TYPE_BACKEND]  = "backend",
        [OPT_TYPE_RATIONAL] = "rational",
        [OPT_TYPE_COLOR]    = "color",
    };
    fprintf(stderr, "Usage: %s [options]%s\n\n"
                    "Options:\n"
                    "    -h/--help: show this help\n",
            program, usage_extra ? usage_extra : "");
    for (int i = 0; i < nb_opts; i++) {
        const struct opt *o = &opts[i];
        const char *type = types_map[o->type];
        fprintf(stderr, "    %s%s%s",
                o->short_name ? o->short_name : "",
                o->short_name && o->name ? "/" : "",
                o->name ? o->name : "");
        if (type)
            fprintf(stderr, " <%s>", type);
        fprintf(stderr, "\n");
    }
}
