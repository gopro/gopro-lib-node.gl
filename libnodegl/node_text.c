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

#define _XOPEN_SOURCE 500 // random
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bstr.h"
#include "memory.h"
#include "nodes.h"
#include "darray.h"
#include "drawutils.h"
#include "gpu_ctx.h"
#include "log.h"
#include "math_utils.h"
#include "pgcache.h"
#include "pgcraft.h"
#include "pipeline.h"
#include "text.h"
#include "text_frag.h"
#include "text_vert.h"
#include "type.h"
#include "topology.h"
#include "utils.h"


#define VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                            NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT) \

#define INDEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_TRANSFER_DST_BIT | \
                           NGLI_BUFFER_USAGE_INDEX_BUFFER_BIT)  \

#define DYNAMIC_VERTEX_USAGE_FLAGS (NGLI_BUFFER_USAGE_DYNAMIC_BIT | VERTEX_USAGE_FLAGS)
#define DYNAMIC_INDEX_USAGE_FLAGS  (NGLI_BUFFER_USAGE_DYNAMIC_BIT | INDEX_USAGE_FLAGS)

struct pipeline_desc_bg {
    struct pgcraft *crafter;
    struct pipeline *pipeline;
    int modelview_matrix_index;
    int projection_matrix_index;
    int color_index;
};

struct pipeline_desc_fg {
    struct graphicstate state;
    struct rendertarget_desc rt_desc;

    struct pgcraft *crafter;
    struct pipeline *pipeline;

    /* vert */
    int modelview_matrix_index;
    int projection_matrix_index;
    int chr_transform_index;

    /* frag */
    int chr_color_index;
    int chr_outline_index;
    int chr_glow_index;
    int chr_glow_color_index;
    int chr_blur_index;
};

struct pipeline_desc {
    struct pipeline_desc_bg bg; /* Background (bounding box) */
    struct pipeline_desc_fg fg; /* Foreground (characters) */
};

struct chr_data {
    NGLI_ALIGNED_MAT(transform);
    float color[4];
    float outline;
    float glow;
    float glow_color[4];
    float blur;
};

struct chr_data_pointers {
    float *transform;
    float *color;
    float *outline;
    float *glow;
    float *glow_color;
    float *blur;
};

struct text_priv {
    /* options */
    char *text;
    float fg_color[4];
    float bg_color[4];
    float box_corner[3];
    float box_width[3];
    float box_height[3];
    struct text_config config;
    double font_scale;
    int scale_mode;
    struct ngl_node **effect_nodes;
    int nb_effect_nodes;
    int valign, halign;
    int aspect_ratio[2];
    struct ngl_node *path;

    /* characters (fg) */
    struct text *text_ctx;
    struct buffer *vertices;
    struct buffer *uvcoords;
    struct buffer *indices;
    int nb_indices;
    int **element_positions;        // position of each element (char, word, line, ...) per effect
    int *element_counts;            // number of element per effect
    struct chr_data_pointers chr;
    float *chars_data;
    float *chars_data_default;
    int chars_data_size;

    /* background box */
    struct buffer *bg_vertices;
    struct buffer *bg_indices;
    int nb_bg_indices;

    struct darray pipeline_descs;
    int live_changed;
};

#define VALIGN_CENTER 0
#define VALIGN_TOP    1
#define VALIGN_BOTTOM 2

#define HALIGN_CENTER NGLI_TEXT_HALIGN_CENTER
#define HALIGN_RIGHT  NGLI_TEXT_HALIGN_RIGHT
#define HALIGN_LEFT   NGLI_TEXT_HALIGN_LEFT

static const struct param_choices valign_choices = {
    .name = "valign",
    .consts = {
        {"center", VALIGN_CENTER, .desc=NGLI_DOCSTRING("vertically centered")},
        {"bottom", VALIGN_BOTTOM, .desc=NGLI_DOCSTRING("bottom positioned")},
        {"top",    VALIGN_TOP,    .desc=NGLI_DOCSTRING("top positioned")},
        {NULL}
    }
};

static const struct param_choices halign_choices = {
    .name = "halign",
    .consts = {
        {"center", HALIGN_CENTER, .desc=NGLI_DOCSTRING("horizontally centered")},
        {"right",  HALIGN_RIGHT,  .desc=NGLI_DOCSTRING("right positioned")},
        {"left",   HALIGN_LEFT,   .desc=NGLI_DOCSTRING("left positioned")},
        {NULL}
    }
};

static const struct param_choices writing_mode_choices = {
    .name = "writing_mode",
    .consts = {
        {"undefined",     NGLI_TEXT_WRITING_MODE_UNDEFINED,
                          .desc=NGLI_DOCSTRING("undefined (automatic)")},
        {"horizontal-tb", NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB,
                          .desc=NGLI_DOCSTRING("LTR: left-to-right flow then top-to-bottom per line, "
                                               "RTL: right-to-left flow then top-to-bottom per line")},
        {"vertical-rl",   NGLI_TEXT_WRITING_MODE_VERTICAL_LR,
                          .desc=NGLI_DOCSTRING("LTR: top-to-bottom flow then right-to-left per line, "
                                               "RTL: bottom-to-top flow then left-to-right per line")},
        {"vertical-lr",   NGLI_TEXT_WRITING_MODE_VERTICAL_RL,
                          .desc=NGLI_DOCSTRING("LTR: top-to-bottom flow then left-to-right per line, "
                                               "RTL: bottom-to-top flow then right-to-left per line")},
        {NULL}
    }
};

#define SCALE_MODE_AUTO  0
#define SCALE_MODE_FIXED 1

static const struct param_choices scale_mode_choices = {
    .name = "scale_mode",
    .consts = {
        {"auto",  SCALE_MODE_AUTO,  .desc=NGLI_DOCSTRING("automatic size by fitting the specified bounding box")},
        {"fixed", SCALE_MODE_FIXED, .desc=NGLI_DOCSTRING("fixed character size (bounding box ignored for scaling)")},
        {NULL}
    }
};

static int set_live_changed(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    s->live_changed = 1;
    return 0;
}

#define OFFSET(x) offsetof(struct text_priv, x)
static const struct node_param text_params[] = {
    {"text",         NGLI_PARAM_TYPE_STR, OFFSET(text), {.str=""},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_NON_NULL,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("text string to rasterize")},
    {"fg_color",     NGLI_PARAM_TYPE_VEC4, OFFSET(fg_color), {.vec={1.0, 1.0, 1.0, 1.0}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("foreground text color")},
    {"bg_color",     NGLI_PARAM_TYPE_VEC4, OFFSET(bg_color), {.vec={0.0, 0.0, 0.0, 0.8}},
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .desc=NGLI_DOCSTRING("background text color")},
    {"box_corner",   NGLI_PARAM_TYPE_VEC3, OFFSET(box_corner), {.vec={-1.0, -1.0, 0.0}},
                     .desc=NGLI_DOCSTRING("origin coordinates of `box_width` and `box_height` vectors")},
    {"box_width",    NGLI_PARAM_TYPE_VEC3, OFFSET(box_width), {.vec={2.0, 0.0, 0.0}},
                     .desc=NGLI_DOCSTRING("box width vector")},
    {"box_height",   NGLI_PARAM_TYPE_VEC3, OFFSET(box_height), {.vec={0.0, 2.0, 0.0}},
                     .desc=NGLI_DOCSTRING("box height vector")},
    {"font_file",    NGLI_PARAM_TYPE_STR, OFFSET(config.fontfile),
                     .desc=NGLI_DOCSTRING("path to font file (require build with external text libraries)")},
    {"pt_size",      NGLI_PARAM_TYPE_INT, OFFSET(config.pt_size), {.i64=54},
                     .desc=NGLI_DOCSTRING("characters size in point (nominal size, 1pt = 1/72 inch)")},
    {"dpi",          NGLI_PARAM_TYPE_IVEC2, OFFSET(config.dpi), {.ivec={96, 96}},
                     .desc=NGLI_DOCSTRING("horizontal and vertical DPI (dot per inch)")},
    {"writing_mode", NGLI_PARAM_TYPE_SELECT, OFFSET(config.wmode), {.i64=NGLI_TEXT_WRITING_MODE_UNDEFINED},
                     .choices=&writing_mode_choices,
                     .desc=NGLI_DOCSTRING("direction flow per character and line")},
    {"padding",      NGLI_PARAM_TYPE_INT, OFFSET(config.padding), {.i64=3},
                     .desc=NGLI_DOCSTRING("pixel padding around the text")},
    {"font_scale",   NGLI_PARAM_TYPE_DBL, OFFSET(font_scale), {.dbl=1.0},
                     .desc=NGLI_DOCSTRING("scaling of the font")},
    {"scale_mode",   NGLI_PARAM_TYPE_SELECT, OFFSET(scale_mode), {.i64=SCALE_MODE_AUTO},
                     .choices=&scale_mode_choices,
                     .desc=NGLI_DOCSTRING("scaling behaviour for the characters")},
    {"effects",      NGLI_PARAM_TYPE_NODELIST, OFFSET(effect_nodes),
                     .node_types=(const int[]){NGL_NODE_TEXTEFFECT, -1},
                     .desc=NGLI_DOCSTRING("stack of effects")},
    {"valign",       NGLI_PARAM_TYPE_SELECT, OFFSET(valign), {.i64=VALIGN_CENTER},
                     .choices=&valign_choices,
                     .desc=NGLI_DOCSTRING("vertical alignment of the text in the box")},
    {"halign",       NGLI_PARAM_TYPE_SELECT, OFFSET(halign), {.i64=HALIGN_CENTER},
                     .choices=&halign_choices,
                     .desc=NGLI_DOCSTRING("horizontal alignment of the text in the box")},
    {"aspect_ratio", NGLI_PARAM_TYPE_RATIONAL, OFFSET(aspect_ratio),
                     .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
                     .update_func=set_live_changed,
                     .desc=NGLI_DOCSTRING("box aspect ratio")},
    {"path",         NGLI_PARAM_TYPE_NODE, OFFSET(path),
                     .node_types=(const int[]){NGL_NODE_PATH, -1},
                     .desc=NGLI_DOCSTRING("path to follow")},
    {NULL}
};

static const char * const bg_vertex_data =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    ngl_out_pos = projection_matrix * modelview_matrix * vec4(position, 1.0);"     "\n"
    "}";

static const char * const bg_fragment_data =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    ngl_out_color = color;"                                                        "\n"
    "}";

static const struct pgcraft_iovar glyph_io_vars[] = {
    {.name = "var_tex_coord",  .type = NGLI_TYPE_VEC2},
    {.name = "var_glyph_id",   .type = NGLI_TYPE_INT},
};

#define BC(index) s->box_corner[index]
#define BW(index) s->box_width[index]
#define BH(index) s->box_height[index]

#define C(index) chr_corner[index]
#define W(index) chr_width[index]
#define H(index) chr_height[index]

static void shuffle_positions(int *positions, int n)
{
    for (int i = 0; i < n - 1; i++) {
        const int r = i + random() % (n - i);
        const int cur = positions[i];
        positions[i] = positions[r];
        positions[r] = cur;
    }
}

static int get_nb_chars(const struct darray *chars_array)
{
    return ngli_darray_count(chars_array);
}

static int get_nb_chars_no_space(const struct darray *chars_array)
{
    int ret = 0;
    const struct char_info *chars = ngli_darray_data(chars_array);
    for (int i = 0; i < ngli_darray_count(chars_array); i++) {
        const struct char_info *c = &chars[i];
        if (c->tags & NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR)
            continue;
        ret++;
    }
    return ret;
}

struct element_info {
    int start;
    int end; // exclusive
};

static struct element_info get_next_elem(const struct darray *chars_array, const struct element_info last, int tag)
{
    struct element_info element = {.start=-1, .end=-1};
    int inside_element = 0; // set if we are inside the element

    const struct char_info *chars = ngli_darray_data(chars_array);
    for (int i = last.end; i < ngli_darray_count(chars_array); i++) {
        const struct char_info *c = &chars[i];
        if (c->tags & tag) {
            if (inside_element) {
                element.end = i;
                break;
            }
            inside_element = 0;
        } else if (!inside_element) {
            element.start = i;
            inside_element = 1;
        }
    }
    if (element.end == -1)
        element.end = ngli_darray_count(chars_array);
    return element;
}

static int get_nb_elems_separator(const struct darray *chars_array, int tag)
{
    int ret = 0;
    struct element_info elem = {0};
    for (;;) {
        elem = get_next_elem(chars_array, elem, tag);
        if (elem.start == -1)
            break;
        ret++;
    }
    return ret;
}

static int get_nb_words(const struct darray *chars_array)
{
    return get_nb_elems_separator(chars_array, NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR);
}

static int get_nb_lines(const struct darray *chars_array)
{
    return get_nb_elems_separator(chars_array, NGLI_TEXT_CHAR_TAG_LINE_BREAK);
}

static int get_nb_text(const struct darray *chars_array)
{
    return 1;
}

static int get_nb_elems(const struct darray *chars, int target)
{
    switch (target) {
    case NGLI_TEXT_EFFECT_CHAR:         return get_nb_chars(chars);
    case NGLI_TEXT_EFFECT_CHAR_NOSPACE: return get_nb_chars_no_space(chars);
    case NGLI_TEXT_EFFECT_WORD:         return get_nb_words(chars);
    case NGLI_TEXT_EFFECT_LINE:         return get_nb_lines(chars);
    case NGLI_TEXT_EFFECT_TEXT:         return get_nb_text(chars);
    default:
        ngli_assert(0);
    }
}

static int set_element_positions(struct text_priv *s)
{
    for (int i = 0; i < s->nb_effect_nodes; i++) {
        struct ngl_node *effect_node = s->effect_nodes[i];
        struct texteffect_priv *effect_priv = effect_node->priv_data;

        const int nb_elems = get_nb_elems(&s->text_ctx->chars, effect_priv->target);
        if (!nb_elems) { // XXX: can we do something more graceful here?
            LOG(ERROR, "element segmentation is not possible with current text");
            return NGL_ERROR_INVALID_USAGE;
        }
        s->element_counts[i] = nb_elems;

        s->element_positions[i] = ngli_calloc(nb_elems, sizeof(*s->element_positions[i]));
        if (!s->element_positions[i])
            return NGL_ERROR_MEMORY;

        for (int j = 0; j < nb_elems; j++)
            s->element_positions[i][j] = j;

        if (effect_priv->random) {
            if (effect_priv->random_seed >= 0)
                srandom(effect_priv->random_seed);
            shuffle_positions(s->element_positions[i], nb_elems);
        }
    }

    return 0;
}

static int set_f32_from_node(float *dst, struct ngl_node *node, double t)
{
    if (!node)
        return 0;
    int ret = ngli_node_update(node, t);
    if (ret < 0)
        return ret;
    const struct variable_priv *v = node->priv_data;
    *dst = v->scalar;
    return 0;
}

static int set_vec4_from_node(float *dst, struct ngl_node *node, double t)
{
    if (!node)
        return 0;
    int ret = ngli_node_update(node, t);
    if (ret < 0)
        return ret;
    const struct variable_priv *v = node->priv_data;
    memcpy(dst, v->vector, 4 * sizeof(*dst));
    return 0;
}

// XXX: need to make sure there is only a chain of transform nodes, maybe at
// init
// XXX: write directly into dst? need to malloc aligned!
static int set_transform_from_node(float *dst, struct ngl_node *node, double t)
{
    if (!node)
        return 0;
    int ret = ngli_node_update(node, t);
    if (ret < 0)
        return ret;
    NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    while (node->cls->id != NGL_NODE_IDENTITY) {
        const struct transform_priv *trf = node->priv_data;
        ngli_mat4_mul(matrix, matrix, trf->matrix);
        node = trf->child;
    }
    memcpy(dst, matrix, sizeof(matrix));
    return 0;
}

static int update_character_data(struct text_priv *s, struct texteffect_priv *effect, int c, double t)
{
    int ret;
    if ((ret = set_transform_from_node(s->chr.transform  + c * 4 * 4, effect->transform_chain, t)) < 0 ||
        (ret = set_vec4_from_node(     s->chr.color      + c * 4,     effect->color_node,      t)) < 0 ||
        (ret = set_f32_from_node(      s->chr.outline    + c,         effect->outline_node,    t)) < 0 ||
        (ret = set_f32_from_node(      s->chr.glow       + c,         effect->glow_node,       t)) < 0 ||
        (ret = set_vec4_from_node(     s->chr.glow_color + c * 4,     effect->glow_color_node, t)) < 0 ||
        (ret = set_f32_from_node(      s->chr.blur       + c,         effect->blur_node,       t)) < 0)
        return ret;
    return 0;
}

static void reset_chars_data_to_defaults(struct text_priv *s)
{
    memcpy(s->chars_data, s->chars_data_default, s->chars_data_size);
}

static struct chr_data_pointers get_chr_data_pointers(float *base, int text_nbchr)
{
    struct chr_data_pointers ptrs = {0};
    ptrs.transform  = base;
    ptrs.color      = ptrs.transform  + text_nbchr * 4 * 4;
    ptrs.outline    = ptrs.color      + text_nbchr * 4;
    ptrs.glow       = ptrs.outline    + text_nbchr;
    ptrs.glow_color = ptrs.glow       + text_nbchr;
    ptrs.blur       = ptrs.glow_color + text_nbchr * 4;
    return ptrs;
}

static void update_fg_color(struct text_priv *s, const float *color)
{
    const int nb_chars = ngli_darray_count(&s->text_ctx->chars);
    struct chr_data_pointers defaults = get_chr_data_pointers(s->chars_data_default, nb_chars);
    for (int i = 0; i < nb_chars; i++)
        memcpy(defaults.color + i * 4, color, 4 * sizeof(*color));
}

static int init_characters_data(struct text_priv *s, int text_nbchr)
{
    const struct chr_data chr_data_default = {
        .transform    = NGLI_MAT4_IDENTITY,
        .color        = {NGLI_ARG_VEC4(s->fg_color)},
        .outline      = 0.f,
        .glow         = 0.f,
        .glow_color   = {1.f, 1.f, 1.f, 1.f},
        .blur         = 0.f,
    };

    /*
     * We can not allocate an array of struct chr_data because each field
     * must be an array. This could be avoided if we had support for array of
     * struct with all backends.
     *
     * The x2 is because we duplicate the data for the defaults, which is the
     * reference data we use to reset all the characters properties at every
     * frame. The default data is positioned first for a more predictible
     * read/write memory access on copy.
     */
    s->chars_data_size = text_nbchr * sizeof(chr_data_default);
    s->chars_data_default = ngli_calloc(2, s->chars_data_size);
    if (!s->chars_data_default)
        return NGL_ERROR_MEMORY;

    struct chr_data_pointers defaults = get_chr_data_pointers(s->chars_data_default, text_nbchr);
    for (int i = 0; i < text_nbchr; i++) {
        memcpy(defaults.transform  + i * 4 * 4, chr_data_default.transform,  sizeof(chr_data_default.transform));
        memcpy(defaults.color      + i * 4,     chr_data_default.color,      sizeof(chr_data_default.color));
        memcpy(defaults.outline    + i,         &chr_data_default.outline,   sizeof(chr_data_default.outline));
        memcpy(defaults.glow       + i,         &chr_data_default.glow,      sizeof(chr_data_default.glow));
        memcpy(defaults.glow_color + i * 4,     chr_data_default.glow_color, sizeof(chr_data_default.glow_color));
        memcpy(defaults.blur       + i,         &chr_data_default.blur,      sizeof(chr_data_default.blur));
    }

    s->chars_data = (void *)(((uint8_t *)s->chars_data_default) + s->chars_data_size);
    s->chr = get_chr_data_pointers(s->chars_data, text_nbchr);
    reset_chars_data_to_defaults(s);

    return 0;
}

struct target_range {
    int start_chr;
    int end_chr;
    float overlap;
};

static int apply_effects_char(struct text_priv *s, const struct target_range *range, double effect_t, int effect_id)
{
    struct ngl_node *effect_node = s->effect_nodes[effect_id];
    struct texteffect_priv *effect_priv = effect_node->priv_data;

    const int text_nbchr = s->element_counts[effect_id];
    const float target_duration  = text_nbchr - range->overlap * (text_nbchr - 1);
    const float target_timescale = (1. - range->overlap) / target_duration;

    for (int c = range->start_chr; c < range->end_chr; c++) {
        const int c_pos = s->element_positions[effect_id][c];
        const float t_prv = target_timescale * c_pos;
        const float t_nxt = t_prv + 1. / target_duration;
        const double target_t = NGLI_LINEAR_INTERP(t_prv, t_nxt, effect_t);
        int ret = update_character_data(s, effect_priv, c, target_t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int apply_effects_char_nospace(struct text_priv *s, const struct target_range *range, double effect_t, int effect_id)
{
    struct ngl_node *effect_node = s->effect_nodes[effect_id];
    struct texteffect_priv *effect_priv = effect_node->priv_data;

    const int text_nbchr = s->element_counts[effect_id];
    const float target_duration  = text_nbchr - range->overlap * (text_nbchr - 1);
    const float target_timescale = (1. - range->overlap) / target_duration;

    const struct char_info *chars = ngli_darray_data(&s->text_ctx->chars);

    int c_id = range->start_chr;
    for (int c = range->start_chr; c < range->end_chr; c++) {
        const struct char_info *c_info = &chars[c];
        if (c_info->tags & NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR)
            continue;
        const int c_pos = s->element_positions[effect_id][c_id++];
        const float t_prv = target_timescale * c_pos;
        const float t_nxt = t_prv + 1. / target_duration;
        const double target_t = NGLI_LINEAR_INTERP(t_prv, t_nxt, effect_t);
        int ret = update_character_data(s, effect_priv, c, target_t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int apply_effects_separator(struct text_priv *s, const struct target_range *range, double effect_t, int effect_id, int tag)
{
    struct ngl_node *effect_node = s->effect_nodes[effect_id];
    struct texteffect_priv *effect_priv = effect_node->priv_data;

    const int nb_elems = s->element_counts[effect_id];
    const float target_duration  = nb_elems - range->overlap * (nb_elems - 1);
    const float target_timescale = (1. - range->overlap) / target_duration;

    int elem_id = 0;
    struct element_info elem = {.start=range->start_chr};

    for (;;) {
        elem = get_next_elem(&s->text_ctx->chars, elem, tag);
        if (elem.start == -1)
            break;

        const int pos = s->element_positions[effect_id][elem_id++];

        if (elem.start < range->start_chr)
            continue;

        for (int c = elem.start; c < elem.end; c++) {
            const float t_prv = target_timescale * pos;
            const float t_nxt = t_prv + 1. / target_duration;
            const double target_t = NGLI_LINEAR_INTERP(t_prv, t_nxt, effect_t);
            int ret = update_character_data(s, effect_priv, c, target_t);
            if (ret < 0)
                return ret;
        }

        if (elem.end >= range->end_chr)
            break;
    }
    return 0;
}

static int apply_effects_word(struct text_priv *s, const struct target_range *range, double effect_t, int effect_id)
{
    return apply_effects_separator(s, range, effect_t, effect_id, NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR);
}

static int apply_effects_line(struct text_priv *s, const struct target_range *range, double effect_t, int effect_id)
{
    return apply_effects_separator(s, range, effect_t, effect_id, NGLI_TEXT_CHAR_TAG_LINE_BREAK);
}

static int apply_effects_text(struct text_priv *s, const struct target_range *range, double effect_t, int effect_id)
{
    struct ngl_node *effect_node = s->effect_nodes[effect_id];
    struct texteffect_priv *effect_priv = effect_node->priv_data;

    for (int c = range->start_chr; c < range->end_chr; c++) {
        int ret = update_character_data(s, effect_priv, c, effect_t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int set_target_range(struct text_priv *s, struct texteffect_priv *effect, double t, struct target_range *r)
{
    int ret;
    float start_pos = 0.;
    float end_pos   = 1.;
    r->overlap = 0.;

    if ((ret = set_f32_from_node(&start_pos,  effect->start_pos_node, t)) < 0 ||
        (ret = set_f32_from_node(&end_pos,    effect->end_pos_node,   t)) < 0 ||
        (ret = set_f32_from_node(&r->overlap, effect->overlap_node,   t)) < 0)
        return ret;

    const int text_nbchr = ngli_darray_count(&s->text_ctx->chars);
    r->start_chr = NGLI_MAX(lrintf(text_nbchr * start_pos), 0);
    r->end_chr   = NGLI_MIN(lrintf(text_nbchr * end_pos), text_nbchr);
    return 0;
}

static int apply_effects(struct text_priv *s, double t)
{
    reset_chars_data_to_defaults(s);

    for (int i = 0; i < s->nb_effect_nodes; i++) {
        struct ngl_node *effect_node = s->effect_nodes[i];
        struct texteffect_priv *effect_priv = effect_node->priv_data;

        if (t < effect_priv->start_time || t > effect_priv->end_time)
            continue;

        const double effect_t = NGLI_LINEAR_INTERP(effect_priv->start_time, effect_priv->end_time, t);

        struct target_range range;
        int ret = set_target_range(s, effect_priv, effect_t, &range);
        if (ret < 0)
            return ret;

        switch (effect_priv->target) {
        case NGLI_TEXT_EFFECT_CHAR:         ret = apply_effects_char(s, &range, effect_t, i);         break;
        case NGLI_TEXT_EFFECT_CHAR_NOSPACE: ret = apply_effects_char_nospace(s, &range, effect_t, i); break;
        case NGLI_TEXT_EFFECT_WORD:         ret = apply_effects_word(s, &range, effect_t, i);         break;
        case NGLI_TEXT_EFFECT_LINE:         ret = apply_effects_line(s, &range, effect_t, i);         break;
        case NGLI_TEXT_EFFECT_TEXT:         ret = apply_effects_text(s, &range, effect_t, i);         break;
        default:
            ngli_assert(0);
        }
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int init_characters_pipeline(struct ngl_node *node, struct pipeline_desc_fg *desc)
{
    struct ngl_ctx *ctx = node->ctx;
    struct text_priv *s = node->priv_data;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    const struct pgcraft_texture textures[] = {
        {
            .name     = "tex",
            .type     = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .stage    = NGLI_PROGRAM_SHADER_FRAG,
            .texture  = s->text_ctx->texture,
        },
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC3,
            .format   = NGLI_FORMAT_R32G32B32_SFLOAT,
            .stride   = 3 * 4,
            .buffer   = s->vertices,
        },
        {
            .name     = "uvcoord",
            .type     = NGLI_TYPE_VEC2,
            .format   = NGLI_FORMAT_R32G32_SFLOAT,
            .stride   = 2 * 4,
            .buffer   = s->uvcoords,
        },
    };

    const int text_nbchr = ngli_darray_count(&s->text_ctx->chars);
    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4,  .stage = NGLI_PROGRAM_SHADER_VERT},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4,  .stage = NGLI_PROGRAM_SHADER_VERT},
        {.name = "chr_transform",     .type = NGLI_TYPE_MAT4,  .stage = NGLI_PROGRAM_SHADER_VERT, .count = text_nbchr},
        {.name = "chr_color",         .type = NGLI_TYPE_VEC4,  .stage = NGLI_PROGRAM_SHADER_FRAG, .count = text_nbchr},
        {.name = "chr_outline",       .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG, .count = text_nbchr},
        {.name = "chr_glow",          .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG, .count = text_nbchr},
        {.name = "chr_glow_color",    .type = NGLI_TYPE_VEC4,  .stage = NGLI_PROGRAM_SHADER_FRAG, .count = text_nbchr},
        {.name = "chr_blur",          .type = NGLI_TYPE_FLOAT, .stage = NGLI_PROGRAM_SHADER_FRAG, .count = text_nbchr},
    };

    /* This controls how the characters blend onto the background */
    struct graphicstate state = desc->state;
    state.blend = 1;
    state.blend_src_factor   = NGLI_BLEND_FACTOR_SRC_ALPHA;
    state.blend_dst_factor   = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.blend_src_factor_a = NGLI_BLEND_FACTOR_SRC_ALPHA;
    state.blend_dst_factor_a = NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology       = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state          = state,
            .rt_desc        = desc->rt_desc,
        }
    };

    const struct pgcraft_params crafter_params = {
        .vert_base        = text_vert,
        .frag_base        = text_frag,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = glyph_io_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(glyph_io_vars),
    };

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    struct pipeline_resource_params pipeline_resource_params = {0};
    int ret = ngli_pgcraft_craft(desc->crafter, &pipeline_params, &pipeline_resource_params, &crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline = ngli_pipeline_create(gpu_ctx);
    if (!desc->pipeline)
        return NGL_ERROR_MEMORY;

    ret = ngli_pipeline_init(desc->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    ret = ngli_pipeline_set_resources(desc->pipeline, &pipeline_resource_params);
    if (ret < 0)
        return ret;

    ngli_assert(!strcmp("position", pipeline_params.attributes_desc[0].name));
    ngli_assert(!strcmp("uvcoord", pipeline_params.attributes_desc[1].name));

    desc->modelview_matrix_index  = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix",  NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->chr_transform_index     = ngli_pgcraft_get_uniform_index(desc->crafter, "chr_transform",     NGLI_PROGRAM_SHADER_VERT);
    desc->chr_color_index         = ngli_pgcraft_get_uniform_index(desc->crafter, "chr_color",         NGLI_PROGRAM_SHADER_FRAG);
    desc->chr_outline_index       = ngli_pgcraft_get_uniform_index(desc->crafter, "chr_outline",       NGLI_PROGRAM_SHADER_FRAG);
    desc->chr_glow_index          = ngli_pgcraft_get_uniform_index(desc->crafter, "chr_glow",          NGLI_PROGRAM_SHADER_FRAG);
    desc->chr_glow_color_index    = ngli_pgcraft_get_uniform_index(desc->crafter, "chr_glow_color",    NGLI_PROGRAM_SHADER_FRAG);
    desc->chr_blur_index          = ngli_pgcraft_get_uniform_index(desc->crafter, "chr_blur",          NGLI_PROGRAM_SHADER_FRAG);

    return 0;
}

static void destroy_characters_resources(struct text_priv *s)
{
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const int nb_descs = ngli_darray_count(&s->pipeline_descs);
    for (int i = 0; i < nb_descs; i++) {
        struct pipeline_desc_fg *desc = &descs[i].fg;
        ngli_pipeline_freep(&desc->pipeline);
        ngli_pgcraft_freep(&desc->crafter);

        desc->modelview_matrix_index = -1;
        desc->projection_matrix_index = -1;
        desc->chr_transform_index = -1;
        desc->chr_color_index = -1;
        desc->chr_outline_index = -1;
        desc->chr_glow_index = -1;
        desc->chr_glow_color_index = -1;
        desc->chr_blur_index = -1;
    }
    ngli_buffer_freep(&s->vertices);
    ngli_buffer_freep(&s->uvcoords);
    ngli_buffer_freep(&s->indices);
    s->nb_indices = 0;
    for (int i = 0; i < s->nb_effect_nodes; i++)
        ngli_freep(&s->element_positions[i]);
    s->chars_data = NULL;
    ngli_freep(&s->chars_data_default);
    s->chars_data_size = 0;
}

static int update_character_geometries(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;

    struct text *text = s->text_ctx;
    int ret = ngli_text_set_string(text, s->text);
    if (ret < 0)
        return ret;

    const int text_nbchr = ngli_darray_count(&text->chars);
    if (!text_nbchr) {
        destroy_characters_resources(s);
        return 0;
    }

    // XXX: rename nb to sizes or something
    const int nb_vertices = text_nbchr * 4 * 3;
    const int nb_uvcoords = text_nbchr * 4 * 2;
    const int nb_indices  = text_nbchr * 6;
    float *vertices = ngli_calloc(nb_vertices, sizeof(*vertices));
    float *uvcoords = ngli_calloc(nb_uvcoords, sizeof(*uvcoords));
    short *indices  = ngli_calloc(nb_indices, sizeof(*indices));
    if (!vertices || !uvcoords || !indices) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    /* Text/Box ratio */
    const float box_width_len  = ngli_vec3_length(s->box_width);
    const float box_height_len = ngli_vec3_length(s->box_height);
    static const int default_ar[2] = {1, 1};
    const int *ar = s->aspect_ratio[1] ? s->aspect_ratio : default_ar;
    const float box_ratio = ar[0] * box_width_len / (float)(ar[1] * box_height_len);
    const float text_ratio = text->width / (float)text->height;

    float ratio_w, ratio_h;
    if (text_ratio < box_ratio) {
        ratio_w = text_ratio / box_ratio;
        ratio_h = 1.0;
    } else {
        ratio_w = 1.0;
        ratio_h = box_ratio / text_ratio;
    }

    float corner[3];
    float width[3];
    float height[3];

#define FIXED_SCALE 1/320./64.

    /* Apply aspect ratio and font scaling */
    if (s->scale_mode == SCALE_MODE_FIXED) {
        const float tw = text->width * FIXED_SCALE * s->font_scale * ar[1]/ar[0];
        const float th = text->height * FIXED_SCALE * s->font_scale;
        const float rw = tw / box_width_len;
        const float rh = th / box_height_len;

        ngli_vec3_scale(width, s->box_width, rw);
        ngli_vec3_scale(height, s->box_height, rh);
    } else {
        ngli_vec3_scale(width, s->box_width, ratio_w * s->font_scale);
        ngli_vec3_scale(height, s->box_height, ratio_h * s->font_scale);
    }

    /* Adjust text position according to alignment settings */
    float align_padw[3];
    float align_padh[3];
    ngli_vec3_sub(align_padw, s->box_width,  width);
    ngli_vec3_sub(align_padh, s->box_height, height);

    const float spx = (s->halign == HALIGN_CENTER ? .5f :
                       s->halign == HALIGN_RIGHT  ? 1.f :
                       0.f);
    const float spy = (s->valign == VALIGN_CENTER ? .5f :
                       s->valign == VALIGN_TOP    ? 1.f :
                       0.f);

    corner[0] = BC(0) + align_padw[0] * spx + align_padh[0] * spy;
    corner[1] = BC(1) + align_padw[1] * spx + align_padh[1] * spy;
    corner[2] = BC(2) + align_padw[2] * spx + align_padh[2] * spy;

    const struct char_info *chars = ngli_darray_data(&text->chars);
    for (int n = 0; n < text_nbchr; n++) {
        const struct char_info *chr = &chars[n];
        float chr_width[3], chr_height[3], chr_corner[3];

        /* character dimension */
        const float rw = chr->w / (float)text->width;
        const float rh = chr->h / (float)text->height;
        ngli_vec3_scale(chr_width, width, rw);
        ngli_vec3_scale(chr_height, height, rh);

        /* character position */
        // rx: x-axis ratio of the width
        const float rx = chr->x / (float)text->width;
        const float ry = chr->y / (float)text->height;

        chr_corner[0] = corner[0] + width[0] * rx + height[0] * ry;
        chr_corner[1] = corner[1] + width[1] * rx + height[1] * ry;
        chr_corner[2] = corner[2] + width[2] * rx + height[2] * ry;

#if 0
        if (s->path) {
            struct path_priv *path = s->path->priv_data;

            const float path_length = path->arc_distances[path->arc_distances_count - 1];
            const float t = rx + rw / 2.f;
            const float d = t * path_length;

            // XXX: z ignored
            // XXX: move eval 0 and 1 out of the loop
            float pos_0[3], pos_1[3], pos_t[3];
            ngli_path_evaluate(path, pos_0, 0);
            ngli_path_evaluate(path, pos_t, t);
            ngli_path_evaluate(path, pos_1, 1);

            /*
             * We assume a straight horizontal line of length 1 (from x=0 to
             * x=1), which gets distorted by the bézier curve.
             * XXX: need to adjust to honor text direction (could be vertical,
             * could be from x=1 to x=0, etc)
             */

            const float px = pos_t[0];
            const float py = pos_t[1];
            const float px0 = pos_0[0];
            const float py0 = pos_0[1];

            LOG(ERROR, "path_length:%f t:%f d:%f p0:(%f,%f) p:(%f,%f)", path_length, t, d, px0, py0, px, py);

            rx += px - px0 - d;
            ry += py - py0;

            //const float tx = NGLI_LINEAR_INTERP(pos_0[0], pos_1[0], pos_t[0]);
            //const float ty = NGLI_LINEAR_INTERP(pos_0[1], pos_1[1], pos_t[1]);
        }
#endif

        /* quad vertices */
        const float chr_vertices[] = {
            C(0),               C(1),               C(2),
            C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
            C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
            C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
        };
        memcpy(vertices + 4 * 3 * n, chr_vertices, sizeof(chr_vertices));

        /* focus uvcoords on the character in the atlas texture */
        memcpy(uvcoords + 4 * 2 * n, chr->atlas_uvcoords, sizeof(chr->atlas_uvcoords));

        /* quad for each character is made of 2 triangles */
        const short chr_indices[] = { n*4 + 0, n*4 + 1, n*4 + 2, n*4 + 1, n*4 + 3, n*4 + 2 };
        memcpy(indices + n * NGLI_ARRAY_NB(chr_indices), chr_indices, sizeof(chr_indices));
    }

    if (nb_indices > s->nb_indices) { // need re-alloc
        destroy_characters_resources(s);

        ret = set_element_positions(s);
        if (ret < 0)
            return ret;

        ret = init_characters_data(s, text_nbchr);
        if (ret < 0)
            return ret;

        s->vertices = ngli_buffer_create(gpu_ctx);
        s->uvcoords = ngli_buffer_create(gpu_ctx);
        s->indices  = ngli_buffer_create(gpu_ctx);
        if (!s->vertices || !s->uvcoords || !s->indices) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }

        if ((ret = ngli_buffer_init(s->vertices, nb_vertices * sizeof(*vertices), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->uvcoords, nb_uvcoords * sizeof(*uvcoords), DYNAMIC_VERTEX_USAGE_FLAGS)) < 0 ||
            (ret = ngli_buffer_init(s->indices,  nb_indices  * sizeof(*indices),  DYNAMIC_INDEX_USAGE_FLAGS)) < 0)
            goto end;

        struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
        const int nb_descs = ngli_darray_count(&s->pipeline_descs);
        for (int i = 0; i < nb_descs; i++) {
            struct pipeline_desc_fg *desc = &descs[i].fg;

            ret = init_characters_pipeline(node, desc);
            if (ret < 0)
                goto end;

            ngli_pipeline_update_attribute(desc->pipeline, 0, s->vertices);
            ngli_pipeline_update_attribute(desc->pipeline, 1, s->uvcoords);
        }
    } else {
        update_fg_color(s, s->fg_color);
    }

    if ((ret = ngli_buffer_upload(s->vertices, vertices, nb_vertices * sizeof(*vertices), 0)) < 0 ||
        (ret = ngli_buffer_upload(s->uvcoords, uvcoords, nb_uvcoords * sizeof(*uvcoords), 0)) < 0 ||
        (ret = ngli_buffer_upload(s->indices, indices, nb_indices * sizeof(*indices), 0)) < 0)
        goto end;

    s->nb_indices = nb_indices;

end:
    ngli_free(vertices);
    ngli_free(uvcoords);
    ngli_free(indices);
    return ret;
}

static int init_bounding_box_geometry(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct text_priv *s = node->priv_data;

    static const short indices[] = { 0, 1, 2, 0, 2, 3 };
    const float vertices[] = {
        BC(0),                 BC(1),                 BC(2),
        BC(0) + BW(0),         BC(1) + BW(1),         BC(2) + BW(2),
        BC(0) + BH(0) + BW(0), BC(1) + BH(1) + BW(1), BC(2) + BH(2) + BW(2),
        BC(0) + BH(0),         BC(1) + BH(1),         BC(2) + BH(2),
    };

    s->bg_vertices = ngli_buffer_create(gpu_ctx);
    s->bg_indices  = ngli_buffer_create(gpu_ctx);
    if (!s->bg_vertices || !s->bg_indices)
        return NGL_ERROR_MEMORY;

    int ret;
    if ((ret = ngli_buffer_init(s->bg_vertices, sizeof(vertices), VERTEX_USAGE_FLAGS)) < 0 ||
        (ret = ngli_buffer_init(s->bg_indices,  sizeof(indices),  INDEX_USAGE_FLAGS)) < 0)
        return ret;

    if ((ret = ngli_buffer_upload(s->bg_vertices, vertices, sizeof(vertices), 0)) < 0 ||
        (ret = ngli_buffer_upload(s->bg_indices,  indices,  sizeof(indices), 0)) < 0)
        return ret;

    s->nb_bg_indices = NGLI_ARRAY_NB(indices);

    return 0;
}

static int text_init(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;

    s->config.halign = s->halign; // XXX

    s->text_ctx = ngli_text_create(node->ctx);
    if (!s->text_ctx)
        return NGL_ERROR_MEMORY;

    int ret = ngli_text_init(s->text_ctx, &s->config);
    if (ret < 0)
        return ret;

    ngli_darray_init(&s->pipeline_descs, sizeof(struct pipeline_desc), 0);

    ret = init_bounding_box_geometry(node);
    if (ret < 0)
        return ret;

    if (s->nb_effect_nodes) {
        s->element_positions = ngli_calloc(s->nb_effect_nodes, sizeof(*s->element_positions));
        s->element_counts    = ngli_calloc(s->nb_effect_nodes, sizeof(*s->element_counts));
        if (!s->element_positions || !s->element_counts)
            return NGL_ERROR_MEMORY;
    }

    ret = update_character_geometries(node);
    if (ret < 0)
        return ret;

    return 0;
}

static int bg_prepare(struct ngl_node *node, struct pipeline_desc_bg *desc)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct text_priv *s = node->priv_data;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;

    const struct pgcraft_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .stage = NGLI_PROGRAM_SHADER_VERT, .data = NULL},
        {.name = "color",             .type = NGLI_TYPE_VEC4, .stage = NGLI_PROGRAM_SHADER_FRAG, .data = s->bg_color},
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC3,
            .format   = NGLI_FORMAT_R32G32B32_SFLOAT,
            .stride   = 3 * 4,
            .buffer   = s->bg_vertices,
        },
    };

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology       = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state          = rnode->graphicstate,
            .rt_desc        = rnode->rendertarget_desc,
        }
    };

    const struct pgcraft_params crafter_params = {
        .vert_base        = bg_vertex_data,
        .frag_base        = bg_fragment_data,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
    };

    desc->crafter = ngli_pgcraft_create(ctx);
    if (!desc->crafter)
        return NGL_ERROR_MEMORY;

    struct pipeline_resource_params pipeline_resource_params = {0};
    int ret = ngli_pgcraft_craft(desc->crafter, &pipeline_params, &pipeline_resource_params, &crafter_params);
    if (ret < 0)
        return ret;

    desc->pipeline = ngli_pipeline_create(gpu_ctx);
    if (!desc->pipeline)
        return NGL_ERROR_MEMORY;

    ret = ngli_pipeline_init(desc->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    ret = ngli_pipeline_set_resources(desc->pipeline, &pipeline_resource_params);
    if (ret < 0)
        return ret;

    desc->modelview_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "modelview_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->projection_matrix_index = ngli_pgcraft_get_uniform_index(desc->crafter, "projection_matrix", NGLI_PROGRAM_SHADER_VERT);
    desc->color_index = ngli_pgcraft_get_uniform_index(desc->crafter, "color", NGLI_PROGRAM_SHADER_FRAG);

    return 0;
}

static int fg_prepare(struct ngl_node *node, struct pipeline_desc_fg *desc)
{
    struct ngl_ctx *ctx = node->ctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct text_priv *s = node->priv_data;

    desc->state   = rnode->graphicstate;
    desc->rt_desc = rnode->rendertarget_desc;

    const int text_nbchr = ngli_darray_count(&s->text_ctx->chars);
    if (!text_nbchr)
        return 0;

    return init_characters_pipeline(node, desc);
}

static int text_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct text_priv *s = node->priv_data;

    struct pipeline_desc *desc = ngli_darray_push(&s->pipeline_descs, NULL);
    if (!desc)
        return NGL_ERROR_MEMORY;
    ctx->rnode_pos->id = ngli_darray_count(&s->pipeline_descs) - 1;

    memset(desc, 0, sizeof(*desc));

    int ret = bg_prepare(node, &desc->bg);
    if (ret < 0)
        return ret;

    ret = fg_prepare(node, &desc->fg);
    if (ret < 0)
        return ret;

    return 0;
}

static int text_update(struct ngl_node *node, double t)
{
    int ret;
    struct text_priv *s = node->priv_data;

    if (s->live_changed) {
        ret = update_character_geometries(node);
        if (ret < 0)
            return ret;
        s->live_changed = 0;
    }

    ret = apply_effects(s, t);
    if (ret < 0)
        return ret;

    return 0;
}

static void text_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct text_priv *s = node->priv_data;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    struct pipeline_desc *desc = &descs[ctx->rnode_pos->id];

    if (ctx->begin_render_pass) {
        struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
        ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
        ctx->begin_render_pass = 0;
    }

    struct pipeline_desc_bg *bg_desc = &desc->bg;
    ngli_pipeline_update_uniform(bg_desc->pipeline, bg_desc->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_update_uniform(bg_desc->pipeline, bg_desc->projection_matrix_index, projection_matrix);
    ngli_pipeline_update_uniform(bg_desc->pipeline, bg_desc->color_index, s->bg_color);
    ngli_pipeline_draw_indexed(bg_desc->pipeline, s->bg_indices, NGLI_FORMAT_R16_UNORM, s->nb_bg_indices, 1);

    if (s->nb_indices) {
        struct pipeline_desc_fg *fg_desc = &desc->fg;
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->modelview_matrix_index,  modelview_matrix);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->projection_matrix_index, projection_matrix);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->chr_transform_index,     s->chr.transform);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->chr_color_index,         s->chr.color);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->chr_outline_index,       s->chr.outline);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->chr_glow_index,          s->chr.glow);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->chr_glow_color_index,    s->chr.glow_color);
        ngli_pipeline_update_uniform(fg_desc->pipeline, fg_desc->chr_blur_index,          s->chr.blur);
        ngli_pipeline_draw_indexed(fg_desc->pipeline, s->indices, NGLI_FORMAT_R16_UNORM, s->nb_indices, 1);
    }
}

static void text_uninit(struct ngl_node *node)
{
    struct text_priv *s = node->priv_data;
    struct pipeline_desc *descs = ngli_darray_data(&s->pipeline_descs);
    const int nb_descs = ngli_darray_count(&s->pipeline_descs);
    for (int i = 0; i < nb_descs; i++) {
        struct pipeline_desc *desc = &descs[i];
        ngli_pipeline_freep(&desc->bg.pipeline);
        ngli_pgcraft_freep(&desc->bg.crafter);
    }
    ngli_buffer_freep(&s->bg_vertices);
    ngli_buffer_freep(&s->bg_indices);

    destroy_characters_resources(s);
    ngli_freep(&s->element_positions);
    ngli_freep(&s->element_counts);
    ngli_darray_reset(&s->pipeline_descs);
    ngli_text_freep(&s->text_ctx);
}

const struct node_class ngli_text_class = {
    .id        = NGL_NODE_TEXT,
    .name      = "Text",
    .init      = text_init,
    .prepare   = text_prepare,
    .update    = text_update,
    .draw      = text_draw,
    .uninit    = text_uninit,
    .priv_size = sizeof(struct text_priv),
    .params    = text_params,
    .file      = __FILE__,
};
