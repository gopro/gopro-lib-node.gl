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

#include "gctx_vk.h"
#include "gtimer_vk.h"
#include "memory.h"
#include "gctx_vk.h"

struct gtimer *ngli_gtimer_vk_create(struct gctx *gctx)
{
    struct gtimer_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct gtimer *)s;
}

int ngli_gtimer_vk_init(struct gtimer *s)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct gtimer_vk *s_priv = (struct gtimer_vk *)s;

    const VkQueryPoolCreateInfo query_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    VkResult vkret = vkCreateQueryPool(vk->device, &query_pool_create_info, NULL, &s_priv->query_pool);
    if (vkret != VK_SUCCESS)
        return -1;

    return 0;
}

#if 0
static int write_ts(struct gtimer *s, int query)
{
    struct gctx_vk *gctx = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx->vkcontext;
    struct gtimer_vk *s_priv = (struct gtimer_vk *)s;

    // XXX: we are always comparing from the top of the pipe; is this correct?
    VkCommandBuffer command_buffer = vk->cur_command_buffer;
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_priv->query_pool, query);

    return 0;
}
#endif

int ngli_gtimer_vk_start(struct gtimer *s)
{
#if 0
    struct gctx_vk *gctx = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx->vkcontext;
    struct gtimer_vk *s_priv = (struct gtimer_vk *)s;

    ngli_gctx_vk_end_render_pass(gctx);

    VkCommandBuffer command_buffer = vk->cur_command_buffer;

    vkCmdResetQueryPool(command_buffer, s_priv->query_pool, 0, 2);

    return write_ts(s, 0);
#else
    return 0;
#endif
}

int ngli_gtimer_vk_stop(struct gtimer *s)
{
#if 0
    struct gctx_vk *gctx = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx->vkcontext;
    struct gtimer_vk *s_priv = (struct gtimer_vk *)s;

    ngli_gctx_vk_end_render_pass(gctx);

    int ret = write_ts(s, 1);
    if (ret < 0)
        return ret;

    // XXX: extra security, could be dropped
    memset(s_priv->results, 0, sizeof(s_priv->results));

    // XXX: we're probably pulling previous results here
    // TODO: check code return
    // XXX: the parameters are somehow redundants, why?
    /*
    vkGetQueryPoolResults(vk->device, s_priv->query_pool, 0, 2, sizeof(s_priv->results),
                          s_priv->results, sizeof(s_priv->results[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
                          */

    return 0;
#else
    return 0;
#endif
}

int64_t ngli_gtimer_vk_read(struct gtimer *s)
{
    //XXX: wrong unit?
    //LOG(ERROR, "results: %ld %ld -> %ld", s_priv->results[0], s_priv->results[1], s_priv->results[1] - s_priv->results[0]);
    struct gtimer_vk *s_priv = (struct gtimer_vk *)s;
    return s_priv->results[1] - s_priv->results[0];
}

void ngli_gtimer_vk_freep(struct gtimer **sp)
{
    if (!*sp)
        return;

    struct gtimer *s = *sp;
    struct gtimer_vk *s_priv = (struct gtimer_vk *)s;
    struct gctx_vk *gctx = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx->vkcontext;
    vkDestroyQueryPool(vk->device, s_priv->query_pool, NULL);
    ngli_freep(sp);
}
