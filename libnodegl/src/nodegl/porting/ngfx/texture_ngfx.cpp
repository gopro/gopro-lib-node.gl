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

#include "nodegl/porting/ngfx/texture_ngfx.h"
#include "nodegl/core/log.h"
#include "nodegl/core/memory.h"
#include <map>
#include "ngfx/graphics/Texture.h"
#include "nodegl/porting/ngfx/gctx_ngfx.h"
#include "nodegl/porting/ngfx/util_ngfx.h"
using namespace ngfx;
using namespace std;

struct texture *ngli_texture_ngfx_create(struct gctx *gctx) {
    texture_ngfx *s = (texture_ngfx*)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct texture *)s;
}

int ngli_texture_ngfx_init(struct texture *s,
                           const struct texture_params *p) {
    struct texture_ngfx *s_priv = (struct texture_ngfx *)s;
    struct gctx_ngfx *ctx = (struct gctx_ngfx *)s->gctx;
    auto &gctx = ctx->graphics_context;
    s->params = *p;
    s->bytes_per_pixel = get_bpp(p->format);
    bool gen_mipmaps = p->mipmap_filter != NGLI_MIPMAP_FILTER_NONE;
    uint32_t image_usage_flags = to_ngfx_image_usage_flags(p->usage);

    uint32_t depth = (p->type == NGLI_TEXTURE_TYPE_3D) ? p->depth : 1;
    uint32_t array_layers = (p->type == NGLI_TEXTURE_TYPE_CUBE) ? 6 : 1;
    uint32_t size = s->bytes_per_pixel * p->width * p->height * depth * array_layers;
    s_priv->v = Texture::create(gctx, ctx->graphics,
        nullptr, to_ngfx_format(p->format), size, p->width, p->height, depth, array_layers,
        image_usage_flags, to_ngfx_texture_type(p->type), gen_mipmaps,
        to_ngfx_filter_mode(p->min_filter), to_ngfx_filter_mode(p->mag_filter),
        gen_mipmaps ? to_ngfx_mip_filter_mode(p->mipmap_filter) : FILTER_NEAREST,
        p->samples == 0 ? 1 : p->samples
    );

    return 0;
}

int ngli_texture_ngfx_has_mipmap(const struct texture *s) {
    return s->params.mipmap_filter != NGLI_MIPMAP_FILTER_NONE;
}

int ngli_texture_ngfx_match_dimensions(const struct texture *s, int width, int height, int depth) {
    const texture_params *params = &s->params;
    return params->width == width && params->height == height && params->depth == depth;
}

int ngli_texture_ngfx_upload(struct texture *s, const uint8_t *data, int linesize) {
    texture_ngfx *texture = (struct texture_ngfx *)s;
    uint32_t size = s->bytes_per_pixel * texture->v->w * texture->v->h * texture->v->d * texture->v->arrayLayers;
    texture->v->upload((void*)data, size, 0, 0, 0, texture->v->w, texture->v->h, texture->v->d, texture->v->arrayLayers);
    return 0;
}
int ngli_texture_ngfx_generate_mipmap(struct texture *s) {
    texture_ngfx *texture = (struct texture_ngfx *)s;
    gctx_ngfx *gctx = (gctx_ngfx *)s->gctx;
    CommandBuffer *cmd_buffer = gctx->cur_command_buffer;
    texture->v->generateMipmaps(cmd_buffer);
    if (texture->v->imageUsageFlags & IMAGE_USAGE_SAMPLED_BIT) {
        texture->v->changeLayout(cmd_buffer, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    return 0;
}

void ngli_texture_ngfx_freep(struct texture **sp) {
    if (!sp) return;
    texture *s = *sp;
    texture_ngfx *s_priv = (struct texture_ngfx *)s;
    delete s_priv->v;
    ngli_freep(sp);
}

