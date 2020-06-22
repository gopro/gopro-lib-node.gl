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

#include <limits.h>
#include <string.h>
#include <stddef.h>

#include "config.h"
#include "gctx.h"
#include "format.h"
#include "hmap.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "pgcraft.h"
#include "precision.h"
#include "type.h"

/*
 * Currently unmapped formats: r11f_g11f_b10f, rgb10_a2, rgb10_a2ui
 */
static const char *image_glsl_format_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R8_UNORM]             = "r8",
    [NGLI_FORMAT_R8_SNORM]             = "r8_snorm",
    [NGLI_FORMAT_R8_UINT]              = "r8ui",
    [NGLI_FORMAT_R8_SINT]              = "r8i",
    [NGLI_FORMAT_R8G8_UNORM]           = "rg8",
    [NGLI_FORMAT_R8G8_SNORM]           = "rg8_snorm",
    [NGLI_FORMAT_R8G8_UINT]            = "rg8ui",
    [NGLI_FORMAT_R8G8_SINT]            = "rg8i",
    [NGLI_FORMAT_R8G8B8_UNORM]         = NULL,
    [NGLI_FORMAT_R8G8B8_SNORM]         = NULL,
    [NGLI_FORMAT_R8G8B8_UINT]          = NULL,
    [NGLI_FORMAT_R8G8B8_SINT]          = NULL,
    [NGLI_FORMAT_R8G8B8_SRGB]          = NULL,
    [NGLI_FORMAT_R8G8B8A8_UNORM]       = "rgba8",
    [NGLI_FORMAT_R8G8B8A8_SNORM]       = "rgba8_snorm",
    [NGLI_FORMAT_R8G8B8A8_UINT]        = "rgba8ui",
    [NGLI_FORMAT_R8G8B8A8_SINT]        = "rgba8i",
    [NGLI_FORMAT_R8G8B8A8_SRGB]        = NULL,
    [NGLI_FORMAT_B8G8R8A8_UNORM]       = "rgba8",
    [NGLI_FORMAT_B8G8R8A8_SNORM]       = "rgba8_snorm",
    [NGLI_FORMAT_B8G8R8A8_UINT]        = "rgba8ui",
    [NGLI_FORMAT_B8G8R8A8_SINT]        = "rgba8i",
    [NGLI_FORMAT_R16_UNORM]            = "r16",
    [NGLI_FORMAT_R16_SNORM]            = "r16_snorm",
    [NGLI_FORMAT_R16_UINT]             = "r16ui",
    [NGLI_FORMAT_R16_SINT]             = "r16i",
    [NGLI_FORMAT_R16_SFLOAT]           = "r16f",
    [NGLI_FORMAT_R16G16_UNORM]         = "rg16",
    [NGLI_FORMAT_R16G16_SNORM]         = "rg16_snorm",
    [NGLI_FORMAT_R16G16_UINT]          = "rg16ui",
    [NGLI_FORMAT_R16G16_SINT]          = "rg16i",
    [NGLI_FORMAT_R16G16_SFLOAT]        = "rg16f",
    [NGLI_FORMAT_R16G16B16_UNORM]      = NULL,
    [NGLI_FORMAT_R16G16B16_SNORM]      = NULL,
    [NGLI_FORMAT_R16G16B16_UINT]       = NULL,
    [NGLI_FORMAT_R16G16B16_SINT]       = NULL,
    [NGLI_FORMAT_R16G16B16_SFLOAT]     = NULL,
    [NGLI_FORMAT_R16G16B16A16_UNORM]   = "rgba16",
    [NGLI_FORMAT_R16G16B16A16_SNORM]   = "rgba16_snorm",
    [NGLI_FORMAT_R16G16B16A16_UINT]    = "rgba16ui",
    [NGLI_FORMAT_R16G16B16A16_SINT]    = "rgba16i",
    [NGLI_FORMAT_R16G16B16A16_SFLOAT]  = "rgba16f",
    [NGLI_FORMAT_R32_UINT]             = "r32ui",
    [NGLI_FORMAT_R32_SINT]             = "r32i",
    [NGLI_FORMAT_R32_SFLOAT]           = "r32f",
    [NGLI_FORMAT_R32G32_UINT]          = "rg32ui",
    [NGLI_FORMAT_R32G32_SINT]          = "rg32i",
    [NGLI_FORMAT_R32G32_SFLOAT]        = "rg32f",
    [NGLI_FORMAT_R32G32B32_UINT]       = NULL,
    [NGLI_FORMAT_R32G32B32_SINT]       = NULL,
    [NGLI_FORMAT_R32G32B32_SFLOAT]     = NULL,
    [NGLI_FORMAT_R32G32B32A32_UINT]    = "rgba32ui",
    [NGLI_FORMAT_R32G32B32A32_SINT]    = "rgba32i",
    [NGLI_FORMAT_R32G32B32A32_SFLOAT]  = "rgba32f",
    [NGLI_FORMAT_D16_UNORM]            = NULL,
    [NGLI_FORMAT_X8_D24_UNORM_PACK32]  = NULL,
    [NGLI_FORMAT_D32_SFLOAT]           = NULL,
    [NGLI_FORMAT_D24_UNORM_S8_UINT]    = NULL,
    [NGLI_FORMAT_D32_SFLOAT_S8_UINT]   = NULL,
    [NGLI_FORMAT_S8_UINT]              = NULL,
};

enum {
    TYPE_FLAG_IS_SAMPLER_OR_IMAGE = 1 << 0,
    TYPE_FLAG_HAS_PRECISION       = 1 << 1,
    TYPE_FLAG_IS_INT              = 1 << 2,
};

static const struct {
    int flags;
    const char *glsl_type;
} type_info_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_INT]                         = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "int"},
    [NGLI_TYPE_IVEC2]                       = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "ivec2"},
    [NGLI_TYPE_IVEC3]                       = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "ivec3"},
    [NGLI_TYPE_IVEC4]                       = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "ivec4"},
    [NGLI_TYPE_UINT]                        = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "uint"},
    [NGLI_TYPE_UIVEC2]                      = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "uvec2"},
    [NGLI_TYPE_UIVEC3]                      = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "uvec3"},
    [NGLI_TYPE_UIVEC4]                      = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT, "uvec4"},
    [NGLI_TYPE_FLOAT]                       = {TYPE_FLAG_HAS_PRECISION, "float"},
    [NGLI_TYPE_VEC2]                        = {TYPE_FLAG_HAS_PRECISION, "vec2"},
    [NGLI_TYPE_VEC3]                        = {TYPE_FLAG_HAS_PRECISION, "vec3"},
    [NGLI_TYPE_VEC4]                        = {TYPE_FLAG_HAS_PRECISION, "vec4"},
    [NGLI_TYPE_MAT3]                        = {TYPE_FLAG_HAS_PRECISION, "mat3"},
    [NGLI_TYPE_MAT4]                        = {TYPE_FLAG_HAS_PRECISION, "mat4"},
    [NGLI_TYPE_BOOL]                        = {0, "bool"},
    [NGLI_TYPE_SAMPLER_2D]                  = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "sampler2D"},
    [NGLI_TYPE_SAMPLER_2D_RECT]             = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "sampler2DRect"},
    [NGLI_TYPE_SAMPLER_3D]                  = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "sampler3D"},
    [NGLI_TYPE_SAMPLER_CUBE]                = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "samplerCube"},
    [NGLI_TYPE_SAMPLER_EXTERNAL_OES]        = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "samplerExternalOES"},
    [NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT] = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "__samplerExternal2DY2YEXT"},
    [NGLI_TYPE_IMAGE_2D]                    = {TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE, "image2D"},
    [NGLI_TYPE_UNIFORM_BUFFER]              = {0, "uniform"},
    [NGLI_TYPE_STORAGE_BUFFER]              = {0, "buffer"},
};

static int is_sampler_or_image(int type)
{
    return type_info_map[type].flags & TYPE_FLAG_IS_SAMPLER_OR_IMAGE;
}

static int type_has_precision(int type)
{
    return type_info_map[type].flags & TYPE_FLAG_HAS_PRECISION;
}

static int type_is_int(int type)
{
    return type_info_map[type].flags & TYPE_FLAG_IS_INT;
}

static const char *get_glsl_type(int type)
{
    const char *ret = type_info_map[type].glsl_type;
    ngli_assert(ret);
    return ret;
}

static const char *get_precision_qualifier(const struct pgcraft *s, int type, int precision, const char *defaultp)
{
    if (!s->has_precision_qualifiers || !type_has_precision(type))
        return "";
    static const char *precision_qualifiers[NGLI_PRECISION_NB] = {
        [NGLI_PRECISION_AUTO]   = NULL,
        [NGLI_PRECISION_HIGH]   = "highp",
        [NGLI_PRECISION_MEDIUM] = "mediump",
        [NGLI_PRECISION_LOW]    = "lowp",
    };
    const char *ret = precision_qualifiers[precision];
    return ret ? ret : defaultp;
}

static int inject_block_uniform(struct pgcraft *s, struct bstr *b,
                                const struct pgcraft_uniform *uniform, int stage)
{
    struct block *block = &s->ublock[stage];

    /* Lazily initialize the block containing the uniforms */
    if (!block->size)
        ngli_block_init(block, NGLI_BLOCK_LAYOUT_STD140);

    return ngli_block_add_field(block, uniform->name, uniform->type, uniform->count);
}

static int inject_uniform(struct pgcraft *s, struct bstr *b,
                          const struct pgcraft_uniform *uniform, int stage)
{
    if (uniform->stage != stage)
        return 0;

    if (s->use_ublock)
        return inject_block_uniform(s, b, uniform, stage);

    struct pipeline_uniform_desc pl_uniform_desc = {
        .type  = uniform->type,
        .count = NGLI_MAX(uniform->count, 1),
    };
    snprintf(pl_uniform_desc.name, sizeof(pl_uniform_desc.name), "%s", uniform->name);

    const char *type = get_glsl_type(uniform->type);
    const char *precision = get_precision_qualifier(s, uniform->type, uniform->precision, "highp");
    if (uniform->count)
        ngli_bstr_printf(b, "uniform %s %s %s[%d];\n", precision, type, uniform->name, uniform->count);
    else
        ngli_bstr_printf(b, "uniform %s %s %s;\n", precision, type, uniform->name);

    if (!ngli_darray_push(&s->pipeline_info.desc.uniforms, &pl_uniform_desc))
        return NGL_ERROR_MEMORY;
    if (!ngli_darray_push(&s->pipeline_info.data.uniforms, &uniform->data))
        return NGL_ERROR_MEMORY;
    return 0;
}

static const char * const texture_info_suffixes[NGLI_INFO_FIELD_NB] = {
    [NGLI_INFO_FIELD_SAMPLING_MODE]     = "_sampling_mode",
    [NGLI_INFO_FIELD_DEFAULT_SAMPLER]   = "",
    [NGLI_INFO_FIELD_COORDINATE_MATRIX] = "_coord_matrix",
    [NGLI_INFO_FIELD_COLOR_MATRIX]      = "_color_matrix",
    [NGLI_INFO_FIELD_DIMENSIONS]        = "_dimensions",
    [NGLI_INFO_FIELD_TIMESTAMP]         = "_ts",
    [NGLI_INFO_FIELD_OES_SAMPLER]       = "_external_sampler",
    [NGLI_INFO_FIELD_Y_SAMPLER]         = "_y_sampler",
    [NGLI_INFO_FIELD_UV_SAMPLER]        = "_uv_sampler",
    [NGLI_INFO_FIELD_Y_RECT_SAMPLER]    = "_y_rect_sampler",
    [NGLI_INFO_FIELD_UV_RECT_SAMPLER]   = "_uv_rect_sampler",
};

static const int texture_types_map[NGLI_PGCRAFT_SHADER_TEX_TYPE_NB][NGLI_INFO_FIELD_NB] = {
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE2D] = {
        [NGLI_INFO_FIELD_DEFAULT_SAMPLER]   = NGLI_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGLI_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGLI_TYPE_FLOAT,
#if defined(TARGET_ANDROID)
        [NGLI_INFO_FIELD_SAMPLING_MODE]     = NGLI_TYPE_INT,
        [NGLI_INFO_FIELD_OES_SAMPLER]       = NGLI_TYPE_SAMPLER_EXTERNAL_OES,
#elif defined(TARGET_IPHONE) || defined(TARGET_LINUX)
        [NGLI_INFO_FIELD_SAMPLING_MODE]     = NGLI_TYPE_INT,
        [NGLI_INFO_FIELD_Y_SAMPLER]         = NGLI_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_UV_SAMPLER]        = NGLI_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_COLOR_MATRIX]      = NGLI_TYPE_MAT4,
#elif defined(TARGET_DARWIN)
        [NGLI_INFO_FIELD_SAMPLING_MODE]     = NGLI_TYPE_INT,
        [NGLI_INFO_FIELD_Y_RECT_SAMPLER]    = NGLI_TYPE_SAMPLER_2D_RECT,
        [NGLI_INFO_FIELD_UV_RECT_SAMPLER]   = NGLI_TYPE_SAMPLER_2D_RECT,
        [NGLI_INFO_FIELD_COLOR_MATRIX]      = NGLI_TYPE_MAT4,
#endif
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE2D] = {
        [NGLI_INFO_FIELD_DEFAULT_SAMPLER]   = NGLI_TYPE_IMAGE_2D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGLI_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGLI_TYPE_FLOAT,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_TEXTURE3D] = {
        [NGLI_INFO_FIELD_DEFAULT_SAMPLER]   = NGLI_TYPE_SAMPLER_3D,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC3,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE] = {
        [NGLI_INFO_FIELD_DEFAULT_SAMPLER]   = NGLI_TYPE_SAMPLER_CUBE,
    },
};

static int prepare_texture_info_fields(struct pgcraft *s, const struct pgcraft_params *params, int graphics,
                                        const struct pgcraft_texture *texture,
                                        struct pgcraft_texture_info *info)
{
    const int *types_map = texture_types_map[texture->type];

    for (int i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        struct pgcraft_texture_info_field *field = &info->fields[i];

        field->type = types_map[i];
        if (field->type == NGLI_TYPE_NONE)
            continue;
        int len = snprintf(field->name, sizeof(field->name), "%s%s", texture->name, texture_info_suffixes[i]);
        if (len >= sizeof(field->name)) {
            LOG(ERROR, "texture name \"%s\" is too long", texture->name);
            return NGL_ERROR_MEMORY;
        }
        if (graphics && i == NGLI_INFO_FIELD_COORDINATE_MATRIX)
            field->stage = NGLI_PROGRAM_SHADER_VERT;
        else
            field->stage = texture->stage;
    }
    return 0;
}

/*
 * A single texture info can be shared between multiple stage, so we need to do
 * a first pass to allocate them with and make them hold all the information
 * needed for the following injection stage.
 * FIXME: this means the internally exposed pgcraft_texture_info contains many
 * unnecessary stuff for our users of the pgcraft API.
 */
static int prepare_texture_infos(struct pgcraft *s, const struct pgcraft_params *params, int graphics)
{
    for (int i = 0; i < params->nb_textures; i++) {
        const struct pgcraft_texture *texture = &params->textures[i];
        struct pgcraft_texture_info info = {
            .stage     = texture->stage,
            .precision = texture->precision,
            .texture   = texture->texture,
            .image     = texture->image,
            .format    = texture->format,
            .writable  = texture->writable,
        };

        int ret = prepare_texture_info_fields(s, params, graphics, texture, &info);
        if (ret < 0)
            return ret;

        if (!ngli_darray_push(&s->texture_infos, &info))
            return NGL_ERROR_MEMORY;
    }
    return 0;
}

static int inject_texture_info(struct pgcraft *s, struct pgcraft_texture_info *info, int stage)
{
    for (int i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        const struct pgcraft_texture_info_field *field = &info->fields[i];

        if (field->type == NGLI_TYPE_NONE || field->stage != stage)
            continue;

        struct bstr *b = s->shaders[stage];

        if (is_sampler_or_image(field->type)) {
            struct pipeline_texture_desc pl_texture_desc = {
                .type     = field->type,
                .location = -1,
                .binding  = -1,
                .access   = info->writable ? NGLI_ACCESS_READ_WRITE : NGLI_ACCESS_READ_BIT,
                .stage    = stage,
            };
            snprintf(pl_texture_desc.name, sizeof(pl_texture_desc.name), "%s", field->name);

            int *next_bind = s->next_bindings[BIND_ID(stage, NGLI_BINDING_TYPE_TEXTURE)];
            if (next_bind)
                pl_texture_desc.binding = (*next_bind)++;

            if (field->type == NGLI_TYPE_IMAGE_2D) {
                if (info->format == NGLI_TYPE_NONE) {
                    LOG(ERROR, "Texture2D.format must be set when accessing it as an image");
                    return NGL_ERROR_INVALID_ARG;
                }
                const char *format = image_glsl_format_map[info->format];
                if (!format) {
                    LOG(ERROR, "unsupported texture format");
                    return NGL_ERROR_UNSUPPORTED;
                }

                ngli_bstr_printf(b, "layout(%s", format);
                if (pl_texture_desc.binding != -1)
                    ngli_bstr_printf(b, ", binding=%d", pl_texture_desc.binding);
                ngli_bstr_printf(b, ") %s ", info->writable ? "" : "readonly");
            } else if (pl_texture_desc.binding != -1) {
                ngli_bstr_printf(b, "layout(binding=%d) ", pl_texture_desc.binding);
            }

            const char *type = get_glsl_type(field->type);
            const char *precision = get_precision_qualifier(s, field->type, info->precision, "lowp");
            ngli_bstr_printf(b, "uniform %s %s %s;\n", precision, type, field->name);

            if (!ngli_darray_push(&s->pipeline_info.desc.textures, &pl_texture_desc))
                return NGL_ERROR_MEMORY;
            if (!ngli_darray_push(&s->pipeline_info.data.textures, &info->texture))
                return NGL_ERROR_MEMORY;
        } else {
            struct pgcraft_uniform uniform = {
                .stage = field->stage,
                .type = field->type,
            };
            snprintf(uniform.name, sizeof(uniform.name), "%s", field->name);
            int ret = inject_uniform(s, b, &uniform, stage);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int inject_texture_infos(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    struct darray *texture_infos_array = &s->texture_infos;
    struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        struct pgcraft_texture_info *info = &texture_infos[i];
        int ret = inject_texture_info(s, info, stage);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static const char *glsl_layout_str_map[NGLI_BLOCK_NB_LAYOUTS] = {
    [NGLI_BLOCK_LAYOUT_STD140] = "std140",
    [NGLI_BLOCK_LAYOUT_STD430] = "std430",
};

static int inject_block(struct pgcraft *s, struct bstr *b,
                        const struct pgcraft_block *named_block, int stage)
{
    if (named_block->stage != stage)
        return 0;

    const struct block *block = named_block->block;

    struct pipeline_buffer_desc pl_buffer_desc = {
        .type    = named_block->type,
        .binding = -1,
        .access  = named_block->writable ? NGLI_ACCESS_READ_WRITE : NGLI_ACCESS_READ_BIT,
        .stage   = stage,
    };
    int len = snprintf(pl_buffer_desc.name, sizeof(pl_buffer_desc.name), "%s_block", named_block->name);
    if (len >= sizeof(pl_buffer_desc.name)) {
        LOG(ERROR, "block name \"%s\" is too long", named_block->name);
        return NGL_ERROR_MEMORY;
    }

    const char *layout = glsl_layout_str_map[block->layout];
    const int bind_type = named_block->type == NGLI_TYPE_UNIFORM_BUFFER ? NGLI_BINDING_TYPE_UBO : NGLI_BINDING_TYPE_SSBO;
    int *next_bind = s->next_bindings[BIND_ID(stage, bind_type)];
    if (next_bind) {
        pl_buffer_desc.binding = (*next_bind)++;
        ngli_bstr_printf(b, "layout(%s,binding=%d)", layout, pl_buffer_desc.binding);
    } else {
        ngli_bstr_printf(b, "layout(%s)", layout);
    }

    if (named_block->type == NGLI_TYPE_STORAGE_BUFFER && !named_block->writable)
        ngli_bstr_print(b, " readonly");

    const char *keyword = get_glsl_type(named_block->type);
    ngli_bstr_printf(b, " %s %s_block {\n", keyword, named_block->name);
    const struct block_field *field_info = ngli_darray_data(&block->fields);
    for (int i = 0; i < ngli_darray_count(&block->fields); i++) {
        const struct block_field *fi = &field_info[i];
        const char *type = get_glsl_type(fi->type);
        if (named_block->variadic && fi->count && i == ngli_darray_count(&block->fields))
            ngli_bstr_printf(b, "    %s %s[];\n", type, fi->name);
        else if (fi->count)
            ngli_bstr_printf(b, "    %s %s[%d];\n", type, fi->name, fi->count);
        else
            ngli_bstr_printf(b, "    %s %s;\n", type, fi->name);
    }
    const char *instance_name = named_block->instance_name ? named_block->instance_name : named_block->name;
    ngli_bstr_printf(b, "} %s;\n", instance_name);

    if (!ngli_darray_push(&s->pipeline_info.desc.buffers, &pl_buffer_desc))
        return NGL_ERROR_MEMORY;
    if (!ngli_darray_push(&s->pipeline_info.data.buffers, &named_block->buffer))
        return NGL_ERROR_MEMORY;
    return 0;
}

static int inject_attribute(struct pgcraft *s, struct bstr *b,
                            const struct pgcraft_attribute *attribute, int stage)
{
    ngli_assert(stage == NGLI_PROGRAM_SHADER_VERT);

    const char *type = get_glsl_type(attribute->type);

    int base_location = -1;
    const int attribute_count = attribute->type == NGLI_TYPE_MAT4 ? 4 : 1;

    if (s->has_in_out_layout_qualifiers) {
        base_location = s->next_in_locations[stage];
        s->next_in_locations[stage] += attribute_count;
        ngli_bstr_printf(b, "layout(location=%d) ", base_location);
    }

    const char *qualifier = s->has_in_out_qualifiers ? "in" : "attribute";
    const char *precision = get_precision_qualifier(s, attribute->type, attribute->precision, "highp");
    ngli_bstr_printf(b, "%s %s %s %s;\n", qualifier, precision, type, attribute->name);

    const int attribute_offset = ngli_format_get_bytes_per_pixel(attribute->format);
    for (int i = 0; i < attribute_count; i++) {
        /* negative location offset trick is for probe_pipeline_attribute() */
        const int loc = base_location != -1 ? base_location + i : -1 - i;
        struct pipeline_attribute_desc pl_attribute_desc = {
            .location = loc,
            .format   = attribute->format,
            .stride   = attribute->stride,
            .offset   = attribute->offset + i * attribute_offset,
            .rate     = attribute->rate,
        };
        snprintf(pl_attribute_desc.name, sizeof(pl_attribute_desc.name), "%s", attribute->name);

        if (!ngli_darray_push(&s->pipeline_info.desc.attributes, &pl_attribute_desc))
            return NGL_ERROR_MEMORY;
        if (!ngli_darray_push(&s->pipeline_info.data.attributes, &attribute->buffer))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

#define DEFINE_INJECT_FUNC(e)                                               \
static int inject_##e##s(struct pgcraft *s, struct bstr *b,                 \
                         const struct pgcraft_params *params, int stage)    \
{                                                                           \
    for (int i = 0; i < params->nb_##e##s; i++) {                           \
        int ret = inject_##e(s, b, &params->e##s[i], stage);                \
        if (ret < 0)                                                        \
            return ret;                                                     \
    }                                                                       \
    return 0;                                                               \
}

DEFINE_INJECT_FUNC(uniform)
DEFINE_INJECT_FUNC(block)
DEFINE_INJECT_FUNC(attribute)

const char *ublock_names[] = {
    [NGLI_PROGRAM_SHADER_VERT] = "vert",
    [NGLI_PROGRAM_SHADER_FRAG] = "frag",
    [NGLI_PROGRAM_SHADER_COMP] = "comp",
};

static int inject_ublock(struct pgcraft *s, struct bstr *b, int stage)
{
    if (!s->use_ublock)
        return 0;

    struct block *block = &s->ublock[stage];
    if (!block->size)
        return 0;

    struct buffer *ubuffer = ngli_buffer_create(s->ctx->gctx);
    if (!ubuffer)
        return NGL_ERROR_MEMORY;
    s->ubuffer[stage] = ubuffer;

    int ret = ngli_buffer_init(ubuffer, block->size, NGLI_BUFFER_USAGE_DYNAMIC_BIT      |
                                                     NGLI_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                     NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    if (ret < 0)
        return ret;

    struct pgcraft_block named_block = {
        /* instance name is empty to make field accesses identical to uniform accesses */
        .instance_name = "",
        /* FIXME: need to fallback on storage buffer if needed, similarly to pass */
        .type          = NGLI_TYPE_UNIFORM_BUFFER,
        .stage         = stage,
        .block         = block,
        .buffer        = ubuffer,
    };
    snprintf(named_block.name, sizeof(named_block.name), "ngl_%s", ublock_names[stage]);

    return inject_block(s, b, &named_block, stage);
}

static int params_have_ssbos(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    for (int i = 0; i < params->nb_blocks; i++) {
        const struct pgcraft_block *pgcraft_block = &params->blocks[i];
        if (pgcraft_block->stage == stage && pgcraft_block->type == NGLI_TYPE_STORAGE_BUFFER)
            return 1;
    }
    return 0;
}

static int params_have_images(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    const struct darray *texture_infos_array = &s->texture_infos;
    const struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        const struct pgcraft_texture_info *info = &texture_infos[i];
        for (int j = 0; j < NGLI_INFO_FIELD_NB; j++) {
            const struct pgcraft_texture_info_field *field = &info->fields[j];
            if (field->stage == stage && field->type == NGLI_TYPE_IMAGE_2D)
                return 1;
        }
    }
    return 0;
}

static void set_glsl_header(struct pgcraft *s, struct bstr *b, const struct pgcraft_params *params, int stage)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gctx *gctx = ctx->gctx;
    const struct ngl_config *config = &gctx->config;

    ngli_bstr_printf(b, "#version %d%s\n", s->glsl_version, s->glsl_version_suffix);

    const int require_ssbo_feature = params_have_ssbos(s, params, stage);
    const int require_image_feature = params_have_images(s, params, stage);
#if defined(TARGET_ANDROID)
    const int require_image_external_feature       = ngli_darray_count(&s->texture_infos) > 0 && s->glsl_version  < 300;
    const int require_image_external_essl3_feature = ngli_darray_count(&s->texture_infos) > 0 && s->glsl_version >= 300;
#endif
    const int enable_shader_texture_lod = (gctx->features & NGLI_FEATURE_SHADER_TEXTURE_LOD) == NGLI_FEATURE_SHADER_TEXTURE_LOD;
    const int enable_texture_3d         = (gctx->features & NGLI_FEATURE_TEXTURE_3D) == NGLI_FEATURE_TEXTURE_3D;

    const struct {
        int backend;
        const char *extension;
        int glsl_version;
        int required;
    } features[] = {
        /* OpenGL */
        {NGL_BACKEND_OPENGL, "GL_ARB_shading_language_420pack",       420, s->has_explicit_bindings},
        {NGL_BACKEND_OPENGL, "GL_ARB_shader_image_load_store",        420, require_image_feature},
        {NGL_BACKEND_OPENGL, "GL_ARB_shader_image_size",              430, require_image_feature},
        {NGL_BACKEND_OPENGL, "GL_ARB_shader_storage_buffer_object",   430, require_ssbo_feature},
        {NGL_BACKEND_OPENGL, "GL_ARB_compute_shader",                 430, stage == NGLI_PROGRAM_SHADER_COMP},

        /* OpenGLES */
#if defined(TARGET_ANDROID)
        {NGL_BACKEND_OPENGLES, "GL_OES_EGL_image_external",       INT_MAX, require_image_external_feature},
        {NGL_BACKEND_OPENGLES, "GL_OES_EGL_image_external_essl3", INT_MAX, require_image_external_essl3_feature},
#endif
        {NGL_BACKEND_OPENGLES, "GL_EXT_shader_texture_lod",           300, enable_shader_texture_lod},
        {NGL_BACKEND_OPENGLES, "GL_OES_texture_3D",                   300, enable_texture_3d},
    };

    for (int i = 0; i < NGLI_ARRAY_NB(features); i++) {
        if (features[i].backend == config->backend &&
            features[i].glsl_version > s->glsl_version &&
            features[i].required)
            ngli_bstr_printf(b, "#extension %s : require\n", features[i].extension);
    }

    if (ngli_darray_count(&s->texture_infos) > 0) {
        if (s->has_modern_texture_picking)
            ngli_bstr_print(b, "#define ngl_tex2d   texture\n"
                               "#define ngl_tex3d   texture\n"
                               "#define ngl_texcube texture\n");
        else
            ngli_bstr_print(b, "#define ngl_tex2d   texture2D\n"
                               "#define ngl_tex3d   texture3D\n"
                               "#define ngl_texcube textureCube\n");

        if (config->backend == NGL_BACKEND_OPENGLES && s->glsl_version < 300)
            ngli_bstr_print(b, "#define ngl_tex2dlod   texture2DLodEXT\n"
                               "#define ngl_tex3dlod   texture3DLodEXT\n"
                               "#define ngl_texcubelod textureCubeLodEXT\n");
        else
            ngli_bstr_print(b, "#define ngl_tex2dlod   textureLod\n"
                               "#define ngl_tex3dlod   textureLod\n"
                               "#define ngl_texcubelod textureLod\n");
    }

    ngli_bstr_print(b, "\n");
}

#define WHITESPACES     "\r\n\t "
#define TOKEN_ID_CHARS  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"

static const char *read_token_id(const char *p, char *buf, size_t size)
{
    const size_t len = strspn(p, TOKEN_ID_CHARS);
    snprintf(buf, size, "%.*s", (int)len, p);
    return p + len;
}

static const char *skip_arg(const char *p)
{
    /*
     * TODO: need to error out on directive lines since evaluating them is too
     * complex (and you could close a '(' in a #ifdef and close again in the
     * #else branch, so it's a problem for us)
     */
    int opened_paren = 0;
    while (*p) {
        if (*p == ',' && !opened_paren) {
            break;
        } else if (*p == '(') {
            opened_paren++;
            p++;
        } else if (*p == ')') {
            if (opened_paren == 0)
                break;
            opened_paren--;
            p++;
        } else if (!strncmp(p, "//", 2)) {
            p += strcspn(p, "\r\n");
            // TODO: skip to EOL (handle '\' at EOL?)
        } else if (!strncmp(p, "/*", 2)) {
            p += 2;
            const char *eoc = strstr(p, "*/");
            if (eoc)
                p = eoc + 2;
        } else {
            p++;
        }
    }
    return p;
}

struct token {
    char id[16];
    ptrdiff_t pos;
};

#define ARG_FMT(x) (int)x##_len, x##_start

static int handle_token(struct pgcraft *s, const struct token *token, const char *p, struct bstr *dst)
{
    /* Skip "ngl_XXX(" and the whitespaces */
    p += strlen(token->id);
    p += strspn(p, WHITESPACES);
    if (*p++ != '(')
        return NGL_ERROR_INVALID_ARG;
    p += strspn(p, WHITESPACES);

    /* Extract the first argument (texture base name) from which we later
     * derive all the uniform names */
    const char *arg0_start = p;
    p = skip_arg(p);
    ptrdiff_t arg0_len = p - arg0_start;

    /* The internal ngli_texvideo() is used as a fast-path to skip the check
     * for the sampling mode and directly do the picking */
    const int fast_picking = !strcmp(token->id, "ngli_texvideo");

    if (fast_picking || !strcmp(token->id, "ngl_texvideo")) {
        if (*p != ',')
            return NGL_ERROR_INVALID_ARG;
        p++;
        p += strspn(p, WHITESPACES);

        const char *coords_start = p;
        p = skip_arg(p);
        ptrdiff_t coords_len = p - coords_start;
        if (*p != ')')
            return NGL_ERROR_INVALID_ARG;
        p++;

        ngli_bstr_print(dst, "(");
#if defined(TARGET_ANDROID)
        if (!fast_picking)
            ngli_bstr_printf(dst, "%.*s_sampling_mode == 2 ? ", ARG_FMT(arg0));
        ngli_bstr_printf(dst, "ngl_tex2d(%.*s_external_sampler, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
        if (!fast_picking)
            ngli_bstr_printf(dst, " : ngl_tex2d(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
#elif defined(TARGET_IPHONE) || defined(TARGET_LINUX)
        if (!fast_picking)
            ngli_bstr_printf(dst, "%.*s_sampling_mode == 3 ? ", ARG_FMT(arg0));
        ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(ngl_tex2d(%.*s_y_sampler,  %.*s).r, "
                                                       "ngl_tex2d(%.*s_uv_sampler, %.*s).%s, 1.0)",
                         ARG_FMT(arg0),
                         ARG_FMT(arg0), ARG_FMT(coords),
                         ARG_FMT(arg0), ARG_FMT(coords), s->rg);
        if (!fast_picking)
            ngli_bstr_printf(dst, " : ngl_tex2d(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
#elif defined(TARGET_DARWIN)
        if (!fast_picking)
            ngli_bstr_printf(dst, "%.*s_sampling_mode == 4 ? ", ARG_FMT(arg0));
        ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(ngl_tex2d(%.*s_y_rect_sampler,  (%.*s) * %.*s_dimensions).r, "
                                                       "ngl_tex2d(%.*s_uv_rect_sampler, (%.*s) * %.*s_dimensions / 2.0).rg, 1.0)",
                         ARG_FMT(arg0),
                         ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0),
                         ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0));
        if (!fast_picking)
            ngli_bstr_printf(dst, " : ngl_tex2d(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
#else
        ngli_bstr_printf(dst, "ngl_tex2d(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
#endif
        ngli_bstr_print(dst, ")");
        ngli_bstr_print(dst, p);
    } else {
        ngli_assert(0);
    }
    return 0;
}

/*
 * We can not make use of the GLSL preproc to create these custom ngl_*()
 * operators because token pasting (##) is needed but illegal in GLES.
 *
 * Implementing a complete preprocessor is too much of a hassle and risky,
 * especially since we need to evaluate all directives in addition to ours.
 * Instead, we do a simple search & replace for our custom texture helpers. We
 * make sure it supports basic nesting, but aside from that, it's pretty basic.
 */
static int samplers_preproc(struct pgcraft *s, struct bstr *b)
{
    /*
     * If there is no texture, no point in looking for these custom "ngl_"
     * texture picking symbols.
     */
    if (!ngli_darray_data(&s->texture_infos))
        return 0;

    struct bstr *tmp_buf = ngli_bstr_create();
    if (!tmp_buf)
        return NGL_ERROR_MEMORY;

    /*
     * Construct a stack of "ngl*" tokens found in the shader.
     */
    struct darray token_stack;
    ngli_darray_init(&token_stack, sizeof(struct token), 0);
    const char *base_str = ngli_bstr_strptr(b);
    const char *p = base_str;
    while ((p = strstr(p, "ngl"))) {
        struct token token = {.pos = p - base_str};
        p = read_token_id(p, token.id, sizeof(token.id));
        if (strcmp(token.id, "ngl_texvideo") &&
            strcmp(token.id, "ngli_texvideo"))
            continue;
        ngli_darray_push(&token_stack, &token);
    }

    /*
     * Read and process the stack from the bottom-up so that we know there is
     * never anything left to substitute up until the end of the buffer.
     */
    int ret = 0;
    const struct token *tokens = ngli_darray_data(&token_stack);
    const int nb_tokens = ngli_darray_count(&token_stack);
    for (int i = 0; i < nb_tokens; i++) {
        const struct token *token = &tokens[nb_tokens - i - 1];
        ngli_bstr_clear(tmp_buf);

        /*
         * We get back the pointer in case it changed in a previous iteration
         * (internal realloc while extending it). The token offset on the other
         * hand wouldn't change since we're doing the replacements backward.
         */
        p = ngli_bstr_strptr(b);
        ret = handle_token(s, token, p + token->pos, tmp_buf);
        if (ret < 0)
            break;

        /*
         * The token function did print into the temporary buffer everything
         * up until the end of the buffer, so we can just truncate the main
         * buffer, and re-append the new payload.
         */
        ngli_bstr_truncate(b, token->pos);
        ngli_bstr_print(b, ngli_bstr_strptr(tmp_buf));
    }

    ngli_darray_reset(&token_stack);
    ngli_bstr_freep(&tmp_buf);
    return ret;
}

static int inject_iovars(struct pgcraft *s, struct bstr *b, int stage)
{
    static const char *qualifiers[2][2] = {
        [NGLI_PROGRAM_SHADER_VERT] = {"varying", "out"},
        [NGLI_PROGRAM_SHADER_FRAG] = {"varying", "in"},
    };
    const char *qualifier = qualifiers[stage][s->has_in_out_qualifiers];
    const struct pgcraft_iovar *iovars = ngli_darray_data(&s->vert_out_vars);
    for (int i = 0; i < ngli_darray_count(&s->vert_out_vars); i++) {
        if (s->has_in_out_layout_qualifiers)
            ngli_bstr_printf(b, "layout(location=%d) ", i);
        const struct pgcraft_iovar *iovar = &iovars[i];
        const char *precision = stage == NGLI_PROGRAM_SHADER_VERT
                              ? get_precision_qualifier(s, iovar->type, iovar->precision_out, "highp")
                              : get_precision_qualifier(s, iovar->type, iovar->precision_in, "highp");
        const char *type = get_glsl_type(iovar->type);
        if (type_is_int(iovar->type))
            ngli_bstr_print(b, "flat ");
        ngli_bstr_printf(b, "%s %s %s %s;\n", qualifier, precision, type, iovar->name);
    }
    return 0;
}

static int craft_vert(struct pgcraft *s, const struct pgcraft_params *params)
{
    struct bstr *b = s->shaders[NGLI_PROGRAM_SHADER_VERT];

    set_glsl_header(s, b, params, NGLI_PROGRAM_SHADER_VERT);

    ngli_bstr_printf(b, "#define ngl_out_pos gl_Position\n"
                        "#define ngl_vertex_index %s\n"
                        "#define ngl_instance_index %s\n",
                        s->sym_vertex_index, s->sym_instance_index);

    int ret;
    if ((ret = inject_iovars(s, b, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_uniforms(s, b, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_texture_infos(s, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_blocks(s, b, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_attributes(s, b, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_ublock(s, b, NGLI_PROGRAM_SHADER_VERT)))
        return ret;

    ngli_bstr_print(b, params->vert_base);
    return samplers_preproc(s, b);
}

static int craft_frag(struct pgcraft *s, const struct pgcraft_params *params)

{
    struct bstr *b = s->shaders[NGLI_PROGRAM_SHADER_FRAG];

    set_glsl_header(s, b, params, NGLI_PROGRAM_SHADER_FRAG);

    if (s->has_precision_qualifiers)
        ngli_bstr_print(b, "#if GL_FRAGMENT_PRECISION_HIGH\n"
                           "precision highp float;\n"
                           "#else\n"
                           "precision mediump float;\n"
                           "#endif\n");
    else
        /*
         * The OpenGL wiki states the following:
         *     Precision qualifiers in GLSL are supported for compatibility
         *     with OpenGL ES. They use the same syntax as ES's qualifiers, but
         *     they have no functional effects.
         *  But as safety measure, we define them anyway.
         */
        ngli_bstr_print(b, "#define lowp\n"
                           "#define mediump\n"
                           "#define highp\n");

    if (s->has_in_out_qualifiers) {
        if (s->has_in_out_layout_qualifiers) {
            const int out_location = s->next_out_locations[NGLI_PROGRAM_SHADER_FRAG]++;
            ngli_bstr_printf(b, "layout(location=%d) ", out_location);
        }
        if (params->nb_frag_output)
            ngli_bstr_printf(b, "out vec4 ngl_out_color[%d];\n", params->nb_frag_output);
        else
            ngli_bstr_print(b, "out vec4 ngl_out_color;\n");
    } else {
        ngli_bstr_print(b, "#define ngl_out_color gl_FragColor\n");
    }

    int ret;
    if ((ret = inject_iovars(s, b, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_uniforms(s, b, params, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_texture_infos(s, params, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_blocks(s, b, params, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_ublock(s, b, NGLI_PROGRAM_SHADER_FRAG)) < 0)
        return ret;

    ngli_bstr_print(b, params->frag_base);
    return samplers_preproc(s, b);
}

static int craft_comp(struct pgcraft *s, const struct pgcraft_params *params)
{
    struct bstr *b = s->shaders[NGLI_PROGRAM_SHADER_COMP];

    set_glsl_header(s, b, params, NGLI_PROGRAM_SHADER_COMP);

    const int *wg_size = params->workgroup_size;
    ngli_assert(wg_size[0] >= 0 && wg_size[1] >= 0 && wg_size[2] >= 0);
    ngli_bstr_printf(b, "layout(local_size_x=%d, local_size_y=%d, local_size_z=%d) in;\n", NGLI_ARG_VEC3(wg_size));

    int ret;
    if ((ret = inject_uniforms(s, b, params, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_texture_infos(s, params, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_blocks(s, b, params, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_ublock(s, b, NGLI_PROGRAM_SHADER_COMP)) < 0)
        return ret;

    ngli_bstr_print(b, params->comp_base);
    return samplers_preproc(s, b);
}

static int probe_pipeline_uniform(const struct hmap *info_map, void *arg)
{
    struct pipeline_uniform_desc *elem_desc = arg;
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    return 0;
}

static int probe_pipeline_buffer(const struct hmap *info_map, void *arg)
{
    struct pipeline_buffer_desc *elem_desc = arg;
    if (elem_desc->binding != -1)
        return 0;
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    elem_desc->binding = info->binding;
    return elem_desc->binding != -1 ? 0 : NGL_ERROR_NOT_FOUND;
}

static int probe_pipeline_texture(const struct hmap *info_map, void *arg)
{
    struct pipeline_texture_desc *elem_desc = arg;
    if (elem_desc->location != -1)
        return 0;
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    elem_desc->location = info->location;
    if (elem_desc->binding == -1)
        elem_desc->binding = info->binding;
    return elem_desc->location != -1 ? 0 : NGL_ERROR_NOT_FOUND;
}

static int probe_pipeline_attribute(const struct hmap *info_map, void *arg)
{
    struct pipeline_attribute_desc *elem_desc = arg;
    if (elem_desc->location >= 0) // can be ≤ -1 if there is a location offset so we don't check ≠ -1 here
        return 0;
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info || info->location == -1)
        return NGL_ERROR_NOT_FOUND;
    const int loc_offset = -elem_desc->location - 1; // reverse location offset trick from inject_attribute()
    elem_desc->location = info->location + loc_offset;
    return 0;
}

typedef int (*probe_func_type)(const struct hmap *info_map, void *arg);

static int filter_pipeline_elems(struct pgcraft *s, probe_func_type probe_func,
                                 const struct hmap *info_map,
                                 struct darray *src_desc, struct darray *src_data,
                                 struct darray *dst_desc, struct darray *dst_data)
{
    uint8_t *desc_elems = ngli_darray_data(src_desc);
    uint8_t *data_elems = ngli_darray_data(src_data);
    int num_elems = ngli_darray_count(src_desc);
    for (int i = 0; i < num_elems; i++) {
        void *desc_elem = desc_elems + i * src_desc->element_size;
        void *data_elem = data_elems + i * src_data->element_size;
        if (info_map && probe_func(info_map, desc_elem) < 0)
            continue;
        if (!ngli_darray_push(dst_desc, desc_elem))
            return NGL_ERROR_MEMORY;
        if (!ngli_darray_push(dst_data, data_elem))
            return NGL_ERROR_MEMORY;
    }
    ngli_darray_reset(src_desc);
    ngli_darray_reset(src_data);
    return 0;
}

static int get_uniform_index(const struct pgcraft *s, const char *name)
{
    const struct pipeline_uniform_desc *pipeline_uniform_descs = ngli_darray_data(&s->filtered_pipeline_info.desc.uniforms);
    for (int i = 0; i < ngli_darray_count(&s->filtered_pipeline_info.desc.uniforms); i++) {
        const struct pipeline_uniform_desc *pipeline_uniform_desc = &pipeline_uniform_descs[i];
        if (!strcmp(pipeline_uniform_desc->name, name))
            return i;
    }
    return -1;
}

static int get_texture_index(const struct pgcraft *s, const char *name)
{
    const struct pipeline_texture_desc *pipeline_texture_descs = ngli_darray_data(&s->filtered_pipeline_info.desc.textures);
    for (int i = 0; i < ngli_darray_count(&s->filtered_pipeline_info.desc.textures); i++) {
        const struct pipeline_texture_desc *pipeline_texture_desc = &pipeline_texture_descs[i];
        if (!strcmp(pipeline_texture_desc->name, name))
            return i;
    }
    return -1;
}

static int get_ublock_index(const struct pgcraft *s, const char *name, int stage)
{
    const struct darray *fields_array = &s->ublock[stage].fields;
    const struct block_field *fields = ngli_darray_data(fields_array);
    for (int i = 0; i < ngli_darray_count(fields_array); i++)
        if (!strcmp(fields[i].name, name))
            return stage << 16 | i;
    return -1;
}

static void probe_texture_info_elems(const struct pgcraft *s, struct pgcraft_texture_info_field *fields)
{
    for (int i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        struct pgcraft_texture_info_field *field = &fields[i];
        if (field->type == NGLI_TYPE_NONE)
            field->index = -1;
        else if (is_sampler_or_image(field->type))
            field->index = get_texture_index(s, field->name);
        else
            field->index = s->use_ublock ? get_ublock_index(s, field->name, field->stage): get_uniform_index(s, field->name);
    }
}

static void probe_texture_infos(struct pgcraft *s)
{
    struct darray *texture_infos_array = &s->texture_infos;
    struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        struct pgcraft_texture_info *info = &texture_infos[i];
        probe_texture_info_elems(s, info->fields);
    }
}

/*
 * Fill location/binding of pipeline params if they are not set by probing the
 * shader. Also fill the filtered array with available entries.
 */
static int probe_pipeline_elems(struct pgcraft *s)
{
    int ret;

    const struct hmap *uniforms_info   = s->program->uniforms;
    const struct hmap *buffers_info    = s->program->buffer_blocks;
    const struct hmap *attributes_info = s->program->attributes;

    struct pgcraft_pipeline_info *info  = &s->pipeline_info;
    struct pgcraft_pipeline_info *finfo = &s->filtered_pipeline_info;
    if ((ret = filter_pipeline_elems(s, probe_pipeline_uniform,   uniforms_info,   &info->desc.uniforms,   &info->data.uniforms,   &finfo->desc.uniforms,   &finfo->data.uniforms))   < 0 ||
        (ret = filter_pipeline_elems(s, probe_pipeline_buffer,    buffers_info,    &info->desc.buffers,    &info->data.buffers,    &finfo->desc.buffers,    &finfo->data.buffers))    < 0 ||
        (ret = filter_pipeline_elems(s, probe_pipeline_texture,   uniforms_info,   &info->desc.textures,   &info->data.textures,   &finfo->desc.textures,   &finfo->data.textures))   < 0 ||
        (ret = filter_pipeline_elems(s, probe_pipeline_attribute, attributes_info, &info->desc.attributes, &info->data.attributes, &finfo->desc.attributes, &finfo->data.attributes)) < 0)
        return ret;

    probe_texture_infos(s);
    return 0;
}

#define IS_GLSL_ES_MIN(min) (config->backend == NGL_BACKEND_OPENGLES && s->glsl_version >= (min))
#define IS_GLSL_MIN(min)    (config->backend == NGL_BACKEND_OPENGL   && s->glsl_version >= (min))

static void setup_glsl_info_gl(struct pgcraft *s)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;
    struct gctx *gctx = ctx->gctx;

    s->sym_vertex_index   = "gl_VertexID";
    s->sym_instance_index = "gl_InstanceID";

    s->glsl_version = gctx->language_version;

    if (config->backend == NGL_BACKEND_OPENGLES) {
        if (gctx->version >= 300) {
            s->glsl_version_suffix = " es";
        } else {
            s->rg = "ra";
        }
    }

    s->has_in_out_qualifiers        = IS_GLSL_ES_MIN(300) || IS_GLSL_MIN(150);
    s->has_in_out_layout_qualifiers = IS_GLSL_ES_MIN(310) || IS_GLSL_MIN(410);
    s->has_precision_qualifiers     = IS_GLSL_ES_MIN(100);
    s->has_modern_texture_picking   = IS_GLSL_ES_MIN(300) || IS_GLSL_MIN(330);

    s->has_explicit_bindings = IS_GLSL_ES_MIN(310) || IS_GLSL_MIN(420) ||
                               (gctx->features & NGLI_FEATURE_SHADING_LANGUAGE_420PACK);
    if (s->has_explicit_bindings) {
        /* Bindings are unique across stages and types */
        for (int i = 0; i < NB_BINDINGS; i++)
            s->next_bindings[i] = &s->bindings[i];

        /*
         * FIXME: currently, program probing code forces a binding for the UBO, so
         * it directly conflicts with the indexes we could set here. The 3
         * following lines should be removed when this is fixed.
         */
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_VERT, NGLI_BINDING_TYPE_UBO)] = NULL;
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_FRAG, NGLI_BINDING_TYPE_UBO)] = NULL;
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_COMP, NGLI_BINDING_TYPE_UBO)] = NULL;

        /* No explicit binding required for now */
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_VERT, NGLI_BINDING_TYPE_TEXTURE)] = NULL;
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_FRAG, NGLI_BINDING_TYPE_TEXTURE)] = NULL;
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_COMP, NGLI_BINDING_TYPE_TEXTURE)] = NULL;
    }
}

static void setup_glsl_info(struct pgcraft *s)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;

    s->rg = "rg";
    s->glsl_version_suffix = "";

    if (config->backend == NGL_BACKEND_OPENGL || config->backend == NGL_BACKEND_OPENGLES)
        setup_glsl_info_gl(s);
    else
        ngli_assert(0);
}

struct pgcraft *ngli_pgcraft_create(struct ngl_ctx *ctx)
{
    struct pgcraft *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->ctx = ctx;

    setup_glsl_info(s);

    if (s->use_ublock)
        ngli_block_init(s->ublock, NGLI_BLOCK_LAYOUT_STD140);

    ngli_darray_init(&s->texture_infos, sizeof(struct pgcraft_texture_info), 0);

    ngli_darray_init(&s->pipeline_info.desc.uniforms,   sizeof(struct pipeline_uniform_desc),   0);
    ngli_darray_init(&s->pipeline_info.desc.textures,   sizeof(struct pipeline_texture_desc),   0);
    ngli_darray_init(&s->pipeline_info.desc.buffers,    sizeof(struct pipeline_buffer_desc),    0);
    ngli_darray_init(&s->pipeline_info.desc.attributes, sizeof(struct pipeline_attribute_desc), 0);

    ngli_darray_init(&s->filtered_pipeline_info.desc.uniforms,   sizeof(struct pipeline_uniform_desc),   0);
    ngli_darray_init(&s->filtered_pipeline_info.desc.textures,   sizeof(struct pipeline_texture_desc),   0);
    ngli_darray_init(&s->filtered_pipeline_info.desc.buffers,    sizeof(struct pipeline_buffer_desc),    0);
    ngli_darray_init(&s->filtered_pipeline_info.desc.attributes, sizeof(struct pipeline_attribute_desc), 0);

    ngli_darray_init(&s->pipeline_info.data.uniforms,   sizeof(void *),           0);
    ngli_darray_init(&s->pipeline_info.data.textures,   sizeof(struct texture *), 0);
    ngli_darray_init(&s->pipeline_info.data.buffers,    sizeof(struct buffer *),  0);
    ngli_darray_init(&s->pipeline_info.data.attributes, sizeof(struct buffer *),  0);

    ngli_darray_init(&s->filtered_pipeline_info.data.uniforms,   sizeof(void *),           0);
    ngli_darray_init(&s->filtered_pipeline_info.data.textures,   sizeof(struct texture *), 0);
    ngli_darray_init(&s->filtered_pipeline_info.data.buffers,    sizeof(struct buffer *),  0);
    ngli_darray_init(&s->filtered_pipeline_info.data.attributes, sizeof(struct buffer *),  0);

    return s;
}

static int alloc_shader(struct pgcraft *s, int stage)
{
    ngli_assert(!s->shaders[stage]);
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NGL_ERROR_MEMORY;
    s->shaders[stage] = b;
    return 0;
}

static int get_program_compute(struct pgcraft *s, const struct pgcraft_params *params)
{
    int ret;

    if ((ret = alloc_shader(s, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = prepare_texture_infos(s, params, 0)) < 0 ||
        (ret = craft_comp(s, params)) < 0)
        return ret;

    const char *comp = ngli_bstr_strptr(s->shaders[NGLI_PROGRAM_SHADER_COMP]);
    ret = ngli_pgcache_get_compute_program(&s->ctx->pgcache, &s->program, comp);
    ngli_bstr_freep(&s->shaders[NGLI_PROGRAM_SHADER_COMP]);
    return ret;
}

static int get_program_graphics(struct pgcraft *s, const struct pgcraft_params *params)
{
    int ret;

    ngli_darray_init(&s->vert_out_vars, sizeof(struct pgcraft_iovar), 0);
    for (int i = 0; i < params->nb_vert_out_vars; i++) {
        struct pgcraft_iovar *iovar = ngli_darray_push(&s->vert_out_vars, &params->vert_out_vars[i]);
        if (!iovar)
            return NGL_ERROR_MEMORY;
    }

    if ((ret = alloc_shader(s, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = alloc_shader(s, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = prepare_texture_infos(s, params, 1)) < 0 ||
        (ret = craft_vert(s, params)) < 0 ||
        (ret = craft_frag(s, params)) < 0)
        return ret;

    const char *vert = ngli_bstr_strptr(s->shaders[NGLI_PROGRAM_SHADER_VERT]);
    const char *frag = ngli_bstr_strptr(s->shaders[NGLI_PROGRAM_SHADER_FRAG]);
    ret = ngli_pgcache_get_graphics_program(&s->ctx->pgcache, &s->program, vert, frag);
    ngli_bstr_freep(&s->shaders[NGLI_PROGRAM_SHADER_VERT]);
    ngli_bstr_freep(&s->shaders[NGLI_PROGRAM_SHADER_FRAG]);
    return ret;
}

int ngli_pgcraft_craft(struct pgcraft *s,
                       struct pipeline_params *dst_desc_params,
                       struct pipeline_resource_params *dst_data_params,
                       const struct pgcraft_params *params)
{
    int ret = params->comp_base ? get_program_compute(s, params)
                                : get_program_graphics(s, params);
    if (ret < 0)
        return ret;

    ret = probe_pipeline_elems(s);
    if (ret < 0)
        return ret;

    dst_desc_params->program            = s->program;
    dst_desc_params->uniforms_desc      = ngli_darray_data(&s->filtered_pipeline_info.desc.uniforms);
    dst_desc_params->nb_uniforms        = ngli_darray_count(&s->filtered_pipeline_info.desc.uniforms);
    dst_desc_params->textures_desc      = ngli_darray_data(&s->filtered_pipeline_info.desc.textures);
    dst_desc_params->nb_textures        = ngli_darray_count(&s->filtered_pipeline_info.desc.textures);
    dst_desc_params->attributes_desc    = ngli_darray_data(&s->filtered_pipeline_info.desc.attributes);
    dst_desc_params->nb_attributes      = ngli_darray_count(&s->filtered_pipeline_info.desc.attributes);
    dst_desc_params->buffers_desc       = ngli_darray_data(&s->filtered_pipeline_info.desc.buffers);
    dst_desc_params->nb_buffers         = ngli_darray_count(&s->filtered_pipeline_info.desc.buffers);

    dst_data_params->uniforms           = ngli_darray_data(&s->filtered_pipeline_info.data.uniforms);
    dst_data_params->nb_uniforms        = ngli_darray_count(&s->filtered_pipeline_info.data.uniforms);
    dst_data_params->textures           = ngli_darray_data(&s->filtered_pipeline_info.data.textures);
    dst_data_params->nb_textures        = ngli_darray_count(&s->filtered_pipeline_info.data.textures);
    dst_data_params->attributes         = ngli_darray_data(&s->filtered_pipeline_info.data.attributes);
    dst_data_params->nb_attributes      = ngli_darray_count(&s->filtered_pipeline_info.data.attributes);
    dst_data_params->buffers            = ngli_darray_data(&s->filtered_pipeline_info.data.buffers);
    dst_data_params->nb_buffers         = ngli_darray_count(&s->filtered_pipeline_info.data.buffers);

    if (s->use_ublock) {
        ngli_assert(dst_data_params->nb_uniforms == 0);
        for (int i = 0; i < NGLI_ARRAY_NB(s->ublock); i++) {
            struct block *block = &s->ublock[i];
            if (block->size) {
                struct buffer *buffer = s->ubuffer[i];
                dst_data_params->ublock[i]  = block;
                dst_data_params->ubuffer[i] = buffer;
            }
        }
    } else {
        memset(dst_data_params->ublock, 0, sizeof(dst_data_params->ublock));
    }

    return 0;
}

int ngli_pgcraft_get_uniform_index(const struct pgcraft *s, const char *name, int stage)
{
    return s->use_ublock ? get_ublock_index(s, name, stage) : get_uniform_index(s, name);
}

void ngli_pgcraft_freep(struct pgcraft **sp)
{
    struct pgcraft *s = *sp;
    if (!s)
        return;

    ngli_darray_reset(&s->texture_infos);
    ngli_darray_reset(&s->vert_out_vars);

    if (s->use_ublock) {
        for (int i = 0; i < NGLI_ARRAY_NB(s->ublock); i++) {
            ngli_block_reset(&s->ublock[i]);
            ngli_buffer_freep(&s->ubuffer[i]);
        }
    }

    for (int i = 0; i < NGLI_ARRAY_NB(s->shaders); i++)
        ngli_bstr_freep(&s->shaders[i]);

    ngli_darray_reset(&s->pipeline_info.desc.uniforms);
    ngli_darray_reset(&s->pipeline_info.desc.textures);
    ngli_darray_reset(&s->pipeline_info.desc.buffers);
    ngli_darray_reset(&s->pipeline_info.desc.attributes);

    ngli_darray_reset(&s->filtered_pipeline_info.desc.uniforms);
    ngli_darray_reset(&s->filtered_pipeline_info.desc.textures);
    ngli_darray_reset(&s->filtered_pipeline_info.desc.buffers);
    ngli_darray_reset(&s->filtered_pipeline_info.desc.attributes);

    ngli_freep(sp);
}
