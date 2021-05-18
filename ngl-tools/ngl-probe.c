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

#include <nodegl.h>

#include "common.h"
#include "opts.h"

struct ctx {
    /* options */
    int log_level;
    struct ngl_config cfg;
    const char *cap;
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-l", "--loglevel",  OPT_TYPE_LOGLEVEL, .offset=OFFSET(log_level)},
    {"-b", "--backend",   OPT_TYPE_BACKEND,  .offset=OFFSET(cfg.backend)},
    {"-c", "--cap",       OPT_TYPE_STR,      .offset=OFFSET(cap)},
};

static const struct ngl_cap *get_cap(const struct ngl_backend *backend, const char *key)
{
    char *endptr;
    long int id = strtol(key, &endptr, 0);
    const int use_id = !*endptr; // user input is an int, so we will use that as id

    for (int i = 0; i < backend->nb_caps; i++) {
        const struct ngl_cap *cap = &backend->caps[i];
        if (use_id && cap->id == id)
            return cap;
        if (!use_id && !strcmp(cap->string_id, key))
            return cap;
    }
    return NULL;
}

static const struct ngl_backend *select_default_backend(const struct ngl_backend *backends, int nb_backends)
{
    for (int i = 0; i < nb_backends; i++) {
        const struct ngl_backend *backend = &backends[i];
        if (backend->is_default)
            return backend;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    struct ctx s = {
        .log_level     = NGL_LOG_WARNING,
        .cfg.backend   = NGL_BACKEND_AUTO,
        .cfg.width     = 1,
        .cfg.height    = 1,
        .cfg.offscreen = 1,
    };

    int ret = opts_parse(argc, argc, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), NULL);
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    ngl_log_set_min_level(s.log_level);

    const int specified_backend = s.cfg.backend != NGL_BACKEND_AUTO;
    const struct ngl_config *config = specified_backend ? &s.cfg : NULL;

    struct ngl_backend *backends;
    int nb_backends;
    ret = ngl_backends_probe(config, &nb_backends, &backends);
    if (ret < 0)
        return EXIT_FAILURE;

    if (s.cap) {
        if (!nb_backends) {
            fprintf(stderr, "no backend to query\n");
            ret = EXIT_FAILURE;
            goto end;
        }

        const struct ngl_backend *backend = &backends[0];
        if (!specified_backend) {
            backend = select_default_backend(backends, nb_backends);
            if (!backend) {
                fprintf(stderr, "unable to get the default backend\n");
                ret = EXIT_FAILURE;
                goto end;
            }
        }

        const struct ngl_cap *cap = get_cap(backend, s.cap);
        if (!cap) {
            fprintf(stderr, "cap %s not found\n", s.cap);
            ret = EXIT_FAILURE;
            goto end;
        }
        printf("%d\n", cap->value);
    } else {
        for (int i = 0; i < nb_backends; i++) {
            const struct ngl_backend *backend = &backends[i];
            printf("- %s:\n", backend->string_id);
            printf("    name: %s\n", backend->name);
            printf("    is_default: %s\n", backend->is_default ? "yes" : "no");
            printf("    caps:\n");
            for (int j = 0; j < backend->nb_caps; j++) {
                const struct ngl_cap *cap = &backends->caps[j];
                printf("      %s: %u\n", cap->string_id, cap->value);
            }
        }
    }

end:
    ngl_backends_freep(&backends);
    return ret;
}
