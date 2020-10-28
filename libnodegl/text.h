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

#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

#include "darray.h"
#include "distmap.h"
#include "hmap.h"
#include "nodes.h"

#define TEXT_DISTMAP_SPREAD_PCENT 25

enum writing_mode {
    NGLI_TEXT_WRITING_MODE_UNDEFINED,
    NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB,
    NGLI_TEXT_WRITING_MODE_VERTICAL_RL,
    NGLI_TEXT_WRITING_MODE_VERTICAL_LR,
};

enum char_tag {
    NGLI_TEXT_CHAR_TAG_GLYPH,
    NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR,
    NGLI_TEXT_CHAR_TAG_LINE_BREAK,
};

// FIXME: replace with directional alignment? (start, middle, end) so that it's
// compatible with all directional texts
enum text_halign {
    NGLI_TEXT_HALIGN_CENTER,
    NGLI_TEXT_HALIGN_RIGHT,
    NGLI_TEXT_HALIGN_LEFT,
};

struct char_info {
    /* quad information */
    int x, y, w, h;
    float atlas_uvcoords[8];

    enum char_tag tags;
    int line;
};

/*
 * WARNING: DO NOT USE ENUMS (typically for wmode and halign): they are
 * directly used by the param system and enum doesn't have a fixed size.
 */
struct text_config {
    const char *fontfile;
    int pt_size;
    int dpi[2];
    int wmode; // enum writing_mode
    int padding;
    int halign; // enum text_halign
};

struct text;

/* structure reserved for internal implementations */
struct text_cls {
    int (*init)(struct text *s);
    int (*set_string)(struct text *s, const char *str);
    void (*reset)(struct text *s);
    int priv_size;
};

struct text {
    struct ngl_ctx *ctx;
    struct text_config config;
    int width;
    int height;
    struct darray chars; // struct char_info
    struct texture *texture;
    const struct text_cls *cls;
    void *priv_data;
};

struct text *ngli_text_create(struct ngl_ctx *ctx);
int ngli_text_init(struct text *s, const struct text_config *cfg);
int ngli_text_set_string(struct text *s, const char *str);
void ngli_text_freep(struct text **sp);

#endif
