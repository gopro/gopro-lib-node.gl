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

#if defined(_WIN32) && defined(_MSC_VER)
#define STDOUT_FILENO _fileno(stdout)
#define STDERR_FILENO _fileno(stderr)
#else
#define _POSIX_C_SOURCE 200809L // fdopen
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nodegl.h>

#include "python_utils.h"

static FILE *open_ofile(const char *output)
{
    if (!strcmp(output, "-")) {
        const int fd = dup(STDOUT_FILENO);
        if (fd < 0)
            return NULL;
        if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
            close(fd);
            return NULL;
        }
        return fdopen(fd, "wb");
    }
    return fopen(output, "wb");
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <module> <scene_func> <output.ngl>\n", argv[0]);
        return 0;
    }

    FILE *of = open_ofile(argv[3]);
    if (!of)
        return EXIT_FAILURE;

    struct ngl_node *scene = python_get_scene(argv[1], argv[2], NULL, NULL);
    if (!scene) {
        ret = EXIT_FAILURE;
        goto end;
    }

    char *serialized_scene = ngl_node_serialize(scene);
    ngl_node_unrefp(&scene);
    if (!serialized_scene) {
        ret = EXIT_FAILURE;
        goto end;
    }

    const size_t slen = strlen(serialized_scene);
    const size_t n = fwrite(serialized_scene, 1, slen, of);
    if (n != slen) {
        ret = EXIT_FAILURE;
        goto end;
    }

end:
    if (of)
        fclose(of);

    return ret;
}
