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

#include "config.h"

#ifdef HAVE_TEXTLIBS
#include <hb.h>
#include <hb-ft.h>
#include FT_OUTLINE_H

#include "log.h"
#include "memory.h"
#include "path.h"
#include "text.h"

struct text_line {
    int size; // used for alignment
    hb_buffer_t *buffer; // TODO: likely needs to become an array of words
    int start; // starting offset of this buffer run in the user input string
};

struct text_external {
    struct distmap *distmap;
    struct path *path;
    struct darray lines; // struct text_line
    struct hmap *glyph_index; // struct glyph
    FT_Library ft_library;
    FT_Face ft_face;
    hb_font_t *hb_font;
    FT_BBox cbox; // current glyph control box
    int spread; // unit: pixel, representation: 26.6
};

#define INT_TO_26D6(x) ((x) * (1 << 6)) /* convert integer to 26.6 fixed point */

static int text_external_init(struct text *text)
{
    struct text_external *s = text->priv_data;

    FT_Error ft_error;
    if ((ft_error = FT_Init_FreeType(&s->ft_library)) ||
        (ft_error = FT_New_Face(s->ft_library, text->config.fontfile, 0, &s->ft_face))) {
        LOG(ERROR, "unable to initialize FreeType with font %s", text->config.fontfile);
        return NGL_ERROR_EXTERNAL;
    }

    if (!FT_IS_SCALABLE(s->ft_face)) {
        LOG(ERROR, "only scalable faces are supported");
        return NGL_ERROR_UNSUPPORTED;
    }

    const FT_F26Dot6 chr_w = INT_TO_26D6(text->config.pt_size);
    const FT_F26Dot6 chr_h = INT_TO_26D6(text->config.pt_size);
    const FT_UInt h_res = text->config.dpi[0];
    const FT_UInt v_res = text->config.dpi[1];
    LOG(ERROR, "char size:%dpt res:%ux%u", text->config.pt_size, h_res, v_res);
    ft_error = FT_Set_Char_Size(s->ft_face, chr_w, chr_h, h_res, v_res);
    if (ft_error)
        return NGL_ERROR_EXTERNAL;

    const FT_Face face = s->ft_face;
    LOG(ERROR, "loaded font family %s", face->family_name);
    if (face->style_name)
        LOG(ERROR, "* style: %s", face->style_name);
    LOG(ERROR, "* num glyphs: %ld", face->num_glyphs);
    LOG(ERROR, "* bbox xmin:%ld xmax:%ld ymin:%ld ymax:%ld",
        face->bbox.xMin, face->bbox.xMax,
        face->bbox.yMin, face->bbox.yMax);
    LOG(ERROR, "* units_per_EM: %d ", face->units_per_EM);
    LOG(ERROR, "* ascender:  %d ", face->ascender);
    LOG(ERROR, "* descender: %d ", face->descender);
    LOG(ERROR, "* height: %d ", face->height);
    LOG(ERROR, "* max_advance_[width:%d height:%d]",
        face->max_advance_width, face->max_advance_height);
    LOG(ERROR, "* underline_[position:%d thickness:%d]",
        face->underline_position, face->underline_thickness);

    s->hb_font = hb_ft_font_create(face, NULL);
    if (!s->hb_font)
        return NGL_ERROR_MEMORY;

    /*
     * We define a fixed padding common to all glyphs: while each glyph has a
     * different control box, we do not want the padding to be relative to
     * that (otherwise effects wouldn't be consistent); instead, we take a
     * certain percentage of the resolution we configured.
     *
     * We could also use the face->bbox, but it is in font units, so it's a
     * pain to work with when building the relationship with the outlines which
     * are in pixels (in 26.6 representation).
     */
    const int px_size = INT_TO_26D6(text->config.pt_size * NGLI_MAX(h_res, v_res) / 72);
    s->spread = TEXT_DISTMAP_SPREAD_PCENT * px_size / 100;
    //LOG(ERROR, "selected spread: %d (pixel size: %d)", s->spread, px_size);

    s->distmap = ngli_distmap_create(text->ctx);
    if (!s->distmap)
        return NGL_ERROR_MEMORY;

    const int chr_px_w = text->config.pt_size * h_res / 72;
    const int chr_px_h = text->config.pt_size * v_res / 72;
    const struct distmap_params params = {
        //.spread  = s->spread,
        .shape_w = chr_px_w,
        .shape_h = chr_px_h,
    };
    int ret = ngli_distmap_init(s->distmap, &params);
    if (ret < 0)
        return ret;

    return 0;
}

struct glyph {
    int shape_id;
    int w, h;
    int bearing_x, bearing_y;
    float uvcoords[8];
};

static void free_glyph(void *user_arg, void *data)
{
    struct glyph *glyph = data;
    ngli_freep(&glyph);
}

static struct glyph *create_glyph(const FT_GlyphSlot slot)
{
    struct glyph *glyph = ngli_calloc(1, sizeof(*glyph));
    if (!glyph)
        return NULL;

    return glyph;
}

struct nvec2 { // normed vec2
    float x, y;
};

static struct nvec2 norm_ftvec2(const FT_BBox *cbox, const FT_Vector *v)
{
    // XXX make scale_[xy] computation only once
    const float scale_x = 1.f / (cbox->xMax - cbox->xMin);
    const float scale_y = 1.f / (cbox->yMax - cbox->yMin);
    const struct nvec2 ret = {
        .x = (v->x - cbox->xMin) * scale_x,
        .y = (v->y - cbox->yMin) * scale_y,
    };
    ngli_assert(ret.x <= 1.0 && ret.x >= 0.0 && ret.y <= 1.0 && ret.y >= 0.0);
    return ret;
}

static int move_to_cb(const FT_Vector *ftvec_to, void *user)
{
    struct text_external *s = user;
    const struct nvec2 norm_to = norm_ftvec2(&s->cbox, ftvec_to);
    const float to[3] = {norm_to.x, norm_to.y, 0.f};
    return ngli_path_move_to(s->path, to);
}

static int line_to_cb(const FT_Vector *ftvec_to, void *user)
{
    struct text_external *s = user;
    const struct nvec2 norm_to = norm_ftvec2(&s->cbox, ftvec_to);
    const float to[3] = {norm_to.x, norm_to.y, 0.f};
    return ngli_path_line_to(s->path, to);
}

static int conic_to_cb(const FT_Vector *ftvec_ctl, const FT_Vector *ftvec_to, void *user)
{
    struct text_external *s = user;
    const struct nvec2 norm_ctl = norm_ftvec2(&s->cbox, ftvec_ctl);
    const struct nvec2 norm_to  = norm_ftvec2(&s->cbox, ftvec_to);
    const float ctl[3] = {norm_ctl.x, norm_ctl.y, 0.f};
    const float to[3]  = {norm_to.x,  norm_to.y,  0.f};
    return ngli_path_bezier2_to(s->path, ctl, to);
}

static int cubic_to_cb(const FT_Vector *ftvec_ctl1, const FT_Vector *ftvec_ctl2,
                       const FT_Vector *ftvec_to, void *user)
{
    struct text_external *s = user;
    const struct nvec2 norm_ctl1 = norm_ftvec2(&s->cbox, ftvec_ctl1);
    const struct nvec2 norm_ctl2 = norm_ftvec2(&s->cbox, ftvec_ctl2);
    const struct nvec2 norm_to  = norm_ftvec2(&s->cbox, ftvec_to);
    const float ctl1[3] = {norm_ctl1.x, norm_ctl1.y, 0.f};
    const float ctl2[3] = {norm_ctl2.x, norm_ctl2.y, 0.f};
    const float to[3]  = {norm_to.x,  norm_to.y,  0.f};
    return ngli_path_bezier3_to(s->path, ctl1, ctl2, to);
}

static const FT_Outline_Funcs outline_funcs = {
    .move_to  = move_to_cb,
    .line_to  = line_to_cb,
    .conic_to = conic_to_cb,
    .cubic_to = cubic_to_cb,
};

static int make_glyph_index(struct text_external *s)
{
    ngli_hmap_freep(&s->glyph_index);
    s->glyph_index = ngli_hmap_create();
    if (!s->glyph_index)
        return NGL_ERROR_MEMORY;
    ngli_hmap_set_free(s->glyph_index, free_glyph, NULL);

    int cur_glyph = 0;
    ngli_path_freep(&s->path);
    s->path = ngli_path_create();
    if (!s->path)
        return NGL_ERROR_MEMORY;

    struct text_line *lines = ngli_darray_data(&s->lines);
    for (int i = 0; i < ngli_darray_count(&s->lines); i++) {
        hb_buffer_t *buffer = lines[i].buffer; // can't be const because of hb_buffer_get_length()
        const unsigned int nb_glyphs = hb_buffer_get_length(buffer);
        const hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buffer, NULL);
        for (int cp = 0; cp < nb_glyphs; cp++) {
            /*
             * Check if glyph is not already registered in the index. We use a
             * unique string identifier.
             *
             * We can't use hb_font_get_glyph_name() since the result is not
             * unique. With some font, it may return an empty string for all
             * the glyph (see ttf-hanazono 20170904 for an example of this).
             *
             * TODO: make a variant of the hmap with an int to save this int->str
             */
            char glyph_name[32];
            const hb_codepoint_t glyph_id = glyph_info[cp].codepoint;
            snprintf(glyph_name, sizeof(glyph_name), "%u", glyph_id);
            if (ngli_hmap_get(s->glyph_index, glyph_name))
                continue;

            /* Rasterize the glyph with FreeType */
            // TODO: error checking?
            // XXX: remove bitmap computations? should we remove the size from
            // the init as well?
            // FT_LOAD_NO_SCALE?
            FT_Load_Glyph(s->ft_face, glyph_id, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
            FT_GlyphSlot slot = s->ft_face->glyph;

            //LOG(ERROR, "width:%ld height:%ld "
            //           "hori[BearingX:%ld BearingY:%ld horiAdvance:%ld] "
            //           "vert[BearingX:%ld BearingY:%ld vertAdvance:%ld]",
            //           slot->metrics.width,
            //           slot->metrics.height,
            //           slot->metrics.horiBearingX,
            //           slot->metrics.horiBearingY,
            //           slot->metrics.horiAdvance,
            //           slot->metrics.vertBearingX,
            //           slot->metrics.vertBearingY,
            //           slot->metrics.vertAdvance
            //);

            FT_Outline_Get_CBox(&slot->outline, &s->cbox);
            //LOG(ERROR, "glyph cbox xmin:%ld xmax:%ld ymin:%ld ymax:%ld",
            //    s->cbox.xMin, s->cbox.xMax, s->cbox.yMin, s->cbox.yMax);

            /*
             * Enlarge the control box by spread so that the glyph distance map
             * is drawn more compressed within its space during the decompose
             * operation.
             */
            // XXX need to be in the font unit!
            s->cbox.xMin -= s->spread;
            s->cbox.xMax += s->spread;
            s->cbox.yMin -= s->spread;
            s->cbox.yMax += s->spread;

            //LOG(ERROR, "glyph with spread cbox xmin:%ld xmax:%ld ymin:%ld ymax:%ld",
            //    s->cbox.xMin, s->cbox.xMax, s->cbox.yMin, s->cbox.yMax);

            ngli_path_clear(s->path);

            FT_Outline_Decompose(&slot->outline, &outline_funcs, s);

            int ret = ngli_path_add_to_distmap(s->path, s->distmap, cur_glyph);
            if (ret < 0)
                return ret;

            /* Save the rasterized glyph in the index */
            struct glyph *glyph = create_glyph(slot);
            if (!glyph)
                return NGL_ERROR_MEMORY;

            glyph->w = s->cbox.xMax - s->cbox.xMin;
            glyph->h = s->cbox.yMax - s->cbox.yMin;
            glyph->bearing_x = s->cbox.xMin + s->spread;
            glyph->bearing_y = s->cbox.yMin + s->spread;
            glyph->shape_id = cur_glyph++;

            ret = ngli_hmap_set(s->glyph_index, glyph_name, glyph);
            if (ret < 0) {
                free_glyph(NULL, glyph);
                return NGL_ERROR_MEMORY;
            }
        }
    }

    ngli_path_freep(&s->path);

    return 0;
}

static void free_lines(struct darray *lines_array)
{
    struct text_line *lines = ngli_darray_data(lines_array);
    for (int i = 0; i < ngli_darray_count(lines_array); i++)
        hb_buffer_destroy(lines[i].buffer);
    ngli_darray_reset(lines_array);
}

/*
 * Split text into lines, where each line is a harfbuzz buffer
 *
 * TODO: need another layer of segmentation: sub-segments per line to handle
 * bidirectional
 */
static int split_text(struct text *text, const char *str)
{
    struct text_external *s = text->priv_data;

    free_lines(&s->lines); // make it re-entrant (for live-update of the text)
    ngli_darray_init(&s->lines, sizeof(struct text_line), 0);

    const size_t len = strlen(str);
    size_t start = 0;

    for (size_t i = 0; i <= len; i++) {
        if (i != len && str[i] != '\n')
            continue;

        hb_buffer_t *buffer = hb_buffer_create();
        if (!hb_buffer_allocation_successful(buffer))
            return NGL_ERROR_MEMORY;

        struct text_line line = {.buffer = buffer, .start=start};

        /*
         * Shape segment
         */
        const size_t segment_len = i - start;
        //LOG(ERROR, "add segment:[%.*s]", (int)segment_len, &str[start]);
        hb_buffer_add_utf8(buffer, &str[start], segment_len, 0, segment_len);

        // TODO: need to probe rtl/ltr script
        if (text->config.wmode == NGLI_TEXT_WRITING_MODE_VERTICAL_LR ||
            text->config.wmode == NGLI_TEXT_WRITING_MODE_VERTICAL_RL) {
            hb_buffer_set_direction(buffer, HB_DIRECTION_TTB);
        } else if (text->config.wmode == NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB) {
            hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
        }

        //XXX: expose top user?
        //hb_buffer_set_script(buffer, script);
        //hb_buffer_set_language(buffer, language);

        hb_buffer_guess_segment_properties(buffer); // this is guessing direction/script/language
        hb_shape(s->hb_font, buffer, NULL, 0);
        start = i + 1;

        if (!ngli_darray_push(&s->lines, &line)) {
            hb_buffer_reset(buffer);
            return NGL_ERROR_MEMORY;
        }
    }

    return 0;
}

/* XXX duplicated */
// TODO: some languages don't use space as word separator; the word
// tokenization needs something more advanced
static enum char_tag get_char_tags(char c)
{
    if (c == ' ')
        return NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR;
    if (c == '\n')
        return NGLI_TEXT_CHAR_TAG_LINE_BREAK | NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR;
    return NGLI_TEXT_CHAR_TAG_GLYPH;
}

static int text_external_set_string(struct text *text, const char *str)
{
    struct text_external *s = text->priv_data;

    int ret = split_text(text, str);
    if (ret < 0)
        return ret;

    ret = make_glyph_index(s);
    if (ret < 0)
        return ret;

    ret = ngli_distmap_generate_texture(s->distmap);
    if (ret < 0)
        return ret;

    text->texture = ngli_distmap_get_texture(s->distmap);

    struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(s->glyph_index, entry))) {
        struct glyph *glyph = entry->data;
        ngli_distmap_get_shape_coords(s->distmap, glyph->shape_id, glyph->uvcoords);
    }

    int x_min = INT_MAX, y_min = INT_MAX;
    int x_max = INT_MIN, y_max = INT_MIN;

    // XXX
    hb_position_t x_cur = 0; //s->config.padding;
    hb_position_t y_cur = 0; //s->config.padding;

    const int line_advance = s->ft_face->size->metrics.height;
    int line_max_size = INT_MIN;

    struct text_line *lines = ngli_darray_data(&s->lines);
    for (int i = 0; i < ngli_darray_count(&s->lines); i++) {
        hb_buffer_t *buffer = lines[i].buffer; // can't be const because of hb_buffer_get_length()

        const unsigned int len = hb_buffer_get_length(buffer);
        const hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buffer, NULL);
        const hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buffer, NULL);

        int line_xmin = INT_MAX, line_xmax = INT_MIN;

        for (int cp = 0; cp < len; cp++) {
            char glyph_name[32];
            const hb_codepoint_t glyph_id = glyph_info[cp].codepoint;
            snprintf(glyph_name, sizeof(glyph_name), "%u", glyph_id);
            const struct glyph *glyph = ngli_hmap_get(s->glyph_index, glyph_name);
            if (!glyph)
                continue;

            //LOG(ERROR, "glyph w:%d h:%d", glyph->w, glyph->h);

            const hb_glyph_position_t *pos = &glyph_pos[cp];

            const int x_adv = pos->x_advance;
            const int y_adv = pos->y_advance;
            const int x_off = pos->x_offset;
            const int y_off = pos->y_offset;

            //LOG(ERROR, "bearing[x:%d y:%d] wh[%d,%d]",
            //    glyph->bearing_x, glyph->bearing_y, glyph->w, glyph->h);

            const int chr_pos = lines[i].start + glyph_info[cp].cluster;
            struct char_info chr = {
                .x = x_cur + glyph->bearing_x + x_off,
                .y = y_cur + glyph->bearing_y + y_off,
                .w = glyph->w,
                .h = glyph->h,
                .tags = get_char_tags(str[chr_pos]),
                .line = i,
            };
            //LOG(ERROR, "%u -> %d", glyph_id, chr.tags);
            memcpy(chr.atlas_uvcoords, glyph->uvcoords, sizeof(chr.atlas_uvcoords));

            x_min = NGLI_MIN(x_min, chr.x + s->spread);
            y_min = NGLI_MIN(y_min, chr.y + s->spread);
            x_max = NGLI_MAX(x_max, chr.x + chr.w - s->spread);
            y_max = NGLI_MAX(y_max, chr.y + chr.h - s->spread);

            line_xmin = NGLI_MIN(line_xmin, chr.x + s->spread);
            line_xmax = NGLI_MAX(line_xmax, chr.x + chr.w - s->spread);

            // XXX: move all the stuff inside that if?
            if (glyph->w > 0 && glyph->h > 0) {
                if (!ngli_darray_push(&text->chars, &chr))
                    return NGL_ERROR_MEMORY;
            }

            x_cur += x_adv;
            y_cur += y_adv;
        }

        lines[i].size = line_xmax - line_xmin;
        line_max_size = NGLI_MAX(line_max_size, lines[i].size);

        /* Jump to next line or column */
        if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(buffer))) {
            x_cur = text->config.padding*64;
            y_cur -= line_advance;
        } else {
            y_cur = text->config.padding*64;
            x_cur -= line_advance; // is this OK?
        }

        /* Insert line break information */
        if (i != ngli_darray_count(&s->lines)) {
            const struct char_info chr = {
                .tags = NGLI_TEXT_CHAR_TAG_LINE_BREAK | NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR,
            };
            if (!ngli_darray_push(&text->chars, &chr))
                return NGL_ERROR_MEMORY;
        }

    }

    //LOG(ERROR, "xmin=%d ymin=%d", x_min, y_min);
    //LOG(ERROR, "xmax=%d ymax=%d", x_max, y_max);

    text->width  = x_max - x_min + text->config.padding*64;
    text->height = y_max - y_min + text->config.padding*64;
    //LOG(ERROR, "text size: %dx%d", s->width, s->height);

    struct char_info *chars = ngli_darray_data(&text->chars);
    const int nb_chars = ngli_darray_count(&text->chars);
    for (int i = 0; i < nb_chars; i++) {
        struct char_info *chr = &chars[i];
        chr->x -= x_min;
        chr->y -= y_min;

        // XXX: vertical text etc?
        // XXX: other aligns
        if (text->config.halign == NGLI_TEXT_HALIGN_CENTER) {
            const int space = line_max_size - lines[chr->line].size;
            chr->x += space / 2;
        }
    }

    return 0;
}

static void text_external_reset(struct text *text)
{
    struct text_external *s = text->priv_data;

    ngli_path_freep(&s->path);
    free_lines(&s->lines);
    hb_font_destroy(s->hb_font);
    FT_Done_Face(s->ft_face);
    FT_Done_FreeType(s->ft_library);
    ngli_hmap_freep(&s->glyph_index);
    ngli_distmap_freep(&s->distmap);
}

const struct text_cls ngli_text_external = {
    .priv_size = sizeof(struct text_external),
    .init       = text_external_init,
    .set_string = text_external_set_string,
    .reset      = text_external_reset,
};

#else

#include "log.h"
#include "nodegl.h"
#include "text.h"

static int text_external_dummy_set_string(struct text *s, const char *str)
{
    return NGL_ERROR_BUG;
}

static int text_external_dummy_init(struct text *s)
{
    LOG(ERROR, "node.gl is not compiled with text libraries support");
    return NGL_ERROR_UNSUPPORTED;
}

const struct text_cls ngli_text_external = {
    .init       = text_external_dummy_init,
    .set_string = text_external_dummy_set_string,
};
#endif
