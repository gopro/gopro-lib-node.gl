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
#include <stdlib.h>
#if defined(_WIN32) && defined(_MSC_VER)
#define STDOUT_FILENO _fileno(stdout)
#define STDERR_FILENO _fileno(stderr)
#endif

#include <nodegl.h>
#include <sxplayer.h>

#include "common.h"
#include "opts.h"
#include "player.h"

struct ctx {
    /* options */
    int log_level;
    struct ngl_config cfg;
    int direct_rendering;
    int player_ui;
    int framerate[2];

    struct sxplayer_info media_info;
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-l", "--loglevel",         OPT_TYPE_LOGLEVEL, .offset=OFFSET(log_level)},
    {"-b", "--backend",          OPT_TYPE_BACKEND,  .offset=OFFSET(cfg.backend)},
    {"-d", "--direct_rendering", OPT_TYPE_INT,      .offset=OFFSET(direct_rendering)},
    {"-z", "--swap_interval",    OPT_TYPE_INT,      .offset=OFFSET(cfg.swap_interval)},
    {"-c", "--clear_color",      OPT_TYPE_COLOR,    .offset=OFFSET(cfg.clear_color)},
    {"-m", "--samples",          OPT_TYPE_INT,      .offset=OFFSET(cfg.samples)},
    {"-u", "--disable-ui",       OPT_TYPE_TOGGLE,   .offset=OFFSET(player_ui)},
    {"-r", "--framerate",        OPT_TYPE_RATIONAL, .offset=OFFSET(framerate)},
};

static const char *media_vertex =
"void main()"                                                                       "\n"
"{"                                                                                 "\n"
"    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;"    "\n"
"    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;"        "\n"
"}";

static const char *media_fragment =
"void main()"                                                           "\n"
"{"                                                                     "\n"
"    ngl_out_color = ngl_texvideo(tex0, var_tex0_coord);"               "\n"
"}";

static struct ngl_node *get_scene(const char *filename, int direct_rendering)
{
    static const float corner[3] = {-1.0, -1.0, 0.0};
    static const float width[3]  = { 2.0,  0.0, 0.0};
    static const float height[3] = { 0.0,  2.0, 0.0};

    struct ngl_node *media          = ngl_node_create(NGL_NODE_MEDIA);
    struct ngl_node *texture        = ngl_node_create(NGL_NODE_TEXTURE2D);
    struct ngl_node *quad           = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program        = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *render         = ngl_node_create(NGL_NODE_RENDER);
    struct ngl_node *var_tex0_coord = ngl_node_create(NGL_NODE_IOVEC2);

    ngl_node_param_set(media, "filename", filename);

    ngl_node_param_set(quad, "corner", corner);
    ngl_node_param_set(quad, "width", width);
    ngl_node_param_set(quad, "height", height);

    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_set(texture, "min_filter", "linear");
    ngl_node_param_set(texture, "mag_filter", "linear");
    if (direct_rendering != -1)
        ngl_node_param_set(texture, "direct_rendering", direct_rendering);

    ngl_node_param_set(program, "vertex",   media_vertex);
    ngl_node_param_set(program, "fragment", media_fragment);
    ngl_node_param_set(program, "vert_out_vars", "var_tex0_coord", var_tex0_coord);

    ngl_node_param_set(render, "geometry", quad);
    ngl_node_param_set(render, "program", program);
    ngl_node_param_set(render, "frag_resources", "tex0", texture);

    ngl_node_unrefp(&var_tex0_coord);

    ngl_node_unrefp(&program);
    ngl_node_unrefp(&media);
    ngl_node_unrefp(&texture);
    ngl_node_unrefp(&quad);

    return render;
}

static int probe(const char *filename, struct sxplayer_info *media_info)
{
    struct sxplayer_ctx *ctx = sxplayer_create(filename);
    if (!ctx)
        return -1;

    int ret = sxplayer_get_info(ctx, media_info);
    if (ret < 0)
        return ret;

    sxplayer_free(&ctx);

    return 0;
}

int main(int argc, char *argv[])
{
    struct ctx s = {
        .log_level          = NGL_LOG_INFO,
        .direct_rendering   = -1,
        .cfg.swap_interval  = -1,
        .cfg.clear_color[3] = 1.f,
        .player_ui          = 1,
        .framerate[0]       = 60,
        .framerate[1]       = 1,
    };

    int ret = opts_parse(argc, argc - 1, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP || argc < 2) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), " <media>");
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    ngl_log_set_min_level(s.log_level);

    const char *filename = argv[argc - 1];
    ret = probe(filename, &s.media_info);
    if (ret < 0)
        return ret;

    struct ngl_node *scene = get_scene(filename, s.direct_rendering);
    if (!scene)
        return -1;

    struct player p;
    s.cfg.width  = s.media_info.width;
    s.cfg.height = s.media_info.height;
    ret = player_init(&p, "ngl-player", scene, &s.cfg, s.media_info.duration, s.framerate, s.player_ui);
    if (ret < 0)
        goto end;
    ngl_node_unrefp(&scene);

    player_main_loop();

end:
    player_uninit();

    return ret;
}
