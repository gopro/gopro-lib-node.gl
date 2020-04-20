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

#include <stdio.h>

#include <nodegl.h>
#include <sxplayer.h>

#include "common.h"
#include "player.h"

struct sxplayer_info g_info;
struct ngl_node *g_opacity_uniform;

static const char *pgbar_shader = \
"#version 100"                                                          "\n" \
"precision mediump float;"                                              "\n" \
                                                                        "\n" \
"uniform float ar;"                                                     "\n" \
"uniform float opacity;"                                                "\n" \
"uniform sampler2D tex0_sampler;"                                       "\n" \
"uniform float tex0_ts;"                                                "\n" \
"uniform float media_duration;"                                         "\n" \
"varying vec2 var_tex0_coord;"                                          "\n" \
                                                                        "\n" \
"void main()"                                                           "\n" \
"{"                                                                     "\n" \
"    float height = 2.0 / 100. * ar;"                                   "\n" \
"    float x = var_tex0_coord.x;"                                       "\n" \
"    float y = var_tex0_coord.y;"                                       "\n" \
"    vec4 video_pix = texture2D(tex0_sampler, var_tex0_coord);"         "\n" \
"    vec4 color = video_pix;"                                           "\n" \
"    float time = tex0_ts / media_duration;"                            "\n" \
"    if (y > 1. - height)"                                              "\n" \
"        color = x < time ? vec4(1.) : mix(video_pix, vec4(1.), 0.3);"  "\n" \
"    gl_FragColor = mix(video_pix, color, opacity);"                    "\n" \
"}"                                                                     "\n";

static struct ngl_node *get_scene(const char *filename)
{
    static const float corner[3] = {-1.0, -1.0, 0.0};
    static const float width[3]  = { 2.0,  0.0, 0.0};
    static const float height[3] = { 0.0,  2.0, 0.0};

    struct ngl_node *media   = ngl_node_create(NGL_NODE_MEDIA, filename);
    struct ngl_node *texture = ngl_node_create(NGL_NODE_TEXTURE2D);
    struct ngl_node *quad    = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *render  = ngl_node_create(NGL_NODE_RENDER, quad);

    struct ngl_node *u_media_duration = ngl_node_create(NGL_NODE_UNIFORMFLOAT);
    struct ngl_node *u_ar             = ngl_node_create(NGL_NODE_UNIFORMFLOAT);
    struct ngl_node *u_opacity        = ngl_node_create(NGL_NODE_UNIFORMFLOAT);

    ngl_node_param_set(quad, "corner", corner);
    ngl_node_param_set(quad, "width", width);
    ngl_node_param_set(quad, "height", height);

    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_set(program, "fragment", pgbar_shader);
    ngl_node_param_set(u_media_duration, "value", g_info.duration);
    ngl_node_param_set(u_ar,             "value", g_info.width / (double)g_info.height);
    ngl_node_param_set(u_opacity,        "value", 0.0);

    ngl_node_param_set(render, "program", program);
    ngl_node_param_set(render, "textures", "tex0",    texture);
    ngl_node_param_set(render, "uniforms", "media_duration", u_media_duration);
    ngl_node_param_set(render, "uniforms", "ar",             u_ar);
    ngl_node_param_set(render, "uniforms", "opacity",        u_opacity);

    g_opacity_uniform = u_opacity;

    ngl_node_unrefp(&u_media_duration);
    ngl_node_unrefp(&u_ar);
    ngl_node_unrefp(&u_opacity);

    ngl_node_unrefp(&program);
    ngl_node_unrefp(&media);
    ngl_node_unrefp(&texture);
    ngl_node_unrefp(&quad);

    return render;
}

static int probe(const char *filename)
{
    struct sxplayer_ctx *ctx = sxplayer_create(filename);
    if (!ctx)
        return -1;

    int ret = sxplayer_get_info(ctx, &g_info);
    if (ret < 0)
        return ret;

    sxplayer_free(&ctx);

    return 0;
}

static void tick_callback(struct player *p)
{
    if (p->lasthover >= 0) {
        const int64_t t64_diff = gettime() - p->lasthover;
        const double opacity = clipd(1.5 - t64_diff / 1000000.0, 0, 1);
        ngl_node_param_set(g_opacity_uniform, "value", opacity);
    }
}

int main(int argc, char *argv[])
{
    int ret;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <media>\n", argv[0]);
        return -1;
    }

    ret = probe(argv[1]);
    if (ret < 0)
        return ret;

    struct ngl_node *scene = get_scene(argv[1]);
    if (!scene)
        return -1;

    struct player p;
    ret = player_init(&p, "ngl-player", scene,
                      g_info.width, g_info.height, g_info.duration);
    if (ret < 0)
        goto end;
    ngl_node_unrefp(&scene);
    p.tick_callback = tick_callback;

    player_main_loop();

end:
    player_uninit();

    return ret;
}
