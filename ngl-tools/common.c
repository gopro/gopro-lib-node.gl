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
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

int64_t gettime(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
}

double clipd(double v, double min, double max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

int clipi(int v, int min, int max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

int64_t clipi64(int64_t v, int64_t min, int64_t max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

void get_viewport(int width, int height, const int *aspect_ratio, int *vp)
{
    vp[2] = width;
    vp[3] = width * aspect_ratio[1] / (double)aspect_ratio[0];
    if (vp[3] > height) {
        vp[3] = height;
        vp[2] = height * aspect_ratio[0] / (double)aspect_ratio[1];
    }
    vp[0] = (width  - vp[2]) / 2.0;
    vp[1] = (height - vp[3]) / 2.0;
}

#define BUF_SIZE 1024

char *get_text_file_content(const char *filename)
{
    char *buf = NULL;

    int fd = filename ? open(filename, O_RDONLY) : STDIN_FILENO;
    if (fd == -1) {
        fprintf(stderr, "unable to open %s\n", filename);
        goto end;
    }

    ssize_t pos = 0;
    for (;;) {
        const ssize_t needed = pos + BUF_SIZE + 1;
        void *new_buf = realloc(buf, needed);
        if (!new_buf) {
            free(buf);
            buf = NULL;
            goto end;
        }
        buf = new_buf;
        const ssize_t n = read(fd, buf + pos, BUF_SIZE);
        if (n < 0) {
            free(buf);
            buf = NULL;
            goto end;
        }
        if (n == 0) {
            buf[pos] = 0;
            break;
        }
        pos += n;
    }

end:
    if (fd != -1 && fd != STDIN_FILENO)
        close(fd);
    return buf;
}
