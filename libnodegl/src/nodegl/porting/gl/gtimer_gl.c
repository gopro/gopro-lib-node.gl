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

#include <string.h>

#include "nodegl/core/gctx.h"
#include "nodegl/porting/gl/gctx_gl.h"
#include "nodegl/porting/gl/gtimer_gl.h"
#include "nodegl/core/log.h"
#include "nodegl/core/memory.h"

static void noop(const struct glcontext *gl, ...)
{
}

struct gtimer *ngli_gtimer_gl_create(struct gctx *gctx)
{
    struct gtimer_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct gtimer *)s;
}

int ngli_gtimer_gl_init(struct gtimer *s)
{
    struct gtimer_gl *s_priv = (struct gtimer_gl *)s;
    struct gctx_gl *gctx = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx->glcontext;

    if (gl->features & NGLI_FEATURE_TIMER_QUERY) {
        s_priv->glGenQueries          = ngli_glGenQueries;
        s_priv->glDeleteQueries       = ngli_glDeleteQueries;
        s_priv->glBeginQuery          = ngli_glBeginQuery;
        s_priv->glEndQuery            = ngli_glEndQuery;
        s_priv->glGetQueryObjectui64v = ngli_glGetQueryObjectui64v;
    } else if (gl->features & NGLI_FEATURE_EXT_DISJOINT_TIMER_QUERY) {
        s_priv->glGenQueries          = ngli_glGenQueriesEXT;
        s_priv->glDeleteQueries       = ngli_glDeleteQueriesEXT;
        s_priv->glBeginQuery          = ngli_glBeginQueryEXT;
        s_priv->glEndQuery            = ngli_glEndQueryEXT;
        s_priv->glGetQueryObjectui64v = ngli_glGetQueryObjectui64vEXT;
    } else {
        s_priv->glGenQueries          = (void *)noop;
        s_priv->glDeleteQueries       = (void *)noop;
        s_priv->glBeginQuery          = (void *)noop;
        s_priv->glEndQuery            = (void *)noop;
        s_priv->glGetQueryObjectui64v = (void *)noop;
    }

    s_priv->glGenQueries(gl, 1, &s_priv->query);
    return 0;
}

int ngli_gtimer_gl_start(struct gtimer *s)
{
    struct gtimer_gl *s_priv = (struct gtimer_gl *)s;
    struct gctx_gl *gctx = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx->glcontext;

    if (gctx->timer_active) {
        LOG(WARNING, "only one instance of GPU timings can be present "
            "in the same graph due to OpenGL limitations");
        return 0;
    }

    /*
     * This specific instance of gtimer was able to grab the global
     * "timer active" lock
     */
    gctx->timer_active = 1;
    s_priv->started = 1;
    s_priv->query_result = 0;
    s_priv->glBeginQuery(gl, GL_TIME_ELAPSED, s_priv->query);
    return 0;
}

int ngli_gtimer_gl_stop(struct gtimer *s)
{
    struct gtimer_gl *s_priv = (struct gtimer_gl *)s;
    struct gctx_gl *gctx = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx->glcontext;

    if (s_priv->started) {
        s_priv->glEndQuery(gl, GL_TIME_ELAPSED);
        s_priv->glGetQueryObjectui64v(gl, s_priv->query, GL_QUERY_RESULT, &s_priv->query_result);
        s_priv->started = 0;
        gctx->timer_active = 0;
    }
    return 0;
}

int64_t ngli_gtimer_gl_read(struct gtimer *s)
{
    struct gtimer_gl *s_priv = (struct gtimer_gl *)s;
    return s_priv->query_result;
}

void ngli_gtimer_gl_freep(struct gtimer **sp)
{
    if (!*sp)
        return;

    struct gtimer *s = *sp;
    struct gtimer_gl *s_priv = (struct gtimer_gl *)s;
    struct gctx_gl *gctx = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx->glcontext;
    s_priv->glDeleteQueries(gl, 1, &s_priv->query);
    ngli_freep(sp);
}
