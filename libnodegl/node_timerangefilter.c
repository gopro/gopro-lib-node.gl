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

#include <float.h>
#include <stddef.h>
#include <string.h>

#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"

struct timerangefilter_priv {
    struct ngl_node *child;
    struct ngl_node **ranges;
    int nb_ranges;
    int current_range;
    double prefetch_time;
    double max_idle_time;

    int drawme;
};

#define RANGES_TYPES_LIST (const int[]){NGL_NODE_TIMERANGEMODEONCE,     \
                                        NGL_NODE_TIMERANGEMODENOOP,     \
                                        NGL_NODE_TIMERANGEMODECONT,     \
                                        -1}

#define OFFSET(x) offsetof(struct timerangefilter_priv, x)
static const struct node_param timerangefilter_params[] = {
    {"child", PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_NON_NULL,
              .desc=NGLI_DOCSTRING("time filtered scene")},
    {"ranges", PARAM_TYPE_NODELIST, OFFSET(ranges),
               .node_types=RANGES_TYPES_LIST,
               .flags=PARAM_FLAG_DOT_DISPLAY_PACKED,
               .desc=NGLI_DOCSTRING("key frame time filtering events")},
    {"prefetch_time", PARAM_TYPE_DBL, OFFSET(prefetch_time), {.dbl=1.0},
                      .desc=NGLI_DOCSTRING("`child` is prefetched `prefetch_time` seconds in advance")},
    {"max_idle_time", PARAM_TYPE_DBL, OFFSET(max_idle_time), {.dbl=4.0},
                      .desc=NGLI_DOCSTRING("`child` will not be released if it is required in the next incoming `max_idle_time` seconds")},
    {NULL}
};

static int timerangefilter_init(struct ngl_node *node)
{
    struct timerangefilter_priv *s = node->priv_data;

    double prev_start_time = -DBL_MAX;
    for (int i = 0; i < s->nb_ranges; i++) {
        const struct timerangemode_priv *trm = s->ranges[i]->priv_data;

        if (trm->start_time < prev_start_time) {
            LOG(ERROR, "time ranges must be monotically increasing: %g < %g",
                trm->start_time, prev_start_time);
            return NGL_ERROR_INVALID_ARG;
        }
        prev_start_time = trm->start_time;
    }

    if (s->prefetch_time < 0) {
        LOG(ERROR, "prefetch time must be positive");
        return NGL_ERROR_INVALID_ARG;
    }

    if (s->max_idle_time <= s->prefetch_time) {
        LOG(ERROR, "max idle time must be superior to prefetch time");
        return NGL_ERROR_INVALID_ARG;
    }

    return 0;
}

static int get_rr_id(const struct timerangefilter_priv *s, int start, double t)
{
    int ret = -1;

    for (int i = start; i < s->nb_ranges; i++) {
        const struct timerangemode_priv *rr = s->ranges[i]->priv_data;
        if (rr->start_time > t)
            break;
        ret = i;
    }
    return ret;
}

static int update_rr_state(struct timerangefilter_priv *s, double t)
{
    if (!s->nb_ranges)
        return NGL_ERROR_INVALID_ARG;

    int rr_id = get_rr_id(s, s->current_range, t);
    if (rr_id < 0) {
        // range not found, probably backward in time so look from the
        // start
        // TODO: look for rr using bsearch?
        rr_id = get_rr_id(s, 0, t);
    }

    if (rr_id >= 0) {
        if (s->current_range != rr_id) {
            // We leave our current render range, so we reset the "Once" flag
            // for next time we may come in again (seek back)
            struct ngl_node *cur_rr = s->ranges[s->current_range];
            if (cur_rr->class->id == NGL_NODE_TIMERANGEMODEONCE) {
                struct timerangemode_priv *rro = cur_rr->priv_data;
                rro->updated = 0;
            }
        }

        s->current_range = rr_id;
    }

    return rr_id;
}

static int timerangefilter_visit(struct ngl_node *node, int is_active, double t)
{
    struct timerangefilter_priv *s = node->priv_data;
    struct ngl_node *child = s->child;

    /*
     * The life of the parent takes over the life of its children: if the
     * parent is dead, the children are likely dead as well. However, a living
     * children from a dead parent can be revealed by another living branch.
     */
    if (is_active) {
        const int rr_id = update_rr_state(s, t);

        if (rr_id >= 0) {
            struct ngl_node *rr = s->ranges[rr_id];

            s->current_range = rr_id;

            if (rr->class->id == NGL_NODE_TIMERANGEMODENOOP) {
                is_active = 0;

                if (rr_id < s->nb_ranges - 1) {
                    // We assume here the next range requires the node started
                    // as the current one doesn't.
                    const struct timerangemode_priv *next = s->ranges[rr_id + 1]->priv_data;
                    const double next_use_in = next->start_time - t;

                    if (next_use_in <= s->prefetch_time) {
                        TRACE("next use of %s in %g (< %g), mark as active",
                              child->label, next_use_in, s->prefetch_time);

                        // The node will actually be needed soon, so we need to
                        // start it if necessary.
                        is_active = 1;
                    } else if (next_use_in <= s->max_idle_time && child->is_active) {
                        TRACE("%s not currently needed but will be soon %g (< %g), keep as active",
                              child->label, next_use_in, s->max_idle_time);

                        // The node will be needed in a slight amount of time;
                        // a bit longer than a prefetch period so we don't need
                        // to start it, but in the case where it's actually
                        // already active it's not worth releasing it to start
                        // it again soon after, so we keep it active.
                        is_active = 1;
                    }
                }
            } else if (rr->class->id == NGL_NODE_TIMERANGEMODEONCE) {
                // If the child of the current once range is inactive, meaning
                // it has been previously released, we need to force an update
                // otherwise the child will stay uninitialized.
                if (!child->is_active) {
                    struct timerangemode_priv *rro = rr->priv_data;
                    rro->updated = 0;
                }
            }
        }
    }

    return ngli_node_visit(child, is_active, t);
}

static int timerangefilter_update(struct ngl_node *node, double t)
{
    struct timerangefilter_priv *s = node->priv_data;

    s->drawme = 0;

    const int rr_id = update_rr_state(s, t);
    if (rr_id >= 0) {
        struct ngl_node *rr = s->ranges[rr_id];

        if (rr->class->id == NGL_NODE_TIMERANGEMODENOOP)
            return 0;

        if (rr->class->id == NGL_NODE_TIMERANGEMODEONCE) {
            struct timerangemode_priv *rro = rr->priv_data;
            if (rro->updated)
                return 0;
            t = rro->render_time;
            rro->updated = 1;
        }
    }

    s->drawme = 1;

    struct ngl_node *child = s->child;
    return ngli_node_update(child, t);
}

static void timerangefilter_draw(struct ngl_node *node)
{
    struct timerangefilter_priv *s = node->priv_data;

    if (!s->drawme) {
        TRACE("%s @ %p not marked for drawing, skip it", node->label, node);
        return;
    }

    struct ngl_node *child = s->child;
    ngli_node_draw(child);
}

const struct node_class ngli_timerangefilter_class = {
    .id        = NGL_NODE_TIMERANGEFILTER,
    .name      = "TimeRangeFilter",
    .init      = timerangefilter_init,
    .visit     = timerangefilter_visit,
    .update    = timerangefilter_update,
    .draw      = timerangefilter_draw,
    .priv_size = sizeof(struct timerangefilter_priv),
    .params    = timerangefilter_params,
    .file      = __FILE__,
};
