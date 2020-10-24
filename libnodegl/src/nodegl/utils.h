/*
 * Copyright 2016 GoPro Inc.
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

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
# define ngli_printf_format(fmtpos, attrpos) __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
# define ngli_printf_format(fmtpos, attrpos)
#endif

#define ngli_assert(cond) do {                          \
    if (!(cond)) {                                      \
        fprintf(stderr, "Assert %s @ %s:%d\n",          \
                #cond, __FILE__, __LINE__);             \
        abort();                                        \
    }                                                   \
} while (0)

#define NGLI_STATIC_ASSERT(id, c) typedef char ngli_checking_##id[(c) ? 1 : -1]

#define NGLI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define NGLI_MAX(a, b) ((a) > (b) ? (a) : (b))

#define NGLI_ARRAY_NB(x) ((int)(sizeof(x)/sizeof(*(x))))
#define NGLI_SWAP(type, a, b) do { type tmp_swap = b; b = a; a = tmp_swap; } while (0)

#define NGLI_ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))
#define NGLI_ALIGN_VAL 16

#ifdef WIN32
#define NGLI_ALIGNED_VEC(vname) float vname[4]
#else
#define NGLI_ALIGNED_VEC(vname) float __attribute__ ((aligned (NGLI_ALIGN_VAL))) vname[4]
#endif

#ifdef WIN32
#define NGLI_ALIGNED_MAT(mname) float mname[4*4]
#else 
#define NGLI_ALIGNED_MAT(mname) float __attribute__ ((aligned (NGLI_ALIGN_VAL))) mname[4*4]
#endif

#ifdef CONFIG_SMALL
#define NGLI_DOCSTRING(s) (NULL)
#else
#define NGLI_DOCSTRING(s) (s)
#endif

/* Format printf helpers */
#define NGLI_FMT_F    "%12g"
#define NGLI_FMT_VEC2 NGLI_FMT_F " " NGLI_FMT_F
#define NGLI_FMT_VEC3 NGLI_FMT_F " " NGLI_FMT_VEC2
#define NGLI_FMT_VEC4 NGLI_FMT_F " " NGLI_FMT_VEC3

#define NGLI_FMT_MAT2 NGLI_FMT_VEC2 "\n" \
                      NGLI_FMT_VEC2
#define NGLI_FMT_MAT3 NGLI_FMT_VEC3 "\n" \
                      NGLI_FMT_VEC3 "\n" \
                      NGLI_FMT_VEC3
#define NGLI_FMT_MAT4 NGLI_FMT_VEC4 "\n" \
                      NGLI_FMT_VEC4 "\n" \
                      NGLI_FMT_VEC4 "\n" \
                      NGLI_FMT_VEC4

/* Argument helpers associated with formats declared above */
#define NGLI_ARG_F(v)    *(v)
#define NGLI_ARG_VEC2(v) NGLI_ARG_F(v), NGLI_ARG_F((v)+1)
#define NGLI_ARG_VEC3(v) NGLI_ARG_F(v), NGLI_ARG_VEC2((v)+1)
#define NGLI_ARG_VEC4(v) NGLI_ARG_F(v), NGLI_ARG_VEC3((v)+1)

#define NGLI_ARG_MAT2(v) NGLI_ARG_VEC2((v)+2*0),    \
                         NGLI_ARG_VEC2((v)+2*1)
#define NGLI_ARG_MAT3(v) NGLI_ARG_VEC3((v)+3*0),    \
                         NGLI_ARG_VEC3((v)+3*1),    \
                         NGLI_ARG_VEC3((v)+3*2)
#define NGLI_ARG_MAT4(v) NGLI_ARG_VEC4((v)+4*0),    \
                         NGLI_ARG_VEC4((v)+4*1),    \
                         NGLI_ARG_VEC4((v)+4*2),    \
                         NGLI_ARG_VEC4((v)+4*3)


char *ngli_strdup(const char *s);
int64_t ngli_gettime_relative(void);
char *ngli_asprintf(const char *fmt, ...) ngli_printf_format(1, 2);
uint32_t ngli_crc32(const char *s);
void ngli_thread_set_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
