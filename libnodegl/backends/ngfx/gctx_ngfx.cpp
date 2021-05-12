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
extern "C" {
#include "memory.h"
#include "math_utils.h"
#include "format.h"
}
#include "buffer_ngfx.h"
#include "pipeline_ngfx.h"
#include "program_ngfx.h"
#include "rendertarget_ngfx.h"
#include "swapchain_util_ngfx.h"
#include "texture_ngfx.h"
#include "util_ngfx.h"
#include "debugutil_ngfx.h"
#include "surface_util_ngfx.h"
#include <glm/gtc/type_ptr.hpp>

#if DEBUG_GPU_CAPTURE
extern "C" {
#include "gpu_capture.h"
}
#endif
using namespace std;
using namespace ngfx;

static struct gctx *ngfx_create(const struct ngl_config *config)
{
    gctx_ngfx *s = (gctx_ngfx *)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    return (struct gctx*)s;
}

static int create_onscreen_resources(struct gctx *s) {
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    // use ngfx default renderpass
    s_priv->default_rendertarget = nullptr;
    return 0;
}

static int create_offscreen_resources(struct gctx *s) {
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    const ngl_config *config = &s->config;
    bool enable_depth_stencil = true;

    auto &color_texture = s_priv->offscreen_resources.color_texture;
    color_texture = ngli_texture_create(s);
    texture_params color_texture_params = {
        .type = NGLI_TEXTURE_TYPE_2D,
        .format = NGLI_FORMAT_R8G8B8A8_UNORM,
        .width = config->width,
        .height = config->height,
        .samples = config->samples,
        .usage = NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT
        //TODO: if MSAA target, set to TRANSIENT_ATTACHMENT_BIT
    };
    if (config->samples == 1)
        color_texture_params.usage |= NGLI_TEXTURE_USAGE_SAMPLED_BIT;

    ngli_texture_init(color_texture, &color_texture_params);

    auto &depth_texture = s_priv->offscreen_resources.depth_texture;
    if (enable_depth_stencil) {
        depth_texture = ngli_texture_create(s);
        texture_params depth_texture_params = {
            .type = NGLI_TEXTURE_TYPE_2D,
            .format = to_ngli_format(s_priv->graphics_context->depthFormat),
            .width = config->width,
            .height = config->height,
            .samples = config->samples,
            .usage = NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        };
        ngli_texture_init(depth_texture, &depth_texture_params);
    }

    rendertarget_params rt_params = {};
    rt_params.width = config->width;
    rt_params.height = config->height;
    rt_params.nb_colors = 1;
    memcpy(rt_params.colors[0].clear_value, config->clear_color, sizeof(config->clear_color));
    rt_params.colors[0].attachment = color_texture;
    rt_params.depth_stencil.attachment = depth_texture,
    rt_params.readable = 1;

    if (config->samples) {
        auto &color_resolve_texture = s_priv->offscreen_resources.color_resolve_texture;
        color_resolve_texture = ngli_texture_create(s);

        texture_params color_resolve_texture_params = {
            .type    = NGLI_TEXTURE_TYPE_2D,
            .format  = NGLI_FORMAT_R8G8B8A8_UNORM,
            .width   = config->width,
            .height  = config->height,
            .samples = 1,
            .usage   = NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT | NGLI_TEXTURE_USAGE_SAMPLED_BIT,
        };

        // FIXME: current function returns VkResult
        ngli_texture_init(color_resolve_texture, &color_resolve_texture_params);
        rt_params.colors[0].resolve_target = color_resolve_texture;
    }

    auto &rt = s_priv->offscreen_resources.rt;
    rt = ngli_rendertarget_create(s);
    if (!rt)
        return int(NGL_ERROR_MEMORY);

    int ret = ngli_rendertarget_init(rt, &rt_params);
    if (ret < 0)
        return ret;

    s_priv->default_rendertarget = rt;

    return 0;
}

static int create_dummy_texture(struct gctx *s) {
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    auto &dummy_texture = s_priv->dummy_texture;
    dummy_texture = ngli_texture_create(s);
    texture_params dummy_texture_params = {
        .type = NGLI_TEXTURE_TYPE_2D,
        .format = NGLI_FORMAT_R8G8B8A8_UNORM,
        .width = 1,
        .height = 1,
        .samples = 1,
        .usage = NGLI_TEXTURE_USAGE_SAMPLED_BIT | NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT
    };

    ngli_texture_init(dummy_texture, &dummy_texture_params);
    return 0;
}

static void ngfx_set_clear_color(struct gctx *s, const float *color);

static int ngfx_init(struct gctx *s)
{
    const ngl_config *config = &s->config;
    if (config->width == 0 || config->height == 0) {
        LOG(ERROR, "invalid config: width = %d height = %d", config->width, config->height);
            return NGL_ERROR_INVALID_ARG;
    }
    gctx_ngfx *ctx = (gctx_ngfx *)s;
#if DEBUG_GPU_CAPTURE
    const char* var = getenv("NGL_GPU_CAPTURE");
    s->gpu_capture = var && !strcmp(var, "yes");
    if (s->gpu_capture) {
        s->gpu_capture_ctx = gpu_capture_ctx_create(s);
        if (!s->gpu_capture_ctx) {
            LOG(ERROR, "could not create GPU capture context");
            return NGL_ERROR_MEMORY;
        }
        int ret = gpu_capture_init(s->gpu_capture_ctx);
        if (ret < 0) {
            LOG(ERROR, "could not initialize GPU capture");
            s->gpu_capture = 0;
            return ret;
}
    }
#endif
    /* FIXME */
    s->features = -1;
    ctx->graphics_context = GraphicsContext::create("NGLApplication", true);
    auto &graphics_context = ctx->graphics_context;
#if DEBUG_GPU_CAPTURE
    if (s->gpu_capture)
        gpu_capture_begin(s->gpu_capture_ctx);
#endif
    if (config->offscreen) {
        ctx->surface = surface_util_ngfx::create_offscreen_surface(config->width, config->height);
    }
    else {
        ctx->surface = surface_util_ngfx::create_surface_from_window_handle(graphics_context, 
            config->platform, config->display, config->window, config->width, config->height);
        ctx->swapchain_util = swapchain_util_ngfx::create(graphics_context, config->window);
    }
    graphics_context->setSurface(ctx->surface);
    ctx->graphics = Graphics::create(graphics_context);

    if (!config->offscreen) {
        create_onscreen_resources(s);
    }
    else {
        create_offscreen_resources(s);
    }

    create_dummy_texture(s);

    const int *viewport = config->viewport;
    if (viewport[2] > 0 && viewport[3] > 0) {
        memcpy(ctx->viewport, viewport, sizeof(ctx->viewport));
    } else {
        const int default_viewport[] = {0, 0, config->width, config->height};
        memcpy(ctx->viewport, default_viewport, sizeof(ctx->viewport));
    }

    const int scissor[] = {0, 0, config->width, config->height};
    memcpy(ctx->scissor, scissor, sizeof(ctx->scissor));

    ngfx_set_clear_color(s, config->clear_color);

    if (config->offscreen) {
        ctx->default_rendertarget_desc.nb_colors = 1;
        ctx->default_rendertarget_desc.colors[0].format = NGLI_FORMAT_R8G8B8A8_UNORM;
        ctx->default_rendertarget_desc.samples = config->samples;
        ctx->default_rendertarget_desc.colors[0].resolve = config->samples > 0 ? 1 : 0;
        ctx->default_rendertarget_desc.depth_stencil.format = to_ngli_format(graphics_context->depthFormat);
        ctx->default_rendertarget_desc.depth_stencil.resolve = 0;
    } else {
        ctx->default_rendertarget_desc.nb_colors = 1;
        ctx->default_rendertarget_desc.colors[0].format = to_ngli_format(graphics_context->surfaceFormat);
        ctx->default_rendertarget_desc.samples = config->samples;
        ctx->default_rendertarget_desc.colors[0].resolve = config->samples > 0 ? 1 : 0;
        ctx->default_rendertarget_desc.depth_stencil.format = to_ngli_format(graphics_context->depthFormat);
    }

    s->limits.max_compute_work_group_count[0] = INT_MAX;
    s->limits.max_compute_work_group_count[1] = INT_MAX;
    s->limits.max_compute_work_group_count[2] = INT_MAX;

    s->limits.max_samples = 8;

    if (config->hud)
        ctx->enable_profiling = true;

    return 0;
}

static int ngfx_resize(struct gctx *s, int width, int height, const int *viewport)
{ TODO("w: %d h: %d", width, height);
    return 0;
}

static int ngfx_set_capture_buffer(struct gctx* s, void* capture_buffer)
{
    struct gctx_ngfx* s_priv = (struct gctx_ngfx*)s;
    if (!s->config.offscreen)
        return NGL_ERROR_INVALID_USAGE;
    s->config.capture_buffer = capture_buffer;
    return 0;
}

static int ngfx_begin_update(struct gctx *s, double t)
{
    return 0;
}

static int ngfx_end_update(struct gctx *s, double t)
{
    return 0;
}

static void ngfx_begin_render_pass(struct gctx *s, struct rendertarget *rt);
static void ngfx_end_render_pass(struct gctx *s);

static int ngfx_begin_draw(struct gctx *s, double t)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    GraphicsContext *ctx = s_priv->graphics_context;
    if (!s->config.offscreen) {
        s_priv->swapchain_util->acquire_image();
    }
    s_priv->cur_command_buffer = s_priv->graphics_context->drawCommandBuffer();
    CommandBuffer *cmd_buf = s_priv->cur_command_buffer;
    const struct ngl_config *config = &s->config;
    cmd_buf->begin();
    if (s_priv->enable_profiling) {
        s_priv->graphics->beginProfile(cmd_buf);
    }
    ngfx_begin_render_pass(s, s_priv->default_rendertarget);
    int *vp = s_priv->viewport;
    s_priv->graphics->setViewport(cmd_buf, { vp[0], vp[1], uint32_t(vp[2]), uint32_t(vp[3]) });
    int *sr = s_priv->scissor;
    s_priv->graphics->setScissor(cmd_buf, { sr[0], sr[1], uint32_t(sr[2]), uint32_t(sr[3]) });

    return 0;
}

static int ngfx_end_draw(struct gctx *s, double t)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    GraphicsContext *ctx = s_priv->graphics_context;
    ngfx_end_render_pass(s);
    if (s_priv->enable_profiling) {
        s_priv->profile_data.time = s_priv->graphics->endProfile(s_priv->cur_command_buffer);
    }
    s_priv->cur_command_buffer->end();
    if (s->config.offscreen) {
        ctx->submit(s_priv->cur_command_buffer);
        if (s->config.capture_buffer) {
            uint32_t size = s->config.width * s->config.height * 4;
            auto &output_color_resolve_texture = s_priv->offscreen_resources.color_resolve_texture;
            auto &output_color_texture = s_priv->offscreen_resources.color_texture;
            auto &output_texture = ((texture_ngfx *)(output_color_resolve_texture ? output_color_resolve_texture : output_color_texture))->v;
            output_texture->download(s->config.capture_buffer, size);
        } else {
            if (ctx->queue) ctx->queue->waitIdle();
        }
    }
    else {
        s_priv->swapchain_util->present(s_priv->cur_command_buffer);
    }
    return 0;
}

static int ngfx_query_draw_time(struct gctx *s, int64_t *time)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    *time = s_priv->profile_data.time;
    s_priv->profile_data.time = 0;
    return 0;
}

static void ngfx_wait_idle(struct gctx *s)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    if (s_priv->cur_command_buffer)
        s_priv->graphics->waitIdle(s_priv->cur_command_buffer);
}

static void ngfx_destroy(struct gctx *s)
{
    gctx_ngfx* ctx = (gctx_ngfx *)s;
    ngfx_wait_idle(s);
#if DEBUG_GPU_CAPTURE
    if (s->gpu_capture)
        gpu_capture_end(s->gpu_capture_ctx);
    gpu_capture_freep(&s->gpu_capture_ctx);
#endif
    auto output_color_texture = ((texture *)ctx->offscreen_resources.color_texture);
    auto output_color_resolve_texture = ((texture *)ctx->offscreen_resources.color_resolve_texture);
    auto output_depth_texture = ((texture *)ctx->offscreen_resources.depth_texture);
    auto dummy_texture = ctx->dummy_texture;
    if (output_depth_texture)
        ngli_texture_freep(&output_depth_texture);
    if (output_color_texture)
        ngli_texture_freep(&output_color_texture);
    if (output_color_resolve_texture)
        ngli_texture_freep(&output_color_resolve_texture);
    if (dummy_texture)
        ngli_texture_freep(&dummy_texture);
    if (ctx->default_rendertarget)
        ngli_rendertarget_freep(&ctx->default_rendertarget);
    if (ctx->swapchain_util)
        delete ctx->swapchain_util;
    if (ctx->surface)
        delete ctx->surface;
    delete ctx->graphics;
    delete ctx->graphics_context;
}

static int ngfx_transform_cull_mode(struct gctx *s, int cull_mode)
{
    return cull_mode;
}

static void ngfx_transform_projection_matrix(struct gctx *s, float *dst)
{
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f,
    };
    ngli_mat4_mul(dst, matrix, dst);
}

static void ngfx_get_rendertarget_uvcoord_matrix(struct gctx *s, float *dst)
{
#if defined(NGFX_GRAPHICS_BACKEND_DIRECT3D12)
    static const NGLI_ALIGNED_MAT(matrix) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f,-1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
#else
    static const NGLI_ALIGNED_MAT(matrix) = NGLI_MAT4_IDENTITY;
#endif
    memcpy(dst, matrix, sizeof(matrix));
}

static struct rendertarget *ngfx_get_default_rendertarget(struct gctx *s)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    return s_priv->default_rendertarget;
}

static const struct rendertarget_desc *ngfx_get_default_rendertarget_desc(struct gctx *s)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    return &s_priv->default_rendertarget_desc;
}

static void begin_render_pass(struct gctx_ngfx *s_priv, rendertarget_ngfx *rt_priv)
{
    Graphics *graphics = s_priv->graphics;
    GraphicsContext *ctx = s_priv->graphics_context;
    CommandBuffer *cmd_buf = s_priv->cur_command_buffer;
    if (!rt_priv) {
        // use ngfx default renderpass
        ctx->beginRenderPass(cmd_buf, graphics);
        return;
    }
    RenderPass *render_pass = rt_priv->render_pass;

    Framebuffer *framebuffer = rt_priv->output_framebuffer;
    auto &color_attachments = rt_priv->parent.params.colors;
    graphics->beginRenderPass(cmd_buf, render_pass, framebuffer,
        glm::make_vec4(color_attachments[0].clear_value));
}

static void end_render_pass(struct gctx_ngfx *s_priv, rendertarget_ngfx *)
{
    Graphics *graphics = s_priv->graphics;
    CommandBuffer *cmd_buf = s_priv->cur_command_buffer;
    if (graphics->currentRenderPass)
        graphics->endRenderPass(cmd_buf);
}

static void ngfx_begin_render_pass(struct gctx *s, struct rendertarget *rt)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    rendertarget_ngfx *rt_priv = (rendertarget_ngfx *)rt;
    if (rt_priv) {
        const auto &attachments = rt_priv->output_framebuffer->attachments;

        for (uint32_t j = 0; j<attachments.size(); j++) {
            auto output_texture = attachments[j].texture;
            if (output_texture->imageUsageFlags & IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
                output_texture->changeLayout(s_priv->cur_command_buffer, IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
            else if (output_texture->imageUsageFlags & IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                output_texture->changeLayout(s_priv->cur_command_buffer, IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }
        }
    }

    begin_render_pass(s_priv, rt_priv);

    s_priv->cur_rendertarget = rt;
}

static void ngfx_end_render_pass(struct gctx *s)
{
    gctx_ngfx *s_priv = (gctx_ngfx *)s;
    rendertarget_ngfx *rt_priv = (rendertarget_ngfx *)s_priv->cur_rendertarget;

    end_render_pass(s_priv, rt_priv);

    if (rt_priv) {
        const auto &attachments = rt_priv->output_framebuffer->attachments;
        for (uint32_t j = 0; j<attachments.size(); j++) {
            auto output_texture = attachments[j].texture;
            if (output_texture->imageUsageFlags & IMAGE_USAGE_SAMPLED_BIT) {
                ngli_assert(output_texture->numSamples == 1);
                output_texture->changeLayout(s_priv->cur_command_buffer, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }
    }
    s_priv->cur_rendertarget = NULL;
}

static void ngfx_set_viewport(struct gctx *s, const int *vp)
{
    struct gctx_ngfx *s_priv = (struct gctx_ngfx *)s;
    memcpy(s_priv->viewport, vp, sizeof(s_priv->viewport));
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
    .set_capture_buffer = ngfx_set_capture_buffer,
    .begin_update = ngfx_begin_update,
    .end_update   = ngfx_end_update,
    .begin_draw   = ngfx_begin_draw,
    .end_draw     = ngfx_end_draw,
    .query_draw_time = ngfx_query_draw_time,
    .wait_idle    = ngfx_wait_idle,
    .destroy      = ngfx_destroy,

    .transform_cull_mode = ngfx_transform_cull_mode,
    .transform_projection_matrix = ngfx_transform_projection_matrix,
    .get_rendertarget_uvcoord_matrix = ngfx_get_rendertarget_uvcoord_matrix,

    .get_default_rendertarget      = ngfx_get_default_rendertarget,
    .get_default_rendertarget_desc = ngfx_get_default_rendertarget_desc,

    .begin_render_pass        = ngfx_begin_render_pass,
    .end_render_pass          = ngfx_end_render_pass,

    .set_viewport             = ngfx_set_viewport,
    .get_viewport             = ngfx_get_viewport,
    .set_scissor              = ngfx_set_scissor,
    .get_scissor              = ngfx_get_scissor,

    .get_preferred_depth_format         = ngfx_get_preferred_depth_format,
    .get_preferred_depth_stencil_format = ngfx_get_preferred_depth_stencil_format,

    .buffer_create = ngli_buffer_ngfx_create,
    .buffer_init   = ngli_buffer_ngfx_init,
    .buffer_upload = ngli_buffer_ngfx_upload,
    .buffer_download = ngli_buffer_ngfx_download,
    .buffer_map      = ngli_buffer_ngfx_map,
    .buffer_unmap    = ngli_buffer_ngfx_unmap,
    .buffer_freep  = ngli_buffer_ngfx_freep,

    .pipeline_create            = ngli_pipeline_ngfx_create,
    .pipeline_init              = ngli_pipeline_ngfx_init,
    .pipeline_set_resources     = ngli_pipeline_ngfx_set_resources,
    .pipeline_update_attribute  = ngli_pipeline_ngfx_update_attribute,
    .pipeline_update_uniform    = ngli_pipeline_ngfx_update_uniform,
    .pipeline_update_texture    = ngli_pipeline_ngfx_update_texture,
    .pipeline_draw              = ngli_pipeline_ngfx_draw,
    .pipeline_draw_indexed      = ngli_pipeline_ngfx_draw_indexed,
    .pipeline_dispatch          = ngli_pipeline_ngfx_dispatch,
    .pipeline_freep             = ngli_pipeline_ngfx_freep,

    .program_create = ngli_program_ngfx_create,
    .program_init   = ngli_program_ngfx_init,
    .program_freep  = ngli_program_ngfx_freep,

    .rendertarget_create        = ngli_rendertarget_ngfx_create,
    .rendertarget_init          = ngli_rendertarget_ngfx_init,
    .rendertarget_read_pixels   = ngli_rendertarget_ngfx_read_pixels,
    .rendertarget_freep         = ngli_rendertarget_ngfx_freep,

    .texture_create           = ngli_texture_ngfx_create,
    .texture_init             = ngli_texture_ngfx_init,
    .texture_has_mipmap       = ngli_texture_ngfx_has_mipmap,
    .texture_match_dimensions = ngli_texture_ngfx_match_dimensions,
    .texture_upload           = ngli_texture_ngfx_upload,
    .texture_generate_mipmap  = ngli_texture_ngfx_generate_mipmap,
    .texture_freep            = ngli_texture_ngfx_freep,
};
