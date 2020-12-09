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

#define _POSIX_C_SOURCE 199309L // clock_gettime()

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#define POW10_9 1000000000
#else
#include <sys/time.h>
#endif

#include "common.h"

int64_t gettime(void)
{
#ifdef _WIN32
    FILETIME t0;
    GetSystemTimeAsFileTime(&t0);
    ULARGE_INTEGER t1;
    t1.LowPart = t0.dwLowDateTime;
    t1.HighPart = t0.dwHighDateTime;
    int64_t t2 = t1.QuadPart;
    return t2 / 10; // convert to microseconds
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
#endif
}

int64_t gettime_relative(void)
{
#ifdef _WIN32
    // reference: https://github.com/mirror/mingw-w64/blob/master/mingw-w64-libraries/winpthreads/src/clock.c
    LARGE_INTEGER pf, pc;
    QueryPerformanceFrequency(&pf);
    QueryPerformanceCounter(&pc);
    int64_t tv_sec = pc.QuadPart / pf.QuadPart;
    int64_t tv_nsec = (int)(((pc.QuadPart % pf.QuadPart) * POW10_9 + (pf.QuadPart >> 1)) / pf.QuadPart);
    if (tv_nsec >= POW10_9) {
        tv_sec++;
        tv_nsec -= POW10_9;
    }
    return tv_sec * 1000000 + tv_nsec / 1000;
#else
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    return ret != 0 ? 0 : ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
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

    FILE *fp = filename ? fopen(filename, "rb") : stdin;
    if (!fp) {
        fprintf(stderr, "unable to open %s\n", filename);
        goto end;
    }

    size_t pos = 0;
    for (;;) {
        const size_t needed = pos + BUF_SIZE + 1;
        void *new_buf = realloc(buf, needed);
        if (!new_buf) {
            free(buf);
            buf = NULL;
            goto end;
        }
        buf = new_buf;
        const size_t n = fread(buf + pos, 1, BUF_SIZE, fp);
        if (ferror(fp)) {
            free(buf);
            buf = NULL;
            goto end;
        }
        pos += n;
        if (feof(fp)) {
            buf[pos] = 0;
            break;
        }
    }

end:
    if (fp && fp != stdin)
        fclose(fp);
    return buf;
}
