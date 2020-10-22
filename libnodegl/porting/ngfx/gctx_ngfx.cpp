/*
 * Copyright 2018 GoPro Inc.
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
#include "gctx_ngfx.h"
#include "buffer_ngfx.h"
#include "gtimer_ngfx.h"
#include "pipeline_ngfx.h"
#include "program_ngfx.h"
#include "rendertarget_ngfx.h"
#include "swapchain_ngfx.h"
#include "texture_ngfx.h"
#include "memory.h"
#include "log.h"
#include "math_utils.h"
#include "format.h"
#include "util_ngfx.h"
#ifdef ENABLE_CAPTURE
#include "capture.h"
static bool DEBUG_CAPTURE = (getenv("DEBUG_CAPTURE") != NULL);
#endif
using namespace std;
using namespace ngfx;

static struct gctx *ngfx_create(const struct ngl_config *config)
{
    gctx_ngfx* ctx = new gctx_ngfx;
    return (struct gctx *)ctx;
}

static int create_offscreen_resources(struct gctx *s) {
    gctx_ngfx *ctx = (gctx_ngfx *)s;
    const ngl_config *config = &s->config;
    bool enable_depth_stencil = true;

    auto &color_texture = ctx->offscreen_resources.color_texture;
    color_texture = ngli_texture_create(s);
    texture_params color_texture_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    color_texture_params.width = config->width;
    color_texture_params.height = config->height;
    color_texture_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    color_texture_params.samples = config->samples;
    color_texture_params.usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT | NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT |
        NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    ngli_texture_init(color_texture, &color_texture_params);

    auto &depth_texture = ctx->offscreen_resources.depth_texture;
    if (enable_depth_stencil) {
        depth_texture = ngli_texture_create(s);
        texture_params depth_texture_params = NGLI_TEXTURE_PARAM_DEFAULTS;
        depth_texture_params.width = config->width;
        depth_texture_params.height = config->height;
        depth_texture_params.format = to_ngli_format(ctx->graphics_context->depthFormat);
        depth_texture_params.samples = config->samples;
        depth_texture_params.usage = NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT |
            NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT | NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ngli_texture_init(depth_texture, &depth_texture_params);
    }

    rendertarget_params rt_params = {};
    rt_params.width = config->width;
    rt_params.height = config->height;
    rt_params.samples = config->samples;
    rt_params.nb_colors = 1;
    rt_params.colors[0].attachment = color_texture;
    rt_params.depth_stencil.attachment = depth_texture,
    rt_params.readable = 1;

    auto &rt = ctx->offscreen_resources.rt;
    rt = ngli_rendertarget_create(s);
    if (!rt)
        return NGL_ERROR_MEMORY;

    int ret = ngli_rendertarget_init(rt, &rt_params);
    if (ret < 0)
        return ret;

    s->default_rendertarget = rt;

    return 0;
}

static int ngfx_init(struct gctx *s)
{
    const ngl_config *config = &s->config;
    gctx_ngfx *ctx = (gctx_ngfx *)s;
#ifdef ENABLE_CAPTURE
    if (DEBUG_CAPTURE) init_capture();
#endif
    /* FIXME */
    s->features = -1;
    ctx->graphics_context = GraphicsContext::create("NGLApplication", true);
#ifdef ENABLE_CAPTURE
    if (DEBUG_CAPTURE) begin_capture();
#endif
    if (config->offscreen) {
        Surface surface(config->width, config->height, true);
        ctx->graphics_context->setSurface(&surface);
    }
    else {
        TODO("create window surface or use existing handle");
    }
    ctx->graphics = Graphics::create(ctx->graphics_context);

    //TODO: use rendertarget (ngli_rendertarget_init)

    if (!config->offscreen) {
        TODO("create window surface");
    }
    else {
        create_offscreen_resources(s);
    }

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        memcpy(ctx->viewport, viewport, sizeof(ctx->viewport));
    } else {
        const int default_viewport[] = {0, 0, config->width, config->height};
        memcpy(ctx->viewport, default_viewport, sizeof(ctx->viewport));
    }

    const int scissor[] = {0, 0, config->width, config->height};
    memcpy(ctx->scissor, scissor, sizeof(ctx->scissor));

    ngli_gctx_set_clear_color(s, config->clear_color);

    if (config->offscreen) {
        ctx->default_rendertarget_desc.nb_colors = 1;
        ctx->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_R8G8B8A8_UNORM;
        ctx->default_rendertarget_desc.colors[0].samples = config->samples;
        ctx->default_rendertarget_desc.colors[0].resolve = config->samples > 0 ? 1 : 0;
        ctx->default_rendertarget_desc.depth_stencil.format = to_ngli_format(ctx->graphics_context->depthFormat);
        ctx->default_rendertarget_desc.depth_stencil.samples = config->samples;
        ctx->default_rendertarget_desc.depth_stencil.resolve = 0;
    } else {
        ctx->default_rendertarget_desc.nb_colors = 1;
        ctx->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_B8G8R8A8_UNORM;
        ctx->default_rendertarget_desc.colors[0].samples = config->samples;
        ctx->default_rendertarget_desc.colors[0].resolve = config->samples > 0 ? 1 : 0;
        ctx->default_rendertarget_desc.depth_stencil.format = to_ngli_format(ctx->graphics_context->depthFormat);
        ctx->default_rendertarget_desc.depth_stencil.samples = config->samples;
    }

    s->limits.max_compute_work_group_counts[0] = INT_MAX;
    s->limits.max_compute_work_group_counts[1] = INT_MAX;
    s->limits.max_compute_work_group_counts[2] = INT_MAX;
    return 0;
}

static int ngfx_resize(struct gctx *s, int width, int height, const int *viewport)
{ TODO("w: %d h: %d", width, height);
    return 0;
}

static int ngfx_pre_draw(struct gctx *s, double t)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    s_priv->cur_command_buffer = s_priv->graphics_context->drawCommandBuffer();
    s_priv->cur_command_buffer->begin();
    ngli_gctx_bind_rendertarget(s, nullptr);
    return 0;
}

static int ngfx_post_draw(struct gctx *s, double t)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    ngli_gctx_bind_rendertarget(s, nullptr);
    s_priv->cur_command_buffer->end();
    s_priv->graphics_context->submit(s_priv->cur_command_buffer);
    if (s->config.offscreen && s->config.capture_buffer) {
        uint32_t size = s->config.width * s->config.height * 4;
        auto &output_texture = ((texture_ngfx *)s_priv->offscreen_resources.color_texture)->v;
        output_texture->download(s->config.capture_buffer, size);
    }
    return 0;
}

static void ngfx_wait_idle(struct gctx *s)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    s_priv->graphics->waitIdle(s_priv->cur_command_buffer);
}

static void ngfx_destroy(struct gctx *s)
{
    gctx_ngfx* ctx = new gctx_ngfx;
    auto output_color_texture = ((texture *)ctx->offscreen_resources.color_texture);
    auto output_depth_texture = ((texture *)ctx->offscreen_resources.depth_texture);
    if (output_depth_texture) ngli_texture_freep(&output_depth_texture);
    if (output_color_texture) ngli_texture_freep(&output_color_texture);
    delete ctx->graphics;
    delete ctx->graphics_context;
    delete ctx;
#ifdef ENABLE_CAPTURE
    if (DEBUG_CAPTURE) end_capture();
#endif
}

static int ngfx_transform_cull_mode(struct gctx *s, int cull_mode)
{
    return cull_mode;
}

static void ngfx_transform_projection_matrix(struct gctx *s, float *dst)
{
#ifdef GRAPHICS_BACKEND_VULKAN
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f,
    };
    ngli_mat4_mul(dst, matrix, dst);
#endif
}

static void ngfx_get_rendertarget_uvcoord_matrix(struct gctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
    memcpy(dst, matrix, 4 * 4 * sizeof(float));
}

static void ngfx_bind_rendertarget(struct gctx *s, struct rendertarget *rt)
{
    if (s->cur_rendertarget != rt) {
        if (s->cur_rendertarget) ngli_rendertarget_end_pass(s->cur_rendertarget);
        if (rt) ngli_rendertarget_begin_pass(rt);
    }
    s->cur_rendertarget = rt;
}

static struct rendertarget *ngfx_get_rendertarget(struct gctx *s)
{
    return s->cur_rendertarget;
}

static const struct rendertarget_desc *ngfx_get_default_rendertarget_desc(struct gctx *s)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    return &s_priv->default_rendertarget_desc;
}

static void ngfx_set_viewport(struct gctx *s, const int *vp)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(s_priv->viewport, vp, sizeof(s_priv->viewport));
    s_priv->graphics->setViewport(s_priv->cur_command_buffer, { vp[0], vp[1], uint32_t(vp[2]), uint32_t(vp[3]) });
}

static void ngfx_get_viewport(struct gctx *s, int *viewport)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(viewport, &s_priv->viewport, sizeof(s_priv->viewport));
}

static void ngfx_set_scissor(struct gctx *s, const int *sr)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(&s_priv->scissor, sr, sizeof(s_priv->scissor));
    s_priv->graphics->setScissor(s_priv->cur_command_buffer, { sr[0], sr[1], uint32_t(sr[2]), uint32_t(sr[3]) });
}

static void ngfx_get_scissor(struct gctx *s, int *scissor)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(scissor, &s_priv->scissor, sizeof(s_priv->scissor));
}

static void ngfx_set_clear_color(struct gctx *s, const float *color)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(s_priv->clear_color, color, sizeof(s_priv->clear_color));
}

static void ngfx_get_clear_color(struct gctx *s, float *color)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(color, &s_priv->clear_color, sizeof(s_priv->clear_color));
}

static void ngfx_clear_color(struct gctx *s)
{ TODO();

}

static void ngfx_clear_depth_stencil(struct gctx *s)
{ TODO();

}

static void ngfx_invalidate_depth_stencil(struct gctx *s)
{ TODO();

}

static void ngfx_flush(struct gctx *s)
{ TODO();

}

static int ngfx_get_preferred_depth_format(struct gctx *s)
{
    gctx_ngfx *ctx = (gctx_ngfx *)s;
    return to_ngli_format(ctx->graphics_context->depthFormat);
}

static int ngfx_get_preferred_depth_stencil_format(struct gctx *s)
{
    gctx_ngfx *ctx = (gctx_ngfx *)s;
    return to_ngli_format(ctx->graphics_context->depthFormat);
}

extern "C" const struct gctx_class ngli_gctx_ngfx = {
    .name         = "NGFX",
    .create       = ngfx_create,
    .init         = ngfx_init,
    .resize       = ngfx_resize,
    .pre_draw     = ngfx_pre_draw,
    .post_draw    = ngfx_post_draw,
    .wait_idle    = ngfx_wait_idle,
    .destroy      = ngfx_destroy,

    .transform_cull_mode = ngfx_transform_cull_mode,
    .transform_projection_matrix = ngfx_transform_projection_matrix,
    .get_rendertarget_uvcoord_matrix = ngfx_get_rendertarget_uvcoord_matrix,

    .bind_rendertarget         = ngfx_bind_rendertarget,
    .get_rendertarget         = ngfx_get_rendertarget,
    .get_default_rendertarget_desc = ngfx_get_default_rendertarget_desc,
    .set_viewport             = ngfx_set_viewport,
    .get_viewport             = ngfx_get_viewport,
    .set_scissor              = ngfx_set_scissor,
    .get_scissor              = ngfx_get_scissor,
    .set_clear_color          = ngfx_set_clear_color,
    .get_clear_color          = ngfx_get_clear_color,
    .clear_color              = ngfx_clear_color,
    .clear_depth_stencil      = ngfx_clear_depth_stencil,
    .invalidate_depth_stencil = ngfx_invalidate_depth_stencil,
    .get_preferred_depth_format= ngfx_get_preferred_depth_format,
    .get_preferred_depth_stencil_format=ngfx_get_preferred_depth_stencil_format,
    .flush                    = ngfx_flush,

    .buffer_create = ngli_buffer_ngfx_create,
    .buffer_init   = ngli_buffer_ngfx_init,
    .buffer_upload = ngli_buffer_ngfx_upload,
    .buffer_download = ngli_buffer_ngfx_download,
    .buffer_map      = ngli_buffer_ngfx_map,
    .buffer_unmap    = ngli_buffer_ngfx_unmap,
    .buffer_freep  = ngli_buffer_ngfx_freep,

    .gtimer_create = ngli_gtimer_ngfx_create,
    .gtimer_init   = ngli_gtimer_ngfx_init,
    .gtimer_start  = ngli_gtimer_ngfx_start,
    .gtimer_stop   = ngli_gtimer_ngfx_stop,
    .gtimer_read   = ngli_gtimer_ngfx_read,
    .gtimer_freep  = ngli_gtimer_ngfx_freep,

    .pipeline_create         = ngli_pipeline_ngfx_create,
    .pipeline_init           = ngli_pipeline_ngfx_init,
    .pipeline_bind_resources = ngli_pipeline_ngfx_bind_resources,
    .pipeline_update_attribute = ngli_pipeline_ngfx_update_attribute,
    .pipeline_update_uniform = ngli_pipeline_ngfx_update_uniform,
    .pipeline_update_texture = ngli_pipeline_ngfx_update_texture,
    .pipeline_draw           = ngli_pipeline_ngfx_draw,
    .pipeline_draw_indexed   = ngli_pipeline_ngfx_draw_indexed,
    .pipeline_dispatch       = ngli_pipeline_ngfx_dispatch,
    .pipeline_freep          = ngli_pipeline_ngfx_freep,

    .program_create = ngli_program_ngfx_create,
    .program_init   = ngli_program_ngfx_init,
    .program_freep  = ngli_program_ngfx_freep,

    .rendertarget_create        = ngli_rendertarget_ngfx_create,
    .rendertarget_init          = ngli_rendertarget_ngfx_init,
    .rendertarget_resolve       = ngli_rendertarget_ngfx_resolve,
    .rendertarget_read_pixels   = ngli_rendertarget_ngfx_read_pixels,
    .rendertarget_begin_pass    = ngli_rendertarget_ngfx_begin_pass,
    .rendertarget_end_pass      = ngli_rendertarget_ngfx_end_pass,
    .rendertarget_freep         = ngli_rendertarget_ngfx_freep,

    .swapchain_create         = ngli_swapchain_ngfx_create,
    .swapchain_destroy        = ngli_swapchain_ngfx_destroy,
    .swapchain_acquire_image  = ngli_swapchain_ngfx_acquire_image,

    .texture_create           = ngli_texture_ngfx_create,
    .texture_init             = ngli_texture_ngfx_init,
    .texture_has_mipmap       = ngli_texture_ngfx_has_mipmap,
    .texture_upload           = ngli_texture_ngfx_upload,
    .texture_generate_mipmap  = ngli_texture_ngfx_generate_mipmap,
    .texture_freep            = ngli_texture_ngfx_freep,
};
