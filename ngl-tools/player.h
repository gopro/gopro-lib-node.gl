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

#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

#include <SDL.h>
#include <nodegl.h>

struct player {

    SDL_Window *window;

    double duration_f;
    int64_t duration;
    int aspect[2];

    struct ngl_ctx *ngl;
    struct ngl_config ngl_config;
    int64_t clock_off;
    int64_t frame_ts;
    int paused;
    int64_t lasthover;
    int mouse_down;
    int fullscreen;
    int text_last_frame_ts;
    int text_last_duration;
    struct ngl_node *pgbar_opacity_node;
    struct ngl_node *pgbar_text_node;
    struct ngl_node *pgbar_duration_node;
};

int player_init(struct player *p, const char *win_title, struct ngl_node *scene,
                const struct ngl_config *cfg, double duration, int enable_ui);

void player_uninit(void);

void player_main_loop(void);

#endif
