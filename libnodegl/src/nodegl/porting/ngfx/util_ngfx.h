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

#ifndef UTIL_NGFX_H
#define UTIL_NGFX_H
#include "graphics/GraphicsCore.h"
#include "graphics/GraphicsContext.h"
#include "nodegl/porting/ngfx/format_ngfx.h"
#include "nodegl/porting/ngfx/topology_ngfx.h"
#include "nodegl/rendertarget.h"

ngfx::FilterMode to_ngfx_filter_mode(int filter);
ngfx::FilterMode to_ngfx_mip_filter_mode(int filter);
ngfx::TextureType to_ngfx_texture_type(int type);
ngfx::IndexFormat to_ngfx_index_format(int indices_format);
ngfx::BlendFactor to_ngfx_blend_factor(int blend_factor);
ngfx::BlendOp to_ngfx_blend_op(int blend_op);
ngfx::ColorComponentFlags to_ngfx_color_mask(int color_write_mask);
ngfx::CullModeFlags to_ngfx_cull_mode(int cull_mode);
ngfx::ImageUsageFlags to_ngfx_image_usage_flags(int usage_flags);
ngfx::RenderPass* get_render_pass(ngfx::GraphicsContext* ctx, const rendertarget_desc &rt_desc);

inline uint32_t get_bpp(int format) {
    return ngli_format_get_bytes_per_pixel(format);
}

#endif
