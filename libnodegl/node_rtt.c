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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "rendertarget.h"
#include "format.h"
#include "gctx.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

struct rtt_priv {
    struct ngl_node *child;
    struct ngl_node **color_textures;
    int nb_color_textures;
    struct ngl_node *depth_texture;
    int samples;
    float clear_color[4];
    int features;
    int vflip;

    int invalidate_depth_stencil;
    int width;
    int height;

    struct rendertarget *rt;
    struct texture *depth;

    struct texture *ms_colors[NGLI_MAX_COLOR_ATTACHMENTS];
    int nb_ms_colors;
    struct texture *ms_depth;
};

#define FEATURE_DEPTH       (1 << 0)
#define FEATURE_STENCIL     (1 << 1)
#define FEATURE_NO_CLEAR    (1 << 2)

static const struct param_choices feature_choices = {
    .name = "framebuffer_features",
    .consts = {
        {"depth",   FEATURE_DEPTH,   .desc=NGLI_DOCSTRING("add depth buffer")},
        {"stencil", FEATURE_STENCIL, .desc=NGLI_DOCSTRING("add stencil buffer")},
        {"no_clear",FEATURE_NO_CLEAR,.desc=NGLI_DOCSTRING("not cleared between draws (non-deterministic)")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct rtt_priv, x)
static const struct node_param rtt_params[] = {
    {"child",         PARAM_TYPE_NODE, OFFSET(child),
                      .flags=PARAM_FLAG_NON_NULL,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_textures` and optionally to `depth_texture`")},
    {"color_textures", PARAM_TYPE_NODELIST, OFFSET(color_textures),
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, NGL_NODE_TEXTURECUBE, -1},
                      .desc=NGLI_DOCSTRING("destination color texture")},
    {"depth_texture", PARAM_TYPE_NODE, OFFSET(depth_texture),
                      .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
                      .desc=NGLI_DOCSTRING("destination depth (and potentially combined stencil) texture")},
    {"samples",       PARAM_TYPE_INT, OFFSET(samples),
                      .desc=NGLI_DOCSTRING("number of samples used for multisampling anti-aliasing")},
    {"clear_color",   PARAM_TYPE_VEC4, OFFSET(clear_color),
                      .desc=NGLI_DOCSTRING("color used to clear the `color_texture`")},
    {"features",      PARAM_TYPE_FLAGS, OFFSET(features),
                      .choices=&feature_choices,
                      .desc=NGLI_DOCSTRING("framebuffer feature mask")},
    {"vflip",         PARAM_TYPE_BOOL, OFFSET(vflip), {.i64=1},
                      .desc=NGLI_DOCSTRING("apply a vertical flip to `color_texture` and `depth_texture` transformation matrices to match the `node.gl` uv coordinates system")},
    {NULL}
};

static int rtt_init(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    for (int i = 0; i < s->nb_color_textures; i++) {
        const struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        if (texture_priv->data_src) {
            LOG(ERROR, "render targets cannot have a data source");
            return NGL_ERROR_INVALID_ARG;
        }
    }

    if (s->depth_texture) {
        const struct texture_priv *texture_priv = s->depth_texture->priv_data;
        if (texture_priv->data_src) {
            LOG(ERROR, "render targets cannot have a data source");
            return NGL_ERROR_INVALID_ARG;
        }
    }

    return 0;
}

#ifdef DEBUG_SCENE
struct renderpass_children_info {
    int has_rtt_or_compute;
    int render_counts[2]; // number of render nodes before and after the first rtt or compute node (renderpass interruption)
};

static void get_renderpass_children_info(const struct ngl_node *node, struct renderpass_children_info *info)
{
    const struct ngl_node **children = ngli_darray_data(&node->children);
    for (int i = 0; i < ngli_darray_count(&node->children); i++) {
        const struct ngl_node *child = children[i];
        if (child->class->id == NGL_NODE_RENDERTOTEXTURE ||
            child->class->id == NGL_NODE_COMPUTE) {
            info->has_rtt_or_compute = 1;
        } else if (child->class->id == NGL_NODE_RENDER) {
            info->render_counts[info->has_rtt_or_compute ? 1 : 0] += 1;
        } else {
            get_renderpass_children_info(child, info);
        }
    }
}
#endif

static int rtt_prepare(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct rnode *rnode = ctx->rnode_pos;
    struct rtt_priv *s = node->priv_data;

#ifdef DEBUG_SCENE
    struct renderpass_children_info info = {0};
    get_renderpass_children_info(s->child, &info);
    if (info.render_counts[0] && info.render_counts[1])
        LOG(WARNING, "the underlying render pass might not be optimal as it contains a rtt or compute node in the middle of it");
#endif

    struct rendertarget_desc desc = {0};
    for (int i = 0; i < s->nb_color_textures; i++) {
        const struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        const struct texture_params *params = &texture_priv->params;
        const int faces = params->type == NGLI_TEXTURE_TYPE_CUBE ? 6 : 1;
        for (int j = 0; j < faces; j++) {
            desc.colors[desc.nb_colors].format = params->format;
            desc.colors[desc.nb_colors].samples = s->samples;
            desc.colors[desc.nb_colors].resolve = s->samples > 1;
            desc.nb_colors++;
        }
    }
    if (s->depth_texture) {
        const struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
        const struct texture_params *depth_texture_params = &depth_texture_priv->params;
        desc.depth_stencil.format = depth_texture_params->format;
        desc.depth_stencil.samples = s->samples;
        desc.depth_stencil.resolve = s->samples > 1;
    } else {
        int depth_format = NGLI_FORMAT_UNDEFINED;
        if (s->features & FEATURE_STENCIL)
            depth_format = ngli_gctx_get_preferred_depth_stencil_format(gctx);
        else if (s->features & FEATURE_DEPTH)
            depth_format = ngli_gctx_get_preferred_depth_format(gctx);
        desc.depth_stencil.format = depth_format;
        desc.depth_stencil.samples = s->samples;
    }
    rnode->rendertarget_desc = desc;

    return ngli_node_prepare(s->child);
}

static int rtt_prefetch(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct rtt_priv *s = node->priv_data;

    if (!(gctx->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) && s->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, "
            "multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (!s->nb_color_textures) {
        LOG(ERROR, "at least one color texture must be specified");
        return NGL_ERROR_INVALID_ARG;
    }

    if (s->nb_color_textures > NGLI_MAX_COLOR_ATTACHMENTS) {
        LOG(ERROR, "context does not support more than %d color attachments", NGLI_MAX_COLOR_ATTACHMENTS);
        return NGL_ERROR_UNSUPPORTED;
    }

    for (int i = 0; i < s->nb_color_textures; i++) {
        const struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        const struct texture *texture = texture_priv->texture;
        const struct texture_params *params = &texture->params;
        if (i == 0) {
            s->width = params->width;
            s->height = params->height;
        } else if (s->width != params->width || s->height != params->height) {
            LOG(ERROR, "all color texture dimensions do not match %dx%d != %dx%d",
            s->width, s->height, params->width, params->height);
            return NGL_ERROR_INVALID_ARG;
        }
    }

    if (s->depth_texture) {
        const struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
        const struct texture *depth_texture = depth_texture_priv->texture;
        const struct texture_params *depth_texture_params = &depth_texture->params;
        if (s->width != depth_texture_params->width || s->height != depth_texture_params->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, depth_texture_params->width, depth_texture_params->height);
            return NGL_ERROR_INVALID_ARG;
        }
    }

    struct rendertarget_params rt_params = {
        .width = s->width,
        .height = s->height,
    };

    for (int i = 0; i < s->nb_color_textures; i++) {
        struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        struct texture *texture = texture_priv->texture;
        struct texture_params *params = &texture->params;
        const int n = params->type == NGLI_TEXTURE_TYPE_CUBE ? 6 : 1;
        for (int j = 0; j < n; j++) {
            if (s->samples) {
                struct texture *ms_texture = ngli_texture_create(gctx);
                if (!ms_texture)
                    return NGL_ERROR_MEMORY;
                s->ms_colors[s->nb_ms_colors++] = ms_texture;
                struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
                attachment_params.format = params->format;
                attachment_params.width = s->width;
                attachment_params.height = s->height;
                attachment_params.samples = s->samples;
                attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
                ret = ngli_texture_init(ms_texture, &attachment_params);
                if (ret < 0)
                    return ret;
                rt_params.colors[rt_params.nb_colors].attachment = ms_texture;
                rt_params.colors[rt_params.nb_colors].attachment_layer = 0;
                rt_params.colors[rt_params.nb_colors].resolve_target = texture;
                rt_params.colors[rt_params.nb_colors].resolve_target_layer = j;
            } else {
                rt_params.colors[rt_params.nb_colors].attachment = texture;
                rt_params.colors[rt_params.nb_colors].attachment_layer = j;
            }
            rt_params.nb_colors++;
        }
    }

    int depth_format = NGLI_FORMAT_UNDEFINED;
    if (s->depth_texture) {
        struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
        struct texture *texture = depth_texture_priv->texture;
        struct texture_params *params = &texture->params;

        if (s->samples) {
            struct texture *ms_texture = ngli_texture_create(gctx);
            if (!ms_texture)
                return NGL_ERROR_MEMORY;
            s->ms_depth = ms_texture;
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = params->format;
            attachment_params.width = s->width;
            attachment_params.height = s->height;
            attachment_params.samples = s->samples;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            ret = ngli_texture_init(ms_texture, &attachment_params);
            if (ret < 0)
                return ret;
            rt_params.depth_stencil.attachment = ms_texture;
            rt_params.depth_stencil.resolve_target = texture;
        } else {
            rt_params.depth_stencil.attachment = texture;
        }
    } else {
        if (s->features & FEATURE_STENCIL)
            depth_format = ngli_gctx_get_preferred_depth_stencil_format(gctx);
        else if (s->features & FEATURE_DEPTH)
            depth_format = ngli_gctx_get_preferred_depth_format(gctx);

        if (depth_format != NGLI_FORMAT_UNDEFINED) {
            struct texture *depth = ngli_texture_create(gctx);
            if (!depth)
                return NGL_ERROR_MEMORY;
            s->depth = depth;
            struct texture_params attachment_params = NGLI_TEXTURE_PARAM_DEFAULTS;
            attachment_params.format = depth_format;
            attachment_params.width = s->width;
            attachment_params.height = s->height;
            attachment_params.samples = s->samples;
            attachment_params.usage = NGLI_TEXTURE_USAGE_ATTACHMENT_ONLY;
            ret = ngli_texture_init(depth, &attachment_params);
            if (ret < 0)
                return ret;
            rt_params.depth_stencil.attachment = depth;

            if (!(s->features & FEATURE_NO_CLEAR))
                s->invalidate_depth_stencil = 1;
        }
    }

    s->rt = ngli_rendertarget_create(gctx);
    if (!s->rt)
        return NGL_ERROR_MEMORY;
    ret = ngli_rendertarget_init(s->rt, &rt_params);
    if (ret < 0)
        return ret;

    if (s->vflip) {
        /* flip vertically the color and depth textures so the coordinates
         * match how the uv coordinates system works */
        for (int i = 0; i < s->nb_color_textures; i++) {
            struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
            struct image *image = &texture_priv->image;
            ngli_gctx_get_rendertarget_uvcoord_matrix(gctx, image->coordinates_matrix);
        }

        if (s->depth_texture) {
            struct texture_priv *depth_texture_priv = s->depth_texture->priv_data;
            struct image *depth_image = &depth_texture_priv->image;
            ngli_gctx_get_rendertarget_uvcoord_matrix(gctx, depth_image->coordinates_matrix);
        }
    }

    return 0;
}

static int rtt_update(struct ngl_node *node, double t)
{
    struct rtt_priv *s = node->priv_data;
    int ret = ngli_node_update(s->child, t);
    if (ret < 0)
        return ret;

    for (int i = 0; i < s->nb_color_textures; i++) {
        ret = ngli_node_update(s->color_textures[i], t);
        if (ret < 0)
            return ret;
    }

    if (s->depth_texture) {
        ret = ngli_node_update(s->depth_texture, t);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gctx *gctx = ctx->gctx;
    struct rtt_priv *s = node->priv_data;

    int prev_vp[4] = {0};
    ngli_gctx_get_viewport(gctx, prev_vp);

    const int vp[4] = {0, 0, s->width, s->height};
    ngli_gctx_set_viewport(gctx, vp);

    float prev_clear_color[4] = {0};
    ngli_gctx_get_clear_color(gctx, prev_clear_color);
    ngli_gctx_set_clear_color(gctx, s->clear_color);

    struct rendertarget *prev_rendertarget = ctx->current_rendertarget;
    int prev_clear = ctx->clear_current_rendertarget;

    ctx->current_rendertarget = s->rt;
    ctx->bind_current_rendertarget = 1;
    ctx->clear_current_rendertarget = !(s->features & FEATURE_NO_CLEAR);

    ngli_node_draw(s->child);

    if (ctx->bind_current_rendertarget) {
        ngli_gctx_set_rendertarget(gctx, ctx->current_rendertarget);
        if (ctx->clear_current_rendertarget) {
            ngli_gctx_clear_color(gctx);
            ngli_gctx_clear_depth_stencil(gctx);
        }
        ctx->bind_current_rendertarget = 0;
        ctx->clear_current_rendertarget = 0;
    }

    if (s->samples > 0)
        ngli_rendertarget_resolve(ctx->current_rendertarget);

    if (s->invalidate_depth_stencil)
        ngli_gctx_invalidate_depth_stencil(gctx);

    ctx->current_rendertarget = prev_rendertarget;
    ctx->bind_current_rendertarget = 1;
    ctx->clear_current_rendertarget = prev_clear;

    ngli_gctx_set_viewport(gctx, prev_vp);
    ngli_gctx_set_clear_color(gctx, prev_clear_color);

    for (int i = 0; i < s->nb_color_textures; i++) {
        struct texture_priv *texture_priv = s->color_textures[i]->priv_data;
        struct texture *texture = texture_priv->texture;
        if (ngli_texture_has_mipmap(texture))
            ngli_texture_generate_mipmap(texture);
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    ngli_rendertarget_freep(&s->rt);
    ngli_texture_freep(&s->depth);

    for (int i = 0; i < s->nb_ms_colors; i++)
        ngli_texture_freep(&s->ms_colors[i]);
    s->nb_ms_colors = 0;
    ngli_texture_freep(&s->ms_depth);
}

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RENDERTOTEXTURE,
    .name      = "RenderToTexture",
    .init      = rtt_init,
    .prepare   = rtt_prepare,
    .prefetch  = rtt_prefetch,
    .update    = rtt_update,
    .draw      = rtt_draw,
    .release   = rtt_release,
    .priv_size = sizeof(struct rtt_priv),
    .params    = rtt_params,
    .file      = __FILE__,
};
