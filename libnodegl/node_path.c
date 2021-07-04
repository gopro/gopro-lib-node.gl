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

#include <stddef.h>
#include <string.h>

#include "nodegl.h"
#include "nodes.h"
#include "path.h"

struct path_priv {
    struct path *path;

    struct ngl_node **keyframes;
    int nb_keyframes;
    int precision;
};

#define OFFSET(x) offsetof(struct path_priv, x)
static const struct node_param path_params[] = {
    {"keyframes", NGLI_PARAM_TYPE_NODELIST, OFFSET(keyframes),
                  .node_types=(const int[]){
                      NGL_NODE_PATHKEYMOVE,
                      NGL_NODE_PATHKEYLINE,
                      NGL_NODE_PATHKEYBEZIER2,
                      NGL_NODE_PATHKEYBEZIER3,
                      -1
                    },
                  .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_PACKED,
                  .desc=NGLI_DOCSTRING("anchor points the path go through")},
    {"precision", NGLI_PARAM_TYPE_INT, OFFSET(precision), {.i64=64},
                  .desc=NGLI_DOCSTRING("number of divisions per curve segment")},
    {NULL}
};

/* We must have the struct path in 1st position for AnimatedPath and PathDraw */
NGLI_STATIC_ASSERT(path_1st_field, OFFSET(path) == 0);

static int path_init(struct ngl_node *node)
{
    struct path_priv *s = node->priv_data;

    s->path = ngli_path_create();
    if (!s->path)
        return NGL_ERROR_MEMORY;

    for (int i = 0; i < s->nb_keyframes; i++) {
        const struct ngl_node *kf = s->keyframes[i];
        int ret;
        if (kf->cls->id == NGL_NODE_PATHKEYMOVE) {
            const struct pathkey_move_priv *move = kf->priv_data;
            ret = ngli_path_move_to(s->path, move->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYLINE) {
            const struct pathkey_line_priv *line = kf->priv_data;
            ret = ngli_path_line_to(s->path, line->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYBEZIER2) {
            const struct pathkey_bezier2_priv *bezier2 = kf->priv_data;
            ret = ngli_path_bezier2_to(s->path, bezier2->control, bezier2->to);
        } else if (kf->cls->id == NGL_NODE_PATHKEYBEZIER3) {
            const struct pathkey_bezier3_priv *bezier3 = kf->priv_data;
            ret = ngli_path_bezier3_to(s->path, bezier3->control1, bezier3->control2, bezier3->to);
        } else {
            ngli_assert(0);
        }
        if (ret < 0)
            return ret;
    }

    return ngli_path_init(s->path, s->precision);
}

static void path_uninit(struct ngl_node *node)
{
    struct path_priv *s = node->priv_data;
    ngli_path_freep(&s->path);
}

const struct node_class ngli_path_class = {
    .id        = NGL_NODE_PATH,
    .name      = "Path",
    .init      = path_init,
    .uninit    = path_uninit,
    .priv_size = sizeof(struct path_priv),
    .params    = path_params,
    .file      = __FILE__,
};
