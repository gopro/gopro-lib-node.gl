/*
 * Copyright 2019 GoPro Inc.
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

#include "nodegl/porting/ngfx/util_ngfx.h"
#include "nodegl/core/texture.h"
#include "nodegl/core/graphicstate.h"
#include <map>
using namespace ngfx;
using namespace std;

FilterMode to_ngfx_filter_mode(int filter) {
    static const map<int, ngfx::FilterMode> filter_map = {
        { NGLI_FILTER_NEAREST, FILTER_NEAREST },
        { NGLI_FILTER_LINEAR, FILTER_LINEAR }
    };
    return filter_map.at(filter);
}

FilterMode to_ngfx_mip_filter_mode(int filter) {
    static const map<int, ngfx::FilterMode> mip_filter_map = {
        { NGLI_MIPMAP_FILTER_NEAREST, FILTER_NEAREST},
        { NGLI_MIPMAP_FILTER_LINEAR, FILTER_LINEAR }
    };
    return mip_filter_map.at(filter);
}

TextureType to_ngfx_texture_type(int type) {
    static const map<int, TextureType> texture_type_map = {
        { NGLI_TEXTURE_TYPE_2D, TEXTURE_TYPE_2D },
        { NGLI_TEXTURE_TYPE_3D, TEXTURE_TYPE_3D },
        { NGLI_TEXTURE_TYPE_CUBE, TEXTURE_TYPE_CUBE }
    };
    return texture_type_map.at(type);
}

IndexFormat to_ngfx_index_format(int indices_format)
{
    static const std::map<int, IndexFormat> index_format_map = {
        { NGLI_FORMAT_R16_UNORM, INDEXFORMAT_UINT16 },
        { NGLI_FORMAT_R32_UINT,  INDEXFORMAT_UINT32 }
    };
    return index_format_map.at(indices_format);
}

BlendFactor to_ngfx_blend_factor(int blend_factor)
{
    static const std::map<int, BlendFactor> blend_factor_map = {
        { NGLI_BLEND_FACTOR_ZERO                , BLEND_FACTOR_ZERO },
        { NGLI_BLEND_FACTOR_ONE                 , BLEND_FACTOR_ONE },
        { NGLI_BLEND_FACTOR_SRC_COLOR           , BLEND_FACTOR_SRC_COLOR },
        { NGLI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR , BLEND_FACTOR_ONE_MINUS_SRC_COLOR },
        { NGLI_BLEND_FACTOR_DST_COLOR           , BLEND_FACTOR_DST_COLOR },
        { NGLI_BLEND_FACTOR_ONE_MINUS_DST_COLOR , BLEND_FACTOR_ONE_MINUS_DST_COLOR },
        { NGLI_BLEND_FACTOR_SRC_ALPHA           , BLEND_FACTOR_SRC_ALPHA },
        { NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA , BLEND_FACTOR_ONE_MINUS_SRC_ALPHA },
        { NGLI_BLEND_FACTOR_DST_ALPHA           , BLEND_FACTOR_DST_ALPHA },
        { NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA , BLEND_FACTOR_ONE_MINUS_DST_ALPHA }
    };

    return blend_factor_map.at(blend_factor);
}

BlendOp to_ngfx_blend_op(int blend_op)
{
    static const std::map<int, BlendOp> blend_op_map = {
        { NGLI_BLEND_OP_ADD              , BLEND_OP_ADD },
        { NGLI_BLEND_OP_SUBTRACT         , BLEND_OP_SUBTRACT },
        { NGLI_BLEND_OP_REVERSE_SUBTRACT , BLEND_OP_REVERSE_SUBTRACT },
        { NGLI_BLEND_OP_MIN              , BLEND_OP_MIN },
        { NGLI_BLEND_OP_MAX              , BLEND_OP_MAX },
    };
    return blend_op_map.at(blend_op);
}

ColorComponentFlags to_ngfx_color_mask(int color_write_mask)
{
    return (color_write_mask & NGLI_COLOR_COMPONENT_R_BIT ? COLOR_COMPONENT_R_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_G_BIT ? COLOR_COMPONENT_G_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_B_BIT ? COLOR_COMPONENT_B_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_A_BIT ? COLOR_COMPONENT_A_BIT : 0);
}

CullModeFlags to_ngfx_cull_mode(int cull_mode)
{
    static std::map<int, CullModeFlags> cull_mode_map = {
        { NGLI_CULL_MODE_NONE          , CULL_MODE_NONE },
        { NGLI_CULL_MODE_FRONT_BIT     , CULL_MODE_FRONT_BIT },
        { NGLI_CULL_MODE_BACK_BIT      , CULL_MODE_BACK_BIT },
    };
    return cull_mode_map.at(cull_mode);
}

ngfx::ImageUsageFlags to_ngfx_image_usage_flags(int usage_flags) {
    static std::map<int, ImageUsageFlags> usage_flags_map = {
        { NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT               , IMAGE_USAGE_TRANSFER_SRC_BIT },
        { NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT               , IMAGE_USAGE_TRANSFER_DST_BIT },
        { NGLI_TEXTURE_USAGE_SAMPLED_BIT                    , IMAGE_USAGE_SAMPLED_BIT },
        { NGLI_TEXTURE_USAGE_STORAGE_BIT                    , IMAGE_USAGE_STORAGE_BIT },
        { NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT           , IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
        { NGLI_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT   , IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT },
        { NGLI_TEXTURE_USAGE_TRANSIENT_ATTACHMENT_BIT       , IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT }
    };
    int image_usage_flags = 0;
    for (auto& flag : usage_flags_map) {
        if (usage_flags & flag.first) image_usage_flags |= flag.second;
    }
    return image_usage_flags;
}

RenderPass* get_render_pass(GraphicsContext* ctx, const rendertarget_desc &rt_desc) {
    GraphicsContext::RenderPassConfig renderPassConfig;
    renderPassConfig.enableDepthStencil = rt_desc.depth_stencil.format != NGLI_FORMAT_UNDEFINED;
    renderPassConfig.enableDepthStencilResolve = rt_desc.depth_stencil.resolve != 0;
    renderPassConfig.numColorAttachments = rt_desc.nb_colors;
    renderPassConfig.numSamples = glm::max(rt_desc.colors[0].samples, 1);
    //TODO: remove renderPassConfig.offscreen param
    renderPassConfig.offscreen = true;
    return ctx->getRenderPass(renderPassConfig);
}
