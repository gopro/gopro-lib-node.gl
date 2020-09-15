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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"
#include "player.h"
#include "wsi.h"

static struct player *g_player;

#ifndef O_BINARY
#define O_BINARY 0
#endif

static int save_ppm(const char *filename, uint8_t *data, int width, int height)
{
    int ret = 0;
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd == -1) {
        fprintf(stderr, "Unable to open '%s'\n", filename);
        return -1;
    }

    uint8_t *buf = malloc(32 + width * height * 3);
    if (!buf) {
        ret = -1;
        goto end;
    }

    const int header_size = snprintf((char *)buf, 32, "P6 %d %d 255\n", width, height);
    if (header_size < 0) {
        ret = -1;
        fprintf(stderr, "Failed to write PPM header\n");
        goto end;
    }

    uint8_t *dst = buf + header_size;
    for (int i = 0; i < width * height; i++) {
        memcpy(dst, data, 3);
        dst += 3;
        data += 4;
    }

    const int size = header_size + width * height * 3;
    ret = write(fd, buf, size);
    if (ret != size) {
        fprintf(stderr, "Failed to write PPM data\n");
        goto end;
    }

end:
    free(buf);
    close(fd);
    return ret;
}

static int screenshot(void)
{
    struct player *p = g_player;
    struct ngl_config *config = &p->ngl_config;
    struct ngl_config backup = *config;

    uint8_t *capture_buffer = calloc(config->width * config->height, 4);
    if (!capture_buffer)
        return -1;

    config->offscreen = 1;
    config->width = config->viewport[2];
    config->height = config->viewport[3];
    memset(config->viewport, 0, sizeof(config->viewport));
    config->capture_buffer = capture_buffer;

    int ret = ngl_configure(p->ngl, config);
    if (ret < 0) {
        fprintf(stderr, "Could not configure node.gl for offscreen capture\n");
        goto end;
    }
    ngl_draw(p->ngl, p->frame_ts / 1000000.0);

    char filename[32];
    snprintf(filename, sizeof(filename), "ngl-%" PRId64 ".ppm", gettime());
    fprintf(stdout, "Screenshot saved to '%s'\n", filename);
    ret = save_ppm(filename, capture_buffer, config->width, config->height);
    if (ret < 0) {
        fprintf(stderr, "Could not save screenshot to '%s'", filename);
    }

end:
    *config = backup;
    ret = ngl_configure(p->ngl, config);
    if (ret < 0)
        fprintf(stderr, "Could not configure node.gl for onscreen rendering\n");
    p->clock_off = gettime_relative() - p->frame_ts;

    free(capture_buffer);
    return ret;
}

static void kill_scene()
{
    struct player *p = g_player;

    ngl_set_scene(p->ngl, NULL);
    p->pgbar_opacity_node  = NULL;
    p->pgbar_duration_node = NULL;
    p->pgbar_text_node     = NULL;
}

static void update_text(void)
{
    struct player *p = g_player;

    if (!p->pgbar_text_node)
        return;

    const int frame_ts = p->frame_ts / 1000000;
    const int duration = p->duration / 1000000;
    if (frame_ts == p->text_last_frame_ts && duration == p->text_last_duration)
        return;

    char buf[128];
    const int cm = frame_ts / 60;
    const int cs = frame_ts % 60;
    const int dm = duration / 60;
    const int ds = duration % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d", cm, cs, dm, ds);
    ngl_node_param_set(p->pgbar_text_node, "text", buf);
    p->text_last_frame_ts = frame_ts;
    p->text_last_duration = duration;
}

static void update_time(int64_t seek_at)
{
    struct player *p = g_player;

    if (seek_at >= 0) {
        p->clock_off = gettime_relative() - seek_at;
        p->frame_ts = seek_at;
        return;
    }

    if (!p->paused && !p->mouse_down) {
        const int64_t now = gettime_relative();
        if (p->clock_off < 0 || now - p->clock_off > p->duration)
            p->clock_off = now;

        p->frame_ts = now - p->clock_off;
    }

    if (p->pgbar_opacity_node && p->lasthover >= 0) {
        const int64_t t64_diff = gettime_relative() - p->lasthover;
        const double opacity = clipd(1.5 - t64_diff / 1000000.0, 0, 1);
        ngl_node_param_set(p->pgbar_opacity_node, "value", opacity);

        const float text_bg[4] = {1.0, 1.0, 1.0, opacity};
        const float text_fg[4] = {0.0, 0.0, 0.0, opacity};
        ngl_node_param_set(p->pgbar_text_node, "bg_color", text_bg);
        ngl_node_param_set(p->pgbar_text_node, "fg_color", text_fg);

        update_text();
    }
}

static int key_callback(SDL_Window *window, SDL_KeyboardEvent *event)
{
    struct player *p = g_player;

    const SDL_Keycode key = event->keysym.sym;
    switch (key) {
    case SDLK_ESCAPE:
    case SDLK_q:
        return 1;
    case SDLK_SPACE:
        p->paused ^= 1;
        p->clock_off = gettime_relative() - p->frame_ts;
        break;
    case SDLK_f:
        p->fullscreen ^= 1;
        SDL_SetWindowFullscreen(window, p->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        break;
    case SDLK_s:
        screenshot();
        break;
    case SDLK_k:
        kill_scene();
        break;
    case SDLK_LEFT:
        update_time(clipi64(p->frame_ts - 10 * 1000000, 0, p->duration));
        break;
    case SDLK_RIGHT:
        update_time(clipi64(p->frame_ts + 10 * 1000000, 0, p->duration));
        break;
    default:
        break;
    }

    return 0;
}

static void size_callback(SDL_Window *window, int width, int height)
{
    struct player *p = g_player;

    get_viewport(width, height, p->aspect, p->ngl_config.viewport);
    p->ngl_config.width = width;
    p->ngl_config.height = height;
    ngl_resize(p->ngl, width, height, p->ngl_config.viewport);
}

static void seek_event(int x)
{
    struct player *p = g_player;
    const int *vp = p->ngl_config.viewport;
    const int pos = clipi(x - vp[0], 0, vp[2]) * 3/2;
    const int64_t seek_at64 = p->duration * pos / vp[2];
    p->lasthover = gettime_relative();
    update_time(seek_at64);
}

static void mouse_buttondown_callback(SDL_Window *window, SDL_MouseButtonEvent *event)
{
    struct player *p = g_player;
    p->mouse_down = 1;
    seek_event(event->x);
}

static void mouse_buttonup_callback(SDL_Window *window, SDL_MouseButtonEvent *event)
{
    struct player *p = g_player;
    p->mouse_down = 0;
    p->clock_off = gettime_relative() - p->frame_ts;
}

static void mouse_pos_callback(SDL_Window *window, SDL_MouseMotionEvent *event)
{
    struct player *p = g_player;
    p->lasthover = gettime_relative();
    if (p->mouse_down)
        seek_event(event->x);
}

static const char *pgbar_vert =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;"    "\n"
    "    coord = ngl_uvcoord;"                                                          "\n"
    "}";

static const char *pgbar_frag =
    "void main()"                                                                       "\n"
    "{"                                                                                 "\n"
    "    float stime = time / duration;"                                                "\n"
    "    float alpha = opacity * (coord.x < stime ? 1.0 : 0.3);"                        "\n"
    "    ngl_out_color = vec4(1.0, 1.0, 1.0, alpha);"                                   "\n"
    "}";

static struct ngl_node *add_progress_bar(struct ngl_node *scene)
{
    struct player *p = g_player;

    static const float bar_corner[3] = {-1.0, -1.0, 0.0};
    static const float bar_width[3]  = { 2.0 * 2/3.,  0.0, 0.0};
    static const float bar_height[3] = { 0.0,  2.0 * 0.05, 0.0}; // 5% of the height

    static const float text_corner[3] = {1.0 - 2.0 * 1/3., -1.0, 0.0};
    static const float text_width[3]  = {2.0 * 1/3.,  0.0, 0.0};
    static const float text_height[3] = {0.0,  2.0 * 0.05, 0.0};
    static const float text_bg[4]     = {1.0, 1.0, 1.0, 0.0};
    static const float text_fg[4]     = {0.0, 0.0, 0.0, 0.0};

    struct ngl_node *text       = ngl_node_create(NGL_NODE_TEXT);
    struct ngl_node *quad       = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program    = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *render     = ngl_node_create(NGL_NODE_RENDER);
    struct ngl_node *time       = ngl_node_create(NGL_NODE_TIME);
    struct ngl_node *v_duration = ngl_node_create(NGL_NODE_UNIFORMFLOAT);
    struct ngl_node *v_opacity  = ngl_node_create(NGL_NODE_UNIFORMFLOAT);
    struct ngl_node *coord      = ngl_node_create(NGL_NODE_IOVEC2);
    struct ngl_node *group      = ngl_node_create(NGL_NODE_GROUP);
    struct ngl_node *gcfg       = ngl_node_create(NGL_NODE_GRAPHICCONFIG);

    if (!text || !quad || !program || !render || !time || !v_duration || !v_opacity ||
        !coord || !group || !gcfg) {
        ngl_node_unrefp(&gcfg);
        goto end;
    }

    struct ngl_node *children[] = {scene, render, text};

    ngl_node_param_set(quad, "corner", bar_corner);
    ngl_node_param_set(quad, "width",  bar_width);
    ngl_node_param_set(quad, "height", bar_height);

    ngl_node_param_set(program, "vertex",   pgbar_vert);
    ngl_node_param_set(program, "fragment", pgbar_frag);
    ngl_node_param_set(program, "vert_out_vars", "coord", coord);

    ngl_node_param_set(v_duration, "value", p->duration_f);
    ngl_node_param_set(v_opacity,  "value", 0.0);

    ngl_node_param_set(render, "geometry", quad);
    ngl_node_param_set(render, "program", program);
    ngl_node_param_set(render, "frag_resources", "time",     time);
    ngl_node_param_set(render, "frag_resources", "duration", v_duration);
    ngl_node_param_set(render, "frag_resources", "opacity",  v_opacity);

    ngl_node_param_add(group, "children", ARRAY_NB(children), children);

    ngl_node_param_set(gcfg, "child", group);
    ngl_node_param_set(gcfg, "blend", 1);
    ngl_node_param_set(gcfg, "blend_src_factor",   "src_alpha");
    ngl_node_param_set(gcfg, "blend_dst_factor",   "one_minus_src_alpha");
    ngl_node_param_set(gcfg, "blend_src_factor_a", "zero");
    ngl_node_param_set(gcfg, "blend_dst_factor_a", "one");

    ngl_node_param_set(text, "box_corner", text_corner);
    ngl_node_param_set(text, "box_width", text_width);
    ngl_node_param_set(text, "box_height", text_height);
    ngl_node_param_set(text, "bg_color", text_bg);
    ngl_node_param_set(text, "fg_color", text_fg);
    ngl_node_param_set(text, "aspect_ratio", p->aspect[0], p->aspect[1]);

    p->pgbar_opacity_node  = v_opacity;
    p->pgbar_duration_node = v_duration;
    p->pgbar_text_node     = text;

end:
    ngl_node_unrefp(&quad);
    ngl_node_unrefp(&program);
    ngl_node_unrefp(&render);
    ngl_node_unrefp(&time);
    ngl_node_unrefp(&v_duration);
    ngl_node_unrefp(&v_opacity);
    ngl_node_unrefp(&coord);
    ngl_node_unrefp(&group);

    return gcfg;
}

static int set_scene(struct ngl_node *scene)
{
    int ret;
    struct player *p = g_player;

    if (p->enable_ui) {
        scene = add_progress_bar(scene);
        if (!scene)
            return NGL_ERROR_MEMORY;
        ret = ngl_set_scene(p->ngl, scene);
        ngl_node_unrefp(&scene);
    } else {
        ret = ngl_set_scene(p->ngl, scene);
    }
    if (ret < 0) {
        p->pgbar_opacity_node  = NULL;
        p->pgbar_duration_node = NULL;
        p->pgbar_text_node     = NULL;
    }
    return ret;
}

int player_init(struct player *p, const char *win_title, struct ngl_node *scene,
                const struct ngl_config *cfg, double duration, int enable_ui)
{
    memset(p, 0, sizeof(*p));

    g_player = p;

    if (init_window() < 0)
        return -1;

    p->window = get_window(win_title, cfg->width, cfg->height);
    if (!p->window) {
        SDL_Quit();
        return -1;
    }

    p->clock_off = -1;
    p->lasthover = -1;
    p->duration_f = duration;
    p->duration = duration * 1000000;
    p->enable_ui = enable_ui;

    p->ngl_config = *cfg;

    p->aspect[0] = cfg->viewport[2];
    p->aspect[1] = cfg->viewport[3];
    if (!p->aspect[0] || !p->aspect[1]) {
        p->aspect[0] = cfg->width;
        p->aspect[1] = cfg->height;

        p->ngl_config.viewport[0] = 0;
        p->ngl_config.viewport[1] = 0;
        p->ngl_config.viewport[2] = cfg->width;
        p->ngl_config.viewport[3] = cfg->height;
    }

    int ret = wsi_set_ngl_config(&p->ngl_config, p->window);
    if (ret < 0)
        return ret;

    p->ngl = ngl_create();
    if (!p->ngl)
        return -1;

    ret = ngl_configure(p->ngl, &p->ngl_config);
    if (ret < 0)
        return ret;

    return set_scene(scene);
}

void player_uninit(void)
{
    struct player *p = g_player;

    if (!p)
        return;

    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_USEREVENT)
            free(event.user.data1);

    ngl_freep(&p->ngl);
    SDL_DestroyWindow(p->window);
    SDL_Quit();
}

static int handle_scene(const void *data)
{
    struct ngl_node *scene = ngl_node_deserialize(data);
    if (!scene)
        return NGL_ERROR_INVALID_DATA;
    int ret = set_scene(scene);
    ngl_node_unrefp(&scene);
    return ret;
}

static int handle_duration(const void *data)
{
    struct player *p = g_player;
    memcpy(&p->duration_f, data, sizeof(p->duration_f));
    p->duration = p->duration_f * 1000000;
    if (p->pgbar_duration_node)
        ngl_node_param_set(p->pgbar_duration_node, "value", p->duration_f);
    update_text();
    return 0;
}

static int handle_clearcolor(const void *data)
{
    struct player *p = g_player;
    memcpy(p->ngl_config.clear_color, data, sizeof(p->ngl_config.clear_color));
    return 0;
}

static int handle_samples(const void *data)
{
    struct player *p = g_player;
    memcpy(&p->ngl_config.samples, data, sizeof(p->ngl_config.samples));
    return 0;
}

static int handle_aspect_ratio(const void *data)
{
    struct player *p = g_player;
    memcpy(p->aspect, data, sizeof(p->aspect));
    if (!p->aspect[0] || !p->aspect[1])
        p->aspect[0] = p->aspect[1] = 1;
    int width, height;
    SDL_GetWindowSize(p->window, &width, &height);
    size_callback(p->window, width, height);
    if (p->pgbar_text_node)
        ngl_node_param_set(p->pgbar_text_node, "aspect_ratio", p->aspect[0], p->aspect[1]);
    return 0;
}

static int handle_framerate(const void *data)
{
    const int *rate = data;
    fprintf(stderr, "WARNING: unhandled framerate %d/%d\n", rate[0], rate[1]);
    return 0;
}

static int handle_reconfigure(const void *data)
{
    struct player *p = g_player;
    return ngl_configure(p->ngl, &p->ngl_config);
}

typedef int (*handle_func)(const void *data);

static const handle_func handle_map[] = {
    [PLAYER_SIGNAL_SCENE]        = handle_scene,
    [PLAYER_SIGNAL_DURATION]     = handle_duration,
    [PLAYER_SIGNAL_ASPECT_RATIO] = handle_aspect_ratio,
    [PLAYER_SIGNAL_FRAMERATE]    = handle_framerate,
    [PLAYER_SIGNAL_CLEARCOLOR]   = handle_clearcolor,
    [PLAYER_SIGNAL_SAMPLES]      = handle_samples,
    [PLAYER_SIGNAL_RECONFIGURE]  = handle_reconfigure,
};

void player_main_loop(void)
{
    struct player *p = g_player;

    int run = 1;
    while (run) {
        update_time(-1);
        ngl_draw(p->ngl, p->frame_ts / 1000000.0);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    run = 0;
                else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    size_callback(p->window, event.window.data1, event.window.data2);
                break;
            case SDL_KEYDOWN:
                run = key_callback(p->window, &event.key) == 0;
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_buttondown_callback(p->window, &event.button);
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_buttonup_callback(p->window, &event.button);
                break;
            case SDL_MOUSEMOTION:
                mouse_pos_callback(p->window, &event.motion);
                break;
            case SDL_USEREVENT:
                run = handle_map[event.user.code](event.user.data1) == 0;
                free(event.user.data1);
                break;
            }
        }
    }
}
