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

#include <string.h>

#include "distmap.h"
#include "log.h"
#include "path.h"
#include "text.h"

struct text_builtin {
    struct distmap *distmap;
    struct path *path;
    int spread;
    int chr_w, chr_h;
};

#define FIRST_CHAR '!'

static const float view_w = 7.f;
static const float view_h = 8.f;
static const char *outlines[] = {
    /* ! */ "M3 1 v4 h1 v-4 z m0 5.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* " */ "M3 1 v2 h1 v-2 z m2 0 v2 h1 v-2 z",
    /* # */ "M2 1.5 v1 h-1 v1 h1 v1 h-1 v1 h1 v1 h1 v-1 h1 v1 h1 v-1 h1 v-1 h-1 v-1 h1 v-1 h-1 v-1 h-1 v1 h-1 v-1 z m1 2 h1 v1 h-1 z",
    /* $ */ "M6 1 h-3 q-2 0 -2 2 v.5 q0 1 1 1 h2.5 q.5 0 .5 .5 0 1 -1 1 h-3 v.5 q0 .5 .5 .5 h2.5 q2 0 2 -2 v-.5 q0 -1 -1 -1 h-2.5 q-.5 0 -.5 -.5 0 -1 1 -1 h3 z M3 0 v8 h1 v-8 z",
    /* % */ "M1 2.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m4 4 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m.5 -4.5 l-4.5 4.5 .5 .5 4.5 -4.5 z",
    /* & */ "M2 1 q-1 0 -1 1 v1 q0 1 1 1 -1 0 -1 1 v1 q0 1 1 1 h2 l1 -1 1 1 v-1 l-.5 -.5 .5 -.5 v-1 l-1 1 -1 -1 q1 0 1 -1 v-1 q0 -1 -1 -1 z m.5 1 h1 q.5 0 .5 .5 0 .5 -.5 .5 h-1 q-.5 0 -.5 -.5 0 -.5 .5 -.5 m0 3 h1.5 l.5 .5 -.5 .5 h-1.5 q-.5 0 -.5 -.5 0 -.5 .5 -.5",
    /* ' */ "M2 1 q0 1 -1 1 v1 q2 0 2 -2 z",
    /* ( */ "M5 1 h-1 q-2 0 -2 2 v2 q0 2 2 2 h1 v-1 h-1 q-1 0 -1 -1 v-2 q0 -1 1 -1 h1 z",
    /* ) */ "M2 1 v1 h1 q1 0 1 1 v2 q0 1 -1 1 h-1 v1 h1 q2 0 2 -2 v-2 q0 -2 -2 -2 z",
    /* * */ "M1 3 v1 h2 v2 h1 v-2 h2 v-1 h-2 v-2 h-1 v2 z m1 -1.5 l-.5 .5 1.5 1.5 -1.5 1.5 .5 .5 1.5 -1.5 1.5 1.5 .5 -.5 -1.5 -1.5 1.5 -1.5 -.5 -.5 -1.5 1.5 z",
    /* + */ "M1 4 v1 h2 v2 h1 v-2 h2 v-1 h-2 v-2 h-1 v2 z",
    /* , */ "M2 6 q0 1 -1 1 v1 q2 0 2 -2 z",
    /* - */ "M2 4 v1 h3 v-1 z",
    /* . */ "M2 6.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* / */ "M6 1.5 l-.5 -.5 -4.5 5.5 .5 .5 z",
    /* 0 */ "M3 1 q-2 0 -2 2 v2 q0 2 2 2 h1 q2 0 2 -2 v-2 q0 -2 -2 -2 z m-1 4.5 v-2.5 q0 -1 1 -1 h1.5 z m3 -3 v2.5 q0 1 -1 1 h-1.5 z",
    /* 1 */ "M3 1 q0 1 -1 1 v1 h1 v4 h1 v-1 v-5 z",
    /* 2 */ "M1 3 h1 q0 -1 1 -1 h1 q1 0 1 1 0 1 -1 1 h-1 q-2 0 -2 2 v1 h4.5 q.5 0 .5 -.5 v-.5 h-4 q0 -1 1 -1 h1 q2 0 2 -2 0 -2 -2 -2 h-1 q-2 0 -2 2",
    /* 3 */ "M1 3 h1 q0 -1 1 -1 h1 q1 0 1 1 0 .5 -.5 .5 h-1.5 v1 h1.5 q.5 0 .5 .5 0 1 -1 1 h-1 q-1 0 -1 -1 h-1 q0 2 2 2 h1 q2 0 2 -2 q0 -1 -1 -1 1 0 1 -1 0 -2 -2 -2 h-1 q-2 0 -2 2",
    /* 4 */ "M4 1 l-3 3 v1 h3 v2 h1 v-2 h1 v-1 h-1 v-3 h-1 m0 1.5 v1.5 h-1.5 z",
    /* 5 */ "M6 1 h-5 v3 h3 q1 0 1 1 0 1 -1 1 h-1 q-1 0 -1 -1 h-1 q0 2 2 2 h1 q2 0 2 -2 q0 -2 -2 -2 h-2 v-1 h4 z",
    /* 6 */ "M6 1 h-3 q-2 0 -2 2 v2 q0 2 2 2 h1 q2 0 2 -2 0 -2 -2 -2 h-2 q0 -1 1 -1 h3 z m-2 3 q1 0 1 1 0 1 -1 1 h-1 q-1 0 -1 -1 0 -1 1 -1 z",
    /* 7 */ "M1 1 v1 h4 l-2 2 v3 h1 v-3 l2 -2 v-1 z",
    /* 8 */ "M2.5 1 q-1 0 -1 1 v1 q0 1 1 1 h-.5 q-1 0 -1 1 v1 q0 1 1 1 h3 q1 0 1 -1 v-1 q0 -1 -1 -1 h-.5 q1 0 1 -1 v-1 q0 -1 -1 -1 z m.5 1 h1 q.5 0 .5 .5 0 .5 -.5 .5 h-1 q-.5 0 -.5 -.5 0 -.5 .5 -.5 m-.5 3 h2 q.5 0 .5 .5 0 .5 -.5 .5 h-2 q-.5 0 -.5 -.5 0 -.5 .5 -.5",
    /* 9 */ "M3 1 q-2 0 -2 2 0 2 2 2 h2 q0 1 -1 1 h-3 v.5 q0 .5 .5 .5 h2.5 q2 0 2 -2 v-2 q0 -2 -2 -2 z m1 1 q1 0 1 1 0 1 -1 1 h-1 q-1 0 -1 -1 0 -1 1 -1 z",
    /* : */ "M2 3.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 3 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* ; */ "M2 3.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 2.5 q0 1 -1 1 v1 q2 0 2 -2 z",
    /* < */ "M4.5 1 l-3 3 3 3 .5 -.5 -2.5 -2.5 2.5 -2.5 -.5 -.5",
    /* = */ "M1 2 h5 v1 h-5 z m0 3 h5 v1 h-5 z",
    /* > */ "M2.5 1 l-.5 .5 2.5 2.5 -2.5 2.5 .5 .5 3 -3 -3 -3",
    /* ? */ "M2 1 v1 h2.5 q.5 0 .5 .5 0 .5 -.5 .5 h-1 q-.5 0 -.5 .5 v1.5 h1 v-1 h1 q1 0 1 -1 v-1 q0 -1 -1 -1 z m1 5.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5",
    /* @ */ "M3 4 q0 1 1 1 h.5 q.5 0 .5 -.5 v-.5 q1 0 1 -1 v-1 q0 -1 -1 -1 h-1 q-3 0 -3 3 0 3 3 3 h1 q2 0 2 -2 h-1 q0 1 -1 1 h-1 q-2 0 -2 -2 0 -2 2 -2 h.5 q.5 0 .5 .5 v.5 h-1 q-1 0 -1 1",
    /* A */ "M1 3 v4 h1 v-2 h3 v2 h1 v-4 q0 -2 -2 -2 h-1 q-2 0 -2 2 m1 0 q0 -1 1 -1 h1 q1 0 1 1 v1 h-3 z",
    /* B */ "M1 1 v6 h4 q1 0 1 -1 v-1 q0 -1 -1 -1 h-.5 q1 0 1 -1 v-1 q0 -1 -1 -1 z m1 1 h2 q.5 0 .5 .5 0 .5 -.5 .5 h-2 z m0 3 h2.5 q.5 0 .5 .5 0 .5 -.5 .5 h-2.5 z",
    /* C */ "M6 1 h-2 q-3 0 -3 3 0 3 3 3 h2 v-1 h-2 q-2 0 -2 -2 0 -2 2 -2 h2 z",
    /* D */ "M1 1 v6 h2 q3 0 3 -3 0 -3 -3 -3 z m1 1 h1 q2 0 2 2 0 2 -2 2 h-1 z",
    /* E */ "M1 1 v6 h5 v-1 h-4 v-1.5 h3 v-1 h-3 v-1.5 h4 v-1 z",
    /* F */ "M1 1 v6 h1 v-2.5 h3 v-1 h-3 v-1.5 h4 v-1 z",
    /* G */ "M6 3 q0 -2 -2 -2 -3 0 -3 3 0 3 3 3 h1.5 q.5 0 .5 -.5 v-2.5 h-2 v1 h1 v1 h-1 q-2 0 -2 -2 0 -2 2 -2 1 0 1 1 z",
    /* H */ "M1 1 v6 h1 v-2.5 h3 v2.5 h1 v-6 h-1 v2.5 h-3 v-2.5 z",
    /* I */ "M2 1 v1 h1 v4 h-1 v1 h3 v-1 h-1 v-4 h1 v-1 z",
    /* J */ "M5 1 v4 q0 1 -1 1 h-1 q-1 0 -1 -1 h-1 q0 2 2 2 h1 q2 0 2 -2 v-4 z",
    /* K */ "M1 1 v6 h1 v-2.5 l3.5 -2.5 -.5 -1 -3 2 v-2 z m2 2.5 l2 3.5 h1 l-2 -4 z",
    /* L */ "M1 1 v6 h5 v-1 h-4 v-5 z",
    /* M */ "M1 1 v6 h1 v-5 l1.5 1 1.5 -1 v5 h1 v-6 h-1 l-1.5 1 -1.5 -1 z",
    /* N */ "M1 1 v6 h1 v-4.5 l2.5 4.5 h1.5 v-6 h-1 v4.5 l-2.5 -4.5 z",
    /* O */ "M3 1 q-2 0 -2 2 v2 q0 2 2 2 h1 q2 0 2 -2 v-2 q0 -2 -2 -2 z m0 1 h1 q1 0 1 1 v2 q0 1 -1 1 h-1 q-1 0 -1 -1 v-2 q0 -1 1 -1",
    /* P */ "M1 1 v6 h1 v-2 h2 q2 0 2 -2 0 -2 -2 -2 z m1 1 h2 q1 0 1 1 0 1 -1 1 h-2 z",
    /* Q */ "M4 1 h-1 q-2 0 -2 2 v1 q0 2 2 2 h1 q0 1 1 1 h1 v-.5 q-1 0 -1 -1 1 0 1 -1 v-1.5 q0 -2 -2 -2 m0 1 q1 0 1 1 v1 q0 1 -1 1 h-1 q-1 0 -1 -1 v-1 q0 -1 1 -1 z",
    /* R */ "M1 1 v6 h1 v-2 h.5 l2 2 h1.5 l-2 -2 q2 0 2 -2 0 -2 -2 -2 z m1 1 h2 q1 0 1 1 0 1 -1 1 h-2 z",
    /* S */ "M6 1 h-3 q-2 0 -2 2 v.5 q0 1 1 1 h2.5 q.5 0 .5 .5 0 1 -1 1 h-3 v.5 q0 .5 .5 .5 h2.5 q2 0 2 -2 v-.5 q0 -1 -1 -1 h-2.5 q-.5 0 -.5 -.5 0 -1 1 -1 h3 z",
    /* T */ "M1 1 v1 h2 v5 h1 v-5 h2 v-1 z",
    /* U */ "M1 1 v4 q0 2 2 2 h1 q2 0 2 -2 v-4 h-1 v4 q0 1 -1 1 h-1 q-1 0 -1 -1 v-4 z",
    /* V */ "M1 1 l2 6 h1 l2 -6 h-1 l-1.5 4 -1.5 -4 z",
    /* W */ "M1 1 l1 6 1.5 -1.5 1.5 1.5 1 -6 h-1 l-.5 4 -1 -1 -1 1 -.5 -4 z",
    /* X */ "M2 1 h-1 l4 6 h1 z m4 0 h-1 l-4 6 h1 z",
    /* Y */ "M4 4 l-2 -3 h-1 l2 3 v3 h1 v-3 l2 -3 h-1 l-2 3",
    /* Z */ "M1 1 v1 h4 l-4 4 v1 h5 v-1 h-4 l4 -4 v-1 z",
    /* [ */ "M5 1 h-3 v6 h3 v-1 h-2 v-4 h2 v-1 z",
    /* \ */ "M1.5 1 l-.5 .5 4.5 5.5 .5 -.5 z",
    /* ] */ "M2 1 v1 h2 v4 h-2 v1 h3 v-6 z",
    /* ^ */ "M3.5 1 l-2.5 2.5 .5 .5 2 -2 2 2 .5 -.5 z",
    /* _ */ "M1 7 v1 h5 v-1 z",
    /* ` */ "M1.5 1 l-.5 .5 1.5 1.5 .5 -.5 z",
    /* a */ "M6 2 h-3 q-2 0 -2 2 v1 q0 2 2 2 h1.5 q1.5 0 1.5 -2 z m-1 1 v2 q0 1 -1 1 h-1 q-1 0 -1 -1 v-1 q0 -1 1 -1 z m0 2 q0 2 2 2 v-1 q-1 0 -1 -1 z",
    /* b */ "M1 0 v6 q0 1 1 1 h2 q2 0 2 -2 v-1 q0 -2 -2 -2 h-2 v-2 z m1.5 3 h1.5 q1 0 1 1 v1 q0 1 -1 1 h-1.5 q-.5 0 -.5 -.5 v-2 q0 -.5 .5 -.5",
    /* c */ "M5 4 h1 v-1 q0 -1 -1 -1 h-2 q-2 0 -2 2 v1 q0 2 2 2 h2.5 q.5 0 .5 -.5 v-.5 h-3 q-1 0 -1 -1 v-1 q0 -1 1 -1 h1.5 q.5 0 .5 .5 z",
    /* d */ "M6 0 h-1 v2 h-2 q-2 0 -2 2 v1 q0 2 2 2 h2 q1 0 1 -1 z m-1 3.5 v2 q0 .5 -.5 .5 h-1.5 q-1 0 -1 -1 v-1 q0 -1 1 -1 h1.5 q.5 0 .5 .5",
    /* e */ "M6 5 v-1 q0 -2 -2 -2 h-1 q-2 0 -2 2 v1 q0 2 2 2 h2.5 q.5 0 .5 -.5 v-.5 h-3 q-1 0 -1 -1 z m-1 -1 h-3 q0 -1 1 -1 h1 q1 0 1 1",
    /* f */ "M6 1 h-3 q-1 0 -1 1 v1 h-1 v1 h1 v3 h1 v-3 h2 v-1 h-2 v-.5 q0 -.5 .5 -.5 h2.5 z",
    /* g */ "M6 2 h-3 q-2 0 -2 2 0 2 2 2 h2 v.5 q0 .5 -.5 .5 h-3.5 v.5 q0 .5 .5 .5 h3.5 q1 0 1 -1 z m-1 1 v2 h-2 q-1 0 -1 -1 0 -1 1 -1 z",
    /* h */ "M1 0 v7 h1 v-2.5 q0 -.5 .5 -.5 h1.5 q1 0 1 1 v2 h1 v-2 q0 -2 -2 -2 h-2 v-3 z",
    /* i */ "M3 1.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 1.5 h-1 v1 h.5 q.5 0 .5 .5 v1.5 q0 1 1 1 h1 v-1 h-.5 q-.5 0 -.5 -.5 v-1.5 q0 -1 -1 -1",
    /* j */ "M4 1.5 q0 .5 .5 .5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 .5 m0 1.5 v3 q0 1 -1 1 -1 0 -1 -1 h-1 q0 2 2 2 2 0 2 -2 v-3 z",
    /* k */ "M1 0 v7 h1 v-2 l2.5 -1 -.5 -1 l-2 1 v-4 z m1 4 l1.5 3 h1 l-1.5 -3 z",
    /* l */ "M2 0 v1 h.5 q.5 0 .5 .5 v4.5 q0 1 1 1 h1 v-1 h-.5 q-.5 0 -.5 -.5 v-4.5 q0 -1 -1 -1 z",
    /* m */ "M1 2 v5 h1 v-4 q1 0 1 1 v3 h1 v-4 q1 0 1 1 v3 h1 v-3 q0 -2 -2 -2 z",
    /* n */ "M1 2 v5 h1 v-2 q0 -2 1 -2 h1 q1 0 1 1 v3 h1 v-3 q0 -2 -2 -2 h-1 q-1 0 -1 1 v-1 z",
    /* o */ "M4 2 h-1 q-2 0 -2 2 v1 q0 2 2 2 h1 q2 0 2 -2 v-1 q0 -2 -2 -2 m0 1 q1 0 1 1 v1 q0 1 -1 1 h-1 q-1 0 -1 -1 v-1 q0 -1 1 -1 z",
    /* p */ "M1 2 v6 h1 v-2 h2 q2 0 2 -2 0 -2 -2 -2 z m1 1 h2 q1 0 1 1 0 1 -1 1 h-2 z",
    /* q */ "M6 2 h-3 q-2 0 -2 2 0 2 2 2 h2 v2 h1 z m-1 1 v2 h-2 q-1 0 -1 -1 0 -1 1 -1 z",
    /* r */ "M1 2 v5 h1 v-3 q0 -1 1 -1 h1 q1 0 1 1 h1 q0 -2 -2 -2 h-1 q-1 0 -1 1 v-1 z",
    /* s */ "M6 2 h-4 q-1 0 -1 1 v1 q0 1 1 1 h2.5 q.5 0 .5 .5 q0 .5 -.5 .5 h-3.5 v.5 q0 .5 .5 .5 h3.5 q1 0 1 -1 v-1 q0 -1 -1 -1 h-2.5 q-.5 0 -.5 -.5 0 -.5 .5 -.5 h3.5 z",
    /* t */ "M2 1 v1 h-1 v1 h1 v2 q0 2 2 2 h.5 q.5 0 .5 -.5 v-.5 h-1 q-1 0 -1 -1 v-2 h2 v-1 h-2 v-1 z",
    /* u */ "M1 2 v3 q0 2 2 2 h1 q1 0 1 -1 v1 h1 v-5 h-1 v2 q0 2 -1 2 h-1 q-1 0 -1 -1 v-3 z",
    /* v */ "M1 2 l2 5 h1 l2 -5 h-1 l-1.5 4 -1.5 -4 z",
    /* w */ "M1 2 l1 5 h1 l.5 -3 .5 3 h1 l1 -5 h-1 l-.5 3 -.5 -2 h-1 l-.5 2 -.5 -3 z",
    /* x */ "M1.5 2 l-.5 .5 2 2 -2 2 .5 .5 2 -2 2 2 .5 -.5 -2 -2 2 -2 -.5 -.5 -2 2 z",
    /* y */ "M1 2 v2 q0 2 2 2 h2 v.5 q0 .5 -.5 .5 h-3.5 v.5 q0 .5 .5 .5 h3.5 q1 0 1 -1 v-5 h-1 v3 h-2 q-1 0 -1 -1 v-2 z",
    /* z */ "M1 2 v1 h3 l-3 4 h5 v-1 h-3 l3 -4 z",
    /* { */ "M5 1 h-1 q-2 0 -2 2 0 .5 -.5 .5 -.5 0 -.5 .5 0 .5 .5 .5 .5 0 .5 .5 0 2 2 2 h1 v-1 h-1 q-1 0 -1 -1 v-2 q0 -1 1 -1 h1 z",
    /* | */ "M3 1 v6 h1 v-6 z",
    /* } */ "M2 1 v1 q1 0 1 1 v2 q0 1 -1 1 v1 q2 0 2 -2 0 -.5 .5 -.5 .5 0 .5 -.5 0 -.5 -.5 -.5 -.5 0 -.5 -.5 0 -2 -2 -2",
    /* ~ */ "M1 4 q0 .5 .5 .5 .5 0 .5 -.5 h1 q0 1 1 1 h1 q1 0 1 -1 0 -.5 -.5 -.5 -.5 0 -.5 .5 h-1 q0 -1 -1 -1 h-1 q-1 0 -1 1",
};

static const char *strip_separators(const char *s)
{
    while (*s && strchr(" ,\t\r\n", *s)) /* comma + whitespaces */
        s++;
    return s;
}

static const char *load_coords(float *dst, const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        s = strip_separators(s);
        dst[i] = strtof(s, (char **)&s);
    }
    return s;
}

static const char *cmd_get_coords(float *dst, const char *s, char lcmd)
{
    if (lcmd == 'm' || lcmd == 'l')
        return load_coords(dst, s, 2); /* x,y */
    if (lcmd == 'v' || lcmd == 'h')
        return load_coords(dst, s, 1); /* x or y */
    if (lcmd == 'q')
        return load_coords(dst, s, 2 * 2); /* x0,y0 x1,y1 */
    if (lcmd == 'c')
        return load_coords(dst, s, 3 * 2); /* x0,y0 x1,y1 x2,y2 */
    return NULL;
}

static int load_outline(struct path *path, const char *s)
{
    float cursor[3] = {0};
    float start[3] = {0};
    const float scale_w = 1.f / view_w;
    const float scale_h = 1.f / view_h;

    char cmd = 0;

    for (;;) {
        s = strip_separators(s);
        if (!*s)
            break;

        // TODO: sStTaA */
        if (strchr("mMvVhHlLqQcCzZ", *s))
            cmd = *s++;
        else if (!cmd)
            return NGL_ERROR_INVALID_DATA;

        const char lcmd = cmd | 0x20; // lower case
        if (lcmd == 'z') { /* closing path */
            ngli_path_line_to(path, start);
            memcpy(cursor, start, sizeof(cursor));
            continue;
        }

        const int relative = cmd == lcmd;
        const float off_x = relative ? cursor[0] : 0.f;
        const float off_y = relative ? cursor[1] : 0.f;

        float coords[3 * 2]; /* maximum number of coordinates (bezier cubic) */
        const char *p = cmd_get_coords(coords, s, lcmd);
        if (!p || p == s) /* bail out in case of error or if the cursor didn't move */
            return NGL_ERROR_INVALID_DATA;
        s = p;

        if (lcmd == 'm') { /* move */
            const float to[3] = {coords[0] * scale_w + off_x, coords[1] * scale_h + off_y, 0.f};
            ngli_path_move_to(path, to);
            memcpy(cursor, to, sizeof(cursor));
            memcpy(start, to, sizeof(start));
        } else if (lcmd == 'l') { /* line */
            const float to[3] = {coords[0] * scale_w + off_x, coords[1] * scale_h + off_y, 0.f};
            ngli_path_line_to(path, to);
            memcpy(cursor, to, sizeof(cursor));
        } else if (lcmd == 'v') { /* vertical line */
            const float to[3] = {off_x, coords[0] * scale_h + off_y, 0.f};
            ngli_path_line_to(path, to);
            memcpy(cursor, to, sizeof(cursor));
        } else if (lcmd == 'h') { /* horizontal line */
            const float to[3] = {coords[0] * scale_w + off_x, off_y, 0.f};
            ngli_path_line_to(path, to);
            memcpy(cursor, to, sizeof(cursor));
        } else if (lcmd == 'q') { /* quadratic bezier */
            const float ctl[3] = {coords[0] * scale_w + off_x, coords[1] * scale_h + off_y, 0.f};
            const float to[3]  = {coords[2] * scale_w + off_x, coords[3] * scale_h + off_y, 0.f};
            ngli_path_bezier2_to(path, ctl, to);
            memcpy(cursor, to, sizeof(cursor));
        } else if (lcmd == 'c') { /* cubic bezier */
            const float ctl1[3] = {coords[0] * scale_w + off_x, coords[1] * scale_h + off_y, 0.f};
            const float ctl2[3] = {coords[2] * scale_w + off_x, coords[3] * scale_h + off_y, 0.f};
            const float to[3]   = {coords[4] * scale_w + off_x, coords[5] * scale_h + off_y, 0.f};
            ngli_path_bezier3_to(path, ctl1, ctl2, to);
            memcpy(cursor, to, sizeof(cursor));
        } else {
            ngli_assert(0);
        }
    }

    return 0;
}

static void get_char_box_dim(const char *s, int *wp, int *hp, int *np)
{
    int w = 0, h = 1;
    int cur_w = 0;
    int n = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') {
            cur_w = 0;
            h++;
        } else {
            cur_w++;
            w = NGLI_MAX(w, cur_w);
            n++;
        }
    }
    *wp = w;
    *hp = h;
    *np = n;
}

static int text_builtin_init(struct text *text)
{
    struct text_builtin *s = text->priv_data;

    if (text->config.wmode != NGLI_TEXT_WRITING_MODE_UNDEFINED &&
        text->config.wmode != NGLI_TEXT_WRITING_MODE_VERTICAL_LR) {
        LOG(ERROR, "writing mode is not supported with the builtin text");
        return NGL_ERROR_UNSUPPORTED;
    }

    const int h_res = text->config.dpi[0];
    const int v_res = text->config.dpi[1];
    s->chr_w = text->config.pt_size * h_res / 72;
    s->chr_h = text->config.pt_size * v_res / 72;

    s->distmap = ngli_distmap_create(text->ctx);
    if (!s->distmap)
        return NGL_ERROR_MEMORY;

    // XXX share with external?
    s->spread = TEXT_DISTMAP_SPREAD_PCENT * NGLI_MAX(s->chr_w, s->chr_h) / 100;
    const float spread_ratio_w = s->spread / (float)s->chr_w;
    const float spread_ratio_h = s->spread / (float)s->chr_h;
    //LOG(ERROR, "selected spread: %d, char size: %dx%d, ratio:%g,%g",
    //    s->spread, s->chr_w, s->chr_h, spread_ratio_w, spread_ratio_h);
    const struct distmap_params params = {
        //.spread      = s->spread,
        .shape_w     = s->chr_w,
        .shape_h     = s->chr_h,
        /* SVG origin is in the top-left corner */
        /*
         * polynomials are defined within the [0;1] box, but we want some
         * padding for spreading effect, so we enlarge the coordinates system
         * by the amount of spread.
         */
        .poly_corner = {0.f - spread_ratio_w, 1.f + spread_ratio_h},
        .poly_width  = {1.f + 2.f*spread_ratio_w, 0.f},
        .poly_height = {0.f, -1.f - 2.f*spread_ratio_h},
    };

    int ret = ngli_distmap_init(s->distmap, &params);
    if (ret < 0)
        return ret;

    s->path = ngli_path_create();
    if (!s->path)
        return NGL_ERROR_MEMORY;

    for (int i = 0; i < NGLI_ARRAY_NB(outlines); i++) {
        ngli_path_clear(s->path);

        ret = load_outline(s->path, outlines[i]);
        if (ret < 0)
            return ret;

        ret = ngli_path_add_to_distmap(s->path, s->distmap, i);
        if (ret < 0)
            return ret;
    }

    ngli_path_freep(&s->path);

    ret = ngli_distmap_generate_texture(s->distmap);
    if (ret < 0)
        return ret;

    text->texture = ngli_distmap_get_texture(s->distmap);

    return 0;
}

/* XXX duplicated */
static enum char_tag get_char_tags(char c)
{
    if (c == ' ')
        return NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR;
    if (c == '\n')
        return NGLI_TEXT_CHAR_TAG_LINE_BREAK | NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR;
    return NGLI_TEXT_CHAR_TAG_GLYPH;
}

static int text_builtin_set_string(struct text *text, const char *str)
{
    struct text_builtin *s = text->priv_data;

    int text_cols, text_rows, text_nbchr;
    get_char_box_dim(str, &text_cols, &text_rows, &text_nbchr);

    text->width  = text_cols * s->chr_w + 2 * text->config.padding;
    text->height = text_rows * s->chr_h + 2 * text->config.padding;

    int col = 0, row = 0;
    for (int i = 0; str[i]; i++) {
        const enum char_tag tag = get_char_tags(str[i]);
        if (tag != NGLI_TEXT_CHAR_TAG_GLYPH) {
            const struct char_info chr = {.tags = tag};
            if (!ngli_darray_push(&text->chars, &chr))
                return NGL_ERROR_MEMORY;
            if ((tag & NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR) == tag) {
                col++;
            } else if (tag & NGLI_TEXT_CHAR_TAG_LINE_BREAK) {
                row++;
                col = 0;
            }
            continue;
        }

        const int glyph_id = str[i] - FIRST_CHAR;
        /*
         * TODO insert a special glyph when unknown (square or crossed square
         * maybe)
         */
        if (glyph_id < 0 || glyph_id >= NGLI_ARRAY_NB(outlines)) {
            LOG(WARNING, "ignoring unknown character 0x%x at position %d", glyph_id, i);
            continue;
        }

        struct char_info chr = {
            .x = text->config.padding + s->chr_w * col - s->spread,
            .y = text->config.padding + s->chr_h * (text_rows - row - 1) - s->spread,
            .w = s->chr_w + 2.f * s->spread,
            .h = s->chr_h + 2.f * s->spread,
            .tags = NGLI_TEXT_CHAR_TAG_GLYPH,
            .line = row,
        };

        ngli_distmap_get_shape_coords(s->distmap, glyph_id, chr.atlas_uvcoords);

        if (!ngli_darray_push(&text->chars, &chr))
            return NGL_ERROR_MEMORY;
        col++;
    }

    return 0;
}

static void text_builtin_reset(struct text *text)
{
    struct text_builtin *s = text->priv_data;
    ngli_path_freep(&s->path);
    ngli_distmap_freep(&s->distmap);
}

const struct text_cls ngli_text_builtin = {
    .init       = text_builtin_init,
    .set_string = text_builtin_set_string,
    .reset      = text_builtin_reset,
    .priv_size  = sizeof(struct text_builtin),
};
