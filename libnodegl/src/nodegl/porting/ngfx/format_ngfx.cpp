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
#include "nodegl/core/format.h"
#include "nodegl/porting/ngfx/format_ngfx.h"
#include "nodegl/core/utils.h"
#include <map>
using namespace ngfx;
using namespace std;

int to_ngli_format(PixelFormat format)
{
    static const map<PixelFormat, int> format_map = {
        { PIXELFORMAT_UNDEFINED     , NGLI_FORMAT_UNDEFINED            },
        { PIXELFORMAT_R8_UNORM      , NGLI_FORMAT_R8_UNORM             },
        { PIXELFORMAT_RG8_UNORM     , NGLI_FORMAT_R8G8_UNORM           },
        { PIXELFORMAT_RGBA8_UNORM   , NGLI_FORMAT_R8G8B8A8_UNORM       },
        { PIXELFORMAT_R16_UINT      , NGLI_FORMAT_R16_UINT             },
        { PIXELFORMAT_RG16_UINT     , NGLI_FORMAT_R16G16_UINT          },
        { PIXELFORMAT_RGBA16_UINT   , NGLI_FORMAT_R16G16B16A16_UINT    },
        { PIXELFORMAT_R16_SFLOAT    , NGLI_FORMAT_R16_SFLOAT           },
        { PIXELFORMAT_RG16_SFLOAT   , NGLI_FORMAT_R16G16_SFLOAT        },
        { PIXELFORMAT_RGBA16_SFLOAT , NGLI_FORMAT_R16G16B16A16_SFLOAT  },
        { PIXELFORMAT_R32_UINT      , NGLI_FORMAT_R32_UINT             },
        { PIXELFORMAT_RG32_UINT     , NGLI_FORMAT_R32G32_UINT          },
        { PIXELFORMAT_RGBA32_UINT   , NGLI_FORMAT_R32G32B32A32_UINT    },
        { PIXELFORMAT_R32_SFLOAT    , NGLI_FORMAT_R32_SFLOAT           },
        { PIXELFORMAT_RG32_SFLOAT   , NGLI_FORMAT_R32G32_SFLOAT        },
        { PIXELFORMAT_RGBA32_SFLOAT , NGLI_FORMAT_R32G32B32A32_SFLOAT  },
        { PIXELFORMAT_BGRA8_UNORM   , NGLI_FORMAT_B8G8R8A8_UNORM       },
        { PIXELFORMAT_D16_UNORM     , NGLI_FORMAT_D16_UNORM            },
        { PIXELFORMAT_D24_UNORM_S8  , NGLI_FORMAT_D24_UNORM_S8_UINT    }
    };
    return format_map.at(format);
}

ngfx::PixelFormat to_ngfx_format(int format) {
    static const map<int, PixelFormat> format_map = {
        { NGLI_FORMAT_UNDEFINED            , PIXELFORMAT_UNDEFINED },
        { NGLI_FORMAT_R8_UNORM             , PIXELFORMAT_R8_UNORM },
        { NGLI_FORMAT_R8G8_UNORM           , PIXELFORMAT_RG8_UNORM },
        { NGLI_FORMAT_R8G8B8A8_UNORM       , PIXELFORMAT_RGBA8_UNORM },
        { NGLI_FORMAT_R16_UINT             , PIXELFORMAT_R16_UINT },
        { NGLI_FORMAT_R16G16_UINT          , PIXELFORMAT_RG16_UINT },
        { NGLI_FORMAT_R16G16B16A16_UINT    , PIXELFORMAT_RGBA16_UINT },
        { NGLI_FORMAT_R16_SFLOAT           , PIXELFORMAT_R16_SFLOAT },
        { NGLI_FORMAT_R16G16_SFLOAT        , PIXELFORMAT_RG16_SFLOAT },
        { NGLI_FORMAT_R16G16B16A16_SFLOAT  , PIXELFORMAT_RGBA16_SFLOAT },
        { NGLI_FORMAT_R32_UINT             , PIXELFORMAT_R32_UINT },
        { NGLI_FORMAT_R32G32_UINT          , PIXELFORMAT_RG32_UINT },
        { NGLI_FORMAT_R32G32B32A32_UINT    , PIXELFORMAT_RGBA32_UINT },
        { NGLI_FORMAT_R32_SFLOAT           , PIXELFORMAT_R32_SFLOAT },
        { NGLI_FORMAT_R32G32_SFLOAT        , PIXELFORMAT_RG32_SFLOAT },
        { NGLI_FORMAT_R32G32B32A32_SFLOAT  , PIXELFORMAT_RGBA32_SFLOAT },
        { NGLI_FORMAT_B8G8R8A8_UNORM       , PIXELFORMAT_BGRA8_UNORM },
        { NGLI_FORMAT_D16_UNORM            , PIXELFORMAT_D16_UNORM },
        { NGLI_FORMAT_D24_UNORM_S8_UINT    , PIXELFORMAT_D24_UNORM_S8 }
    };
    return format_map.at(format);
}

bool is_depth_format(int format)
{
    switch (format) {
    case NGLI_FORMAT_D16_UNORM:
    case NGLI_FORMAT_D24_UNORM_S8_UINT:
        return 1;
    default:
        return 0;
    }
}

