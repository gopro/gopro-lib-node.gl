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
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <glcontext.h>
#include <nodegl.h>

#include <GLFW/glfw3.h>


#ifdef __APPLE__
#error "OSX and IOS are not supported for now (but will be)"
#endif

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360
#define WINDOW_ASPECT_RATIO (WINDOW_WIDTH / (double)WINDOW_HEIGHT)

#define FRAMERATE 60

// Please ingore this change, experimenting with the CLA tool

int64_t g_tick;

struct ngl_ctx *g_ctx;

static int64_t gettime()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
}

#if 0
static struct ngl_node *get_simple_scene(const char *filename)
{
    struct ngl_node *group = ngl_node_create(NGL_NODE_GROUP);

    static const float corner[3] = { -1.0, -1.0, 0.0 };
    static const float width[3]  = {  2.0,  0.0, 0.0 };
    static const float height[3] = {  0.0,  2.0, 0.0 };

    struct ngl_node *media       = ngl_node_create(NGL_NODE_MEDIA, filename);
    struct ngl_node *texture     = ngl_node_create(NGL_NODE_TEXTURE);
    struct ngl_node *quad        = ngl_node_create(NGL_NODE_QUAD, corner, width, height);
    struct ngl_node *shader      = ngl_node_create(NGL_NODE_SHADER);
    struct ngl_node *tshape      = ngl_node_create(NGL_NODE_TEXTUREDSHAPE, quad, shader);

    ngl_node_param_set(tshape, "texture0", texture);
    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_add(group, "children", 1, &tshape);

    ngl_node_unrefp(&tshape);
    ngl_node_unrefp(&shader);
    ngl_node_unrefp(&media);
    ngl_node_unrefp(&texture);
    ngl_node_unrefp(&quad);

    return group;
}
#endif

static struct ngl_node *get_scene(const char *filename)
{
    struct ngl_node *group = ngl_node_create(NGL_NODE_GROUP);

    const float duration = 30.;
    const float overlap_time = 1.;
    const int dim = 3;

    const float qw = 2. / dim;
    const float qh = qw;
    const int nb_videos = dim * dim;

    struct ngl_node *shader = ngl_node_create(NGL_NODE_SHADER);

    for (int y = 0; y < dim; y++) {
        for (int x = 0; x < dim; x++) {
            const int video_id = y*dim + x;
            const float start = video_id * duration / nb_videos;

            const float corner_x = -1. + x*qw;
            const float corner_y =  1. - (y+1)*qh;

            const float corner[3] = {corner_x, corner_y, 0};
            const float width[3]  = {qw, 0, 0};
            const float height[3] = {0, qh, 0};

            struct ngl_node *media   = ngl_node_create(NGL_NODE_MEDIA, filename);
            struct ngl_node *texture = ngl_node_create(NGL_NODE_TEXTURE);
            struct ngl_node *quad    = ngl_node_create(NGL_NODE_QUAD, corner, width, height);
            struct ngl_node *tshape  = ngl_node_create(NGL_NODE_TEXTUREDSHAPE, quad, shader);

            ngl_node_param_set(media, "start", start);
            ngl_node_param_set(tshape, "texture0", texture);
            ngl_node_param_set(texture, "data_src", media);

            struct ngl_node *rrs[3] = {
                ngl_node_create(NGL_NODE_RENDERRANGENORENDER, 0.),
                ngl_node_create(NGL_NODE_RENDERRANGECONTINUOUS, start),
                ngl_node_create(NGL_NODE_RENDERRANGENORENDER,
                                start + duration/nb_videos + overlap_time),
            };

            if (video_id) ngl_node_param_add(tshape, "ranges", 3, rrs);
            else          ngl_node_param_add(tshape, "ranges", 2, rrs + 1);

            ngl_node_param_add(group, "children", 1, &tshape);

            ngl_node_unrefp(&rrs[0]);
            ngl_node_unrefp(&rrs[1]);
            ngl_node_unrefp(&rrs[2]);
            ngl_node_unrefp(&tshape);
            ngl_node_unrefp(&media);
            ngl_node_unrefp(&texture);
            ngl_node_unrefp(&quad);
        }
    }

    ngl_node_unrefp(&shader);

    return group;
}

static int init(GLFWwindow *window, const char *filename)
{
    int platform = NGL_GLPLATFORM_GLX;
    int api = NGL_GLAPI_OPENGL3;

    g_ctx = ngl_create();
    ngl_set_glcontext(g_ctx, NULL, NULL, NULL, platform, api);
    ngl_set_viewport(g_ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    struct ngl_node *scene = get_scene(filename);
    int ret = ngl_set_scene(g_ctx, scene);
    if (ret < 0)
        return ret;

    ngl_node_unrefp(&scene);
    return 0;
}

static void render()
{
    ngl_draw(g_ctx, g_tick / (double)FRAMERATE);
    g_tick++;
}

static void reset()
{
    ngl_free(&g_ctx);
}

int main(int argc, char *argv[])
{
    GLFWwindow *window;
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "node.gl playground", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to initialize GL context\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    int nb_frames = 0;
    int64_t timer = gettime();

    int ret = init(window, argv[1]);
    if (ret < 0)
        goto end;

    do {
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
        nb_frames++;
        if (nb_frames == 1)
            timer = gettime();
    }
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
           glfwWindowShouldClose(window) == 0);

    fprintf(stderr, "FPS=%f\n", nb_frames / ((gettime() - timer) / (double)1000000));

end:
    reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    return ret;
}
