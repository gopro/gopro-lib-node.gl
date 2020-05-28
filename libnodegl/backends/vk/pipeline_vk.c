/*
 * Copyright 2016-2018 GoPro Inc.
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
#include <limits.h>

#include "darray.h"
#include "format.h"
#include "gctx_vk.h"
#include "hmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "pipeline_vk.h"
#include "rendertarget.h"
#include "texture_vk.h"
#include "topology.h"
#include "type.h"
#include "utils.h"
#include "vkcontext.h"
#include "vkutils.h"
#include "format_vk.h"
#include "buffer_vk.h"
#include "program_vk.h"
#include "rendertarget_vk.h"

struct attribute_binding {
    struct pipeline_attribute_desc desc;
    struct buffer *buffer;
};

struct buffer_binding {
    struct pipeline_buffer_desc desc;
    struct buffer *buffer;
};

struct texture_binding {
    struct pipeline_texture_desc desc;
    struct texture *texture;
};

static const VkPrimitiveTopology vk_primitive_topology_map[NGLI_PRIMITIVE_TOPOLOGY_NB] = {
    [NGLI_PRIMITIVE_TOPOLOGY_POINT_LIST]     = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_LIST]      = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_STRIP]     = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST]  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

static VkPrimitiveTopology get_vk_topology(int topology)
{
    return vk_primitive_topology_map[topology];
}

static const VkBlendFactor vk_blend_factor_map[NGLI_BLEND_FACTOR_NB] = {

    [NGLI_BLEND_FACTOR_ZERO]                = VK_BLEND_FACTOR_ZERO,
    [NGLI_BLEND_FACTOR_ONE]                 = VK_BLEND_FACTOR_ONE,
    [NGLI_BLEND_FACTOR_SRC_COLOR]           = VK_BLEND_FACTOR_SRC_COLOR,
    [NGLI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    [NGLI_BLEND_FACTOR_DST_COLOR]           = VK_BLEND_FACTOR_DST_COLOR,
    [NGLI_BLEND_FACTOR_ONE_MINUS_DST_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    [NGLI_BLEND_FACTOR_SRC_ALPHA]           = VK_BLEND_FACTOR_SRC_ALPHA,
    [NGLI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    [NGLI_BLEND_FACTOR_DST_ALPHA]           = VK_BLEND_FACTOR_DST_ALPHA,
    [NGLI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
};

static const VkBlendFactor get_vk_blend_factor(int blend_factor)
{
    return vk_blend_factor_map[blend_factor];
}

static const VkBlendOp vk_blend_op_map[NGLI_BLEND_OP_NB] = {
    [NGLI_BLEND_OP_ADD]              = VK_BLEND_OP_ADD,
    [NGLI_BLEND_OP_SUBTRACT]         = VK_BLEND_OP_SUBTRACT,
    [NGLI_BLEND_OP_REVERSE_SUBTRACT] = VK_BLEND_OP_REVERSE_SUBTRACT,
    [NGLI_BLEND_OP_MIN]              = VK_BLEND_OP_MIN,
    [NGLI_BLEND_OP_MAX]              = VK_BLEND_OP_MAX,
};

static VkBlendOp get_vk_blend_op(int blend_op)
{
    return vk_blend_op_map[blend_op];
}

static const VkCompareOp vk_compare_op_map[NGLI_COMPARE_OP_NB] = {
    [NGLI_COMPARE_OP_NEVER]            = VK_COMPARE_OP_NEVER,
    [NGLI_COMPARE_OP_LESS]             = VK_COMPARE_OP_LESS,
    [NGLI_COMPARE_OP_EQUAL]            = VK_COMPARE_OP_EQUAL,
    [NGLI_COMPARE_OP_LESS_OR_EQUAL]    = VK_COMPARE_OP_LESS_OR_EQUAL,
    [NGLI_COMPARE_OP_GREATER]          = VK_COMPARE_OP_GREATER,
    [NGLI_COMPARE_OP_NOT_EQUAL]        = VK_COMPARE_OP_NOT_EQUAL,
    [NGLI_COMPARE_OP_GREATER_OR_EQUAL] = VK_COMPARE_OP_GREATER_OR_EQUAL,
    [NGLI_COMPARE_OP_ALWAYS]           = VK_COMPARE_OP_ALWAYS,
};

static VkCompareOp get_vk_compare_op(int compare_op)
{
    return vk_compare_op_map[compare_op];
}

static const VkStencilOp vk_stencil_op_map[NGLI_STENCIL_OP_NB] = {
    [NGLI_STENCIL_OP_KEEP]                = VK_STENCIL_OP_KEEP,
    [NGLI_STENCIL_OP_ZERO]                = VK_STENCIL_OP_ZERO,
    [NGLI_STENCIL_OP_REPLACE]             = VK_STENCIL_OP_REPLACE,
    [NGLI_STENCIL_OP_INCREMENT_AND_CLAMP] = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    [NGLI_STENCIL_OP_DECREMENT_AND_CLAMP] = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    [NGLI_STENCIL_OP_INVERT]              = VK_STENCIL_OP_INVERT,
    [NGLI_STENCIL_OP_INCREMENT_AND_WRAP]  = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    [NGLI_STENCIL_OP_DECREMENT_AND_WRAP]  = VK_STENCIL_OP_DECREMENT_AND_WRAP,
};

static VkStencilOp get_vk_stencil_op(int stencil_op)
{
    return vk_stencil_op_map[stencil_op];
}

static const VkCullModeFlags vk_cull_mode_map[NGLI_CULL_MODE_NB] = {
    [NGLI_CULL_MODE_NONE]           = VK_CULL_MODE_NONE,
    [NGLI_CULL_MODE_FRONT_BIT]      = VK_CULL_MODE_FRONT_BIT,
    [NGLI_CULL_MODE_BACK_BIT]       = VK_CULL_MODE_BACK_BIT,
};

static VkCullModeFlags get_vk_cull_mode(int cull_mode)
{
    return vk_cull_mode_map[cull_mode];
}

static VkColorComponentFlags get_vk_color_write_mask(int color_write_mask)
{
    return (color_write_mask & NGLI_COLOR_COMPONENT_R_BIT ? VK_COLOR_COMPONENT_R_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_G_BIT ? VK_COLOR_COMPONENT_G_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_B_BIT ? VK_COLOR_COMPONENT_B_BIT : 0)
         | (color_write_mask & NGLI_COLOR_COMPONENT_A_BIT ? VK_COLOR_COMPONENT_A_BIT : 0);
}

static int build_attribute_descs(struct pipeline *s, const struct pipeline_params *params)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_darray_init(&s_priv->vertex_attribute_descs, sizeof(VkVertexInputAttributeDescription), 0);
    ngli_darray_init(&s_priv->vertex_binding_descs,   sizeof(VkVertexInputBindingDescription), 0);
    ngli_darray_init(&s_priv->vertex_buffers, sizeof(VkBuffer), 0);
    ngli_darray_init(&s_priv->vertex_offsets, sizeof(VkDeviceSize), 0);

    for (int i = 0; i < params->nb_attributes; i++) {
        const struct pipeline_attribute_desc *desc = &params->attributes_desc[i];

        struct attribute_binding attribute_binding = {
            .desc = *desc,
        };
        if (!ngli_darray_push(&s_priv->attribute_bindings, &attribute_binding))
            return NGL_ERROR_MEMORY;

        const VkVertexInputBindingDescription binding_desc = {
            .binding   = i,
            .stride    = desc->stride,
            .inputRate = desc->rate ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        if (!ngli_darray_push(&s_priv->vertex_binding_descs, &binding_desc))
            return NGL_ERROR_MEMORY;

        const VkVertexInputAttributeDescription attr_desc = {
            .binding  = i,
            .location = desc->location,
            .format   = ngli_format_get_vk_format(vk, desc->format),
            .offset   = desc->offset,
        };
        if (!ngli_darray_push(&s_priv->vertex_attribute_descs, &attr_desc))
            return NGL_ERROR_MEMORY;

        const VkBuffer buffer = VK_NULL_HANDLE;
        if (!ngli_darray_push(&s_priv->vertex_buffers, &buffer))
            return NGL_ERROR_MEMORY;

        const VkDeviceSize offset = 0;
        if (!ngli_darray_push(&s_priv->vertex_offsets, &offset))
            return NGL_ERROR_MEMORY;

    }
    s_priv->nb_unbound_attributes = params->nb_attributes;

    return 0;
}

static int pipeline_graphics_init(struct pipeline *s, const struct pipeline_params *params)
{
    const struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    const struct vkcontext *vk = gctx_vk->vkcontext;
    const struct pipeline_graphics *graphics = &s->graphics;
    const struct graphicstate *state = &graphics->state;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    int ret = build_attribute_descs(s, params);
    if (ret < 0)
        return ret;

    const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = ngli_darray_count(&s_priv->vertex_binding_descs),
        .pVertexBindingDescriptions      = ngli_darray_data(&s_priv->vertex_binding_descs),
        .vertexAttributeDescriptionCount = ngli_darray_count(&s_priv->vertex_attribute_descs),
        .pVertexAttributeDescriptions    = ngli_darray_data(&s_priv->vertex_attribute_descs),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = get_vk_topology(graphics->topology),
    };

    const VkViewport viewport = {
        .width    = gctx_vk->width,
        .height   = gctx_vk->height,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    const VkRect2D scissor = {
        .extent = gctx_vk->extent,
    };

    const VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
    };

    const VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth   = 1.f,
        .cullMode    = get_vk_cull_mode(state->cull_mode),
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
    };

    const struct rendertarget_desc *desc = &graphics->rt_desc;
    const VkSampleCountFlagBits samples = ngli_vk_get_sample_count(desc->samples);
    const VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = samples,
    };

    const VkPipelineDepthStencilStateCreateInfo depthstencil_state_create_info = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = state->depth_test,
        .depthWriteEnable      = state->depth_write_mask,
        .depthCompareOp        = get_vk_compare_op(state->depth_func),
        .depthBoundsTestEnable = 0,
        .stencilTestEnable     = state->stencil_test,
        .front = {
            .failOp      = get_vk_stencil_op(state->stencil_fail),
            .passOp      = get_vk_stencil_op(state->stencil_depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_depth_fail),
            .compareOp   = get_vk_compare_op(state->stencil_func),
            .compareMask = state->stencil_read_mask,
            .writeMask   = state->stencil_write_mask,
            .reference   = state->stencil_ref,
        },
        .back = {
            .failOp      = get_vk_stencil_op(state->stencil_fail),
            .passOp      = get_vk_stencil_op(state->stencil_depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_depth_fail),
            .compareOp   = get_vk_compare_op(state->stencil_func),
            .compareMask = state->stencil_read_mask,
            .writeMask   = state->stencil_write_mask,
            .reference   = state->stencil_ref,
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };

    VkPipelineColorBlendAttachmentState colorblend_attachment_states[NGLI_MAX_COLOR_ATTACHMENTS] = {0};
    for (int i = 0; i < graphics->rt_desc.nb_colors; i++) {
        colorblend_attachment_states[i] = (VkPipelineColorBlendAttachmentState) {
            .blendEnable         = state->blend,
            .srcColorBlendFactor = get_vk_blend_factor(state->blend_src_factor),
            .dstColorBlendFactor = get_vk_blend_factor(state->blend_dst_factor),
            .colorBlendOp        = get_vk_blend_op(state->blend_op),
            .srcAlphaBlendFactor = get_vk_blend_factor(state->blend_src_factor_a),
            .dstAlphaBlendFactor = get_vk_blend_factor(state->blend_dst_factor_a),
            .alphaBlendOp        = get_vk_blend_op(state->blend_op_a),
            .colorWriteMask      = get_vk_color_write_mask(state->color_write_mask),
        };
    }

    const VkPipelineColorBlendStateCreateInfo colorblend_state_create_info = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = graphics->rt_desc.nb_colors,
        .pAttachments    = colorblend_attachment_states,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NGLI_ARRAY_NB(dynamic_states),
        .pDynamicStates    = dynamic_states,
    };

    const struct program_vk *program_vk = (struct program_vk *)s->program;
    const VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = program_vk->shaders[NGLI_PROGRAM_SHADER_VERT],
            .pName  = "main",
        }, {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = program_vk->shaders[NGLI_PROGRAM_SHADER_FRAG],
            .pName  = "main",
        },
    };

    VkRenderPass render_pass = VK_NULL_HANDLE;
    ret = ngli_vk_create_compatible_renderpass(s->gctx, &graphics->rt_desc, &render_pass);
    if (ret < 0)
        return ret;

    const VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = NGLI_ARRAY_NB(shader_stage_create_info),
        .pStages             = shader_stage_create_info,
        .pVertexInputState   = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState      = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState   = &multisampling_state_create_info,
        .pDepthStencilState  = &depthstencil_state_create_info,
        .pColorBlendState    = &colorblend_state_create_info,
        .pDynamicState       = &dynamic_state_create_info,
        .layout              = s_priv->pipeline_layout,
        .renderPass          = render_pass,
        .subpass             = 0,
    };
    VkResult res = vkCreateGraphicsPipelines(vk->device, NULL, 1, &graphics_pipeline_create_info, NULL, &s_priv->pipeline);

    vkDestroyRenderPass(vk->device, render_pass, NULL);

    if (res != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    const struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    const struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const struct program_vk *program_vk = (struct program_vk *)s->program;
    const VkPipelineShaderStageCreateInfo shader_stage_create_info = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = program_vk->shaders[NGLI_PROGRAM_SHADER_COMP],
        .pName  = "main",
    };

    const VkComputePipelineCreateInfo compute_pipeline_create_info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = shader_stage_create_info,
        .layout = s_priv->pipeline_layout,
    };

    VkResult res = vkCreateComputePipelines(vk->device, NULL, 1, &compute_pipeline_create_info, NULL, &s_priv->pipeline);
    if (res != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    return 0;
}

static const VkShaderStageFlags stage_flag_map[] = {
    [NGLI_PROGRAM_SHADER_VERT] = VK_SHADER_STAGE_VERTEX_BIT,
    [NGLI_PROGRAM_SHADER_FRAG] = VK_SHADER_STAGE_FRAGMENT_BIT,
    [NGLI_PROGRAM_SHADER_COMP] = VK_SHADER_STAGE_COMPUTE_BIT,
};

static const VkDescriptorType descriptor_type_map[] = {
    [NGLI_TYPE_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [NGLI_TYPE_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [NGLI_TYPE_SAMPLER_2D]     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_SAMPLER_3D]     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_SAMPLER_CUBE]   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_IMAGE_2D]       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
};

static VkDescriptorType get_descriptor_type(int type)
{
    ngli_assert(type >= 0 && type < NGLI_TYPE_NB);
    VkDescriptorType descriptor_type = descriptor_type_map[type];
    ngli_assert(descriptor_type);
    return descriptor_type;
}

static const VkImageLayout image_layout_map[] ={
    [NGLI_TYPE_SAMPLER_2D]     = VK_IMAGE_LAYOUT_GENERAL,
    [NGLI_TYPE_SAMPLER_3D]     = VK_IMAGE_LAYOUT_GENERAL,
    [NGLI_TYPE_SAMPLER_CUBE]   = VK_IMAGE_LAYOUT_GENERAL,
    [NGLI_TYPE_IMAGE_2D]       = VK_IMAGE_LAYOUT_GENERAL,
};

static VkImageLayout get_image_layout(int type)
{
    ngli_assert(type >= 0 && type < NGLI_TYPE_NB);
    VkImageLayout image_layout = image_layout_map[type];
    ngli_assert(image_layout);
    return image_layout;
}

static int create_desc_set_layout_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    const struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    const struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_darray_init(&s_priv->desc_set_layout_bindings, sizeof(VkDescriptorSetLayoutBinding), 0);

    VkDescriptorPoolSize desc_pool_size_map[NGLI_TYPE_NB] = {
        [NGLI_TYPE_UNIFORM_BUFFER] = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        [NGLI_TYPE_STORAGE_BUFFER] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        [NGLI_TYPE_SAMPLER_2D]     = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_SAMPLER_3D]     = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_SAMPLER_CUBE]   = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_IMAGE_2D]       = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
    };
    /* FIXME: Check access of size_map */

    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer_desc *desc = &params->buffers_desc[i];

        const VkDescriptorType type = get_descriptor_type(desc->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = desc->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = stage_flag_map[desc->stage],
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return NGL_ERROR_MEMORY;

        struct buffer_binding buffer_binding = {
            .desc = *desc,
        };
        if (!ngli_darray_push(&s_priv->buffer_bindings, &buffer_binding))
            return NGL_ERROR_MEMORY;

        desc_pool_size_map[desc->type].descriptorCount += gctx_vk->nb_in_flight_frames;
    }

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture_desc *desc = &params->textures_desc[i];

        const VkDescriptorType type = get_descriptor_type(desc->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = desc->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = stage_flag_map[desc->stage],
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return NGL_ERROR_MEMORY;

        const struct texture_binding texture_binding = {
            .desc = *desc,
        };
        if (!ngli_darray_push(&s_priv->texture_bindings, &texture_binding))
            return NGL_ERROR_MEMORY;

        desc_pool_size_map[desc->type].descriptorCount += gctx_vk->nb_in_flight_frames;
    }

    struct VkDescriptorPoolSize desc_pool_sizes[NGLI_ARRAY_NB(desc_pool_size_map)];
    uint32_t nb_desc_pool_sizes = 0;
    for (int i = 0; i < NGLI_ARRAY_NB(desc_pool_size_map); i++) {
        if (desc_pool_size_map[i].descriptorCount) {
            desc_pool_sizes[nb_desc_pool_sizes++] = desc_pool_size_map[i];
        }
    }
    if (!nb_desc_pool_sizes)
        return 0;

    const VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = nb_desc_pool_sizes,
        .pPoolSizes    = desc_pool_sizes,
        .maxSets       = gctx_vk->nb_in_flight_frames,
    };

    VkResult res = vkCreateDescriptorPool(vk->device, &descriptor_pool_create_info, NULL, &s_priv->desc_pool);
    if (res != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ngli_darray_count(&s_priv->desc_set_layout_bindings),
        .pBindings    = (const VkDescriptorSetLayoutBinding *)ngli_darray_data(&s_priv->desc_set_layout_bindings),
    };

    res = vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, NULL, &s_priv->desc_set_layout);
    if (res != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    VkDescriptorSetLayout *desc_set_layouts = ngli_calloc(gctx_vk->nb_in_flight_frames, sizeof(*desc_set_layouts));
    if (!desc_set_layouts)
        return NGL_ERROR_MEMORY;

    for (int i = 0; i < gctx_vk->nb_in_flight_frames; i++)
        desc_set_layouts[i] = s_priv->desc_set_layout;

    const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = s_priv->desc_pool,
        .descriptorSetCount = gctx_vk->nb_in_flight_frames,
        .pSetLayouts        = desc_set_layouts
    };

    s_priv->desc_sets = ngli_calloc(gctx_vk->nb_in_flight_frames, sizeof(*s_priv->desc_sets));
    if (!s_priv->desc_sets) {
        ngli_free(desc_set_layouts);
        return NGL_ERROR_MEMORY;
    }

    res = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, s_priv->desc_sets);
    if (res != VK_SUCCESS) {
        ngli_free(desc_set_layouts);
        return NGL_ERROR_EXTERNAL;
    }

    ngli_free(desc_set_layouts);
    return 0;
}

static int create_pipeline_layout(struct pipeline *s)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = s_priv->desc_set_layout ? 1 : 0,
        .pSetLayouts    = &s_priv->desc_set_layout,
    };

    VkResult res = vkCreatePipelineLayout(vk->device, &pipeline_layout_create_info, NULL, &s_priv->pipeline_layout);
    if (res != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    return 0;
}

static int set_uniforms(struct pipeline *s)
{
    for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const uint8_t *udata = s->udata[i];
        if (!udata)
            continue;
        struct buffer *ubuffer = s->ubuffer[i];
        const struct block *ublock = s->ublock[i];
        int ret = ngli_buffer_upload(ubuffer, udata, ublock->size, 0);
        if (ret < 0)
            return ret;
    }

    return 0;
}

struct pipeline *ngli_pipeline_vk_create(struct gctx *gctx)
{
    struct pipeline_vk *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct pipeline *)s;
}

int ngli_pipeline_vk_init(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    s->type     = params->type;
    s->graphics = params->graphics;
    s->program  = params->program;

    ngli_darray_init(&s_priv->texture_bindings, sizeof(struct texture_binding), 0);
    ngli_darray_init(&s_priv->buffer_bindings,  sizeof(struct buffer_binding), 0);
    ngli_darray_init(&s_priv->attribute_bindings, sizeof(struct attribute_binding), 0);

    int ret;
    if ((ret = create_desc_set_layout_bindings(s, params)) < 0)
        return ret;

    if ((ret = create_pipeline_layout(s)) < 0)
        return ret;

    if (params->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        ret = pipeline_graphics_init(s, params);
        if (ret < 0)
            return ret;
    } else if (params->type == NGLI_PIPELINE_TYPE_COMPUTE) {
        ret = pipeline_compute_init(s);
        if (ret < 0)
            return ret;
    } else {
        ngli_assert(0);
    }

    return 0;
}

int ngli_pipeline_vk_set_resources(struct pipeline *s, const struct pipeline_resource_params *params)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_assert(ngli_darray_count(&s_priv->attribute_bindings) == params->nb_attributes);
    for (int i = 0; i < params->nb_attributes; i++) {
        int ret = ngli_pipeline_vk_update_attribute(s, i, params->attributes[i]);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->buffer_bindings) == params->nb_buffers);
    for (int i = 0; i < params->nb_buffers; i++) {
        int ret = ngli_pipeline_vk_update_buffer(s, i, params->buffers[i]);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->texture_bindings) == params->nb_textures);
    for (int i = 0; i < params->nb_textures; i++) {
        int ret = ngli_pipeline_vk_update_texture(s, i, params->textures[i]);
        if (ret < 0)
            return ret;
    }

    /* Uniform blocks */
    memcpy(s->ublock, params->ublock, sizeof(s->ublock));
    memcpy(s->ubuffer, params->ubuffer, sizeof(s->ubuffer));
    for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const struct buffer *ubuffer = params->ubuffer[i];
        if (!ubuffer)
            continue;
        const struct block *ublock = params->ublock[i];
        s->udata[i] = ngli_calloc(1, ublock->size);
        if (!s->udata[i])
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

int ngli_pipeline_vk_update_attribute(struct pipeline *s, int index, struct buffer *buffer)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(s->type == NGLI_PIPELINE_TYPE_GRAPHICS);

    struct attribute_binding *attribute_binding = ngli_darray_get(&s_priv->attribute_bindings, index);
    ngli_assert(attribute_binding);

    const struct buffer *current_buffer = attribute_binding->buffer;
    if (!current_buffer && buffer)
        s_priv->nb_unbound_attributes--;
    else if (current_buffer && !buffer)
        s_priv->nb_unbound_attributes++;

    attribute_binding->buffer = buffer;

    VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    if (buffer) {
        struct buffer_vk *buffer_vk = (struct buffer_vk *)buffer;
        vertex_buffers[index] = buffer_vk->buffer;
    } else {
        vertex_buffers[index] = VK_NULL_HANDLE;
    }

    return 0;
}

int ngli_pipeline_vk_update_uniform(struct pipeline *s, int index, const void *value)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    const int stage = index >> 16;
    const int field_index = index & 0xffff;
    const struct block *block = s->ublock[stage];
    const struct block_field *field_infos = ngli_darray_data(&block->fields);
    const struct block_field *field_info = &field_infos[field_index];
    if (value) {
        uint8_t *dst = s->udata[stage] + field_info->offset;
        ngli_block_field_copy(field_info, dst, value);
    }
    return 0;
}

int ngli_pipeline_vk_update_texture(struct pipeline *s, int index, struct texture *texture)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    struct texture_binding *texture_binding = ngli_darray_get(&s_priv->texture_bindings, index);
    ngli_assert(texture_binding);

    texture_binding->texture = texture;

    if (texture) {
        const struct pipeline_texture_desc *desc = &texture_binding->desc;
        const struct texture_vk *texture_vk = (struct texture_vk *)texture;
        const VkDescriptorImageInfo image_info = {
            .imageLayout = get_image_layout(desc->type),
            .imageView   = texture_vk->image_view,
            .sampler     = texture_vk->image_sampler,
        };
        const VkWriteDescriptorSet write_descriptor_set = {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet           = s_priv->desc_sets[0],
            .dstBinding       = desc->binding,
            .dstArrayElement  = 0,
            .descriptorType   = get_descriptor_type(desc->type),
            .descriptorCount  = 1,
            .pImageInfo       = &image_info,
        };
        vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
    }

    return 0;
}

int ngli_pipeline_vk_update_buffer(struct pipeline *s, int index, struct buffer *buffer)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    struct buffer_binding *buffer_binding = ngli_darray_get(&s_priv->buffer_bindings, index);
    ngli_assert(buffer_binding);

    buffer_binding->buffer = buffer;

    if (buffer) {
        const struct buffer_vk *buffer_vk = (struct buffer_vk *)buffer;
        const VkDescriptorBufferInfo descriptor_buffer_info = {
            .buffer = buffer_vk->buffer,
            .offset = 0,
            .range  = buffer->size,
        };
        const struct pipeline_buffer_desc *desc = &buffer_binding->desc;
        const VkWriteDescriptorSet write_descriptor_set = {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet           = s_priv->desc_sets[0],
            .dstBinding       = desc->binding,
            .dstArrayElement  = 0,
            .descriptorType   = get_descriptor_type(desc->type),
            .descriptorCount  = 1,
            .pBufferInfo      = &descriptor_buffer_info,
            .pImageInfo       = NULL,
            .pTexelBufferView = NULL,
        };
        vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
    }

    return 0;
}

static const VkIndexType vk_indices_type_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R16_UNORM] = VK_INDEX_TYPE_UINT16,
    [NGLI_FORMAT_R32_UINT]  = VK_INDEX_TYPE_UINT32,
};

static VkIndexType get_vk_indices_type(int indices_format)
{
    return vk_indices_type_map[indices_format];
}

void ngli_pipeline_vk_draw(struct pipeline *s, int nb_vertices, int nb_instances)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (s_priv->nb_unbound_attributes) {
        LOG(ERROR, "pipeline has unbound vertex attributes");
        return;
    }

    int ret = set_uniforms(s);
    if (ret < 0)
        return;

    VkCommandBuffer cmd_buf = gctx_vk->cur_command_buffer;
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_priv->pipeline);

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[0], 0, NULL);

    const VkViewport viewport = {
        .x        = gctx_vk->viewport[0],
        .y        = gctx_vk->viewport[1],
        .width    = gctx_vk->viewport[2],
        .height   = gctx_vk->viewport[3],
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    vkCmdSetLineWidth(cmd_buf, 1.0f);

    VkRect2D scissor;
    const struct rendertarget *rt = gctx_vk->rendertarget;
    const struct pipeline_graphics *graphics = &s->graphics;
    if (graphics->state.scissor_test) {
        scissor.offset.x      = gctx_vk->scissor[0];
        scissor.offset.y      = NGLI_MAX(rt->height - gctx_vk->scissor[1] - gctx_vk->scissor[3], 0);
        scissor.extent.width  = gctx_vk->scissor[2];
        scissor.extent.height = gctx_vk->scissor[3];
    } else {
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = rt->width;
        scissor.extent.height = rt->height;
    }
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const int nb_vertex_buffers = ngli_darray_count(&s_priv->vertex_buffers);
    const VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    const VkDeviceSize *vertex_offsets = ngli_darray_data(&s_priv->vertex_offsets);
    vkCmdBindVertexBuffers(cmd_buf, 0, nb_vertex_buffers, vertex_buffers, vertex_offsets);

    vkCmdDraw(cmd_buf, nb_vertices, nb_instances, 0, 0);
}

void ngli_pipeline_vk_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (s_priv->nb_unbound_attributes) {
        LOG(ERROR, "pipeline has unbound vertex attributes");
        return;
    }

    int ret = set_uniforms(s);
    if (ret < 0)
        return;

    VkCommandBuffer cmd_buf = gctx_vk->cur_command_buffer;
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_priv->pipeline);

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[0], 0, NULL);

    const VkViewport viewport = {
        .x        = gctx_vk->viewport[0],
        .y        = gctx_vk->viewport[1],
        .width    = gctx_vk->viewport[2],
        .height   = gctx_vk->viewport[3],
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    vkCmdSetLineWidth(cmd_buf, 1.0f);

    VkRect2D scissor;
    const struct rendertarget *rt = gctx_vk->rendertarget;
    const struct pipeline_graphics *graphics = &s->graphics;
    if (graphics->state.scissor_test) {
        scissor.offset.x      = gctx_vk->scissor[0];
        scissor.offset.y      = NGLI_MAX(rt->height - gctx_vk->scissor[1] - gctx_vk->scissor[3], 0);
        scissor.extent.width  = gctx_vk->scissor[2];
        scissor.extent.height = gctx_vk->scissor[3];
    } else {
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = rt->width;
        scissor.extent.height = rt->height;
    }
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const int nb_vertex_buffers = ngli_darray_count(&s_priv->vertex_buffers);
    const VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    const VkDeviceSize *vertex_offsets = ngli_darray_data(&s_priv->vertex_offsets);
    vkCmdBindVertexBuffers(cmd_buf, 0, nb_vertex_buffers, vertex_buffers, vertex_offsets);

    struct buffer_vk *indices_vk = (struct buffer_vk *)indices;
    const VkIndexType indices_type = get_vk_indices_type(indices_format);
    vkCmdBindIndexBuffer(cmd_buf, indices_vk->buffer, 0, indices_type);

    vkCmdDrawIndexed(cmd_buf, nb_indices, nb_instances, 0, 0, 0);
}

void ngli_pipeline_vk_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    int ret = set_uniforms(s);
    if (ret < 0)
        return;

    VkCommandBuffer cmd_buf = gctx_vk->cur_command_buffer;
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, s_priv->pipeline);

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[0], 0, NULL);

    vkCmdDispatch(cmd_buf, nb_group_x, nb_group_y, nb_group_z);

    // FIXME: use more specific barriers for buffers and images
    const VkMemoryBarrier barrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
    };
    const VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 1, &barrier, 0, NULL, 0, NULL);
}

void ngli_pipeline_vk_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;

    vkDestroyDescriptorPool(vk->device, s_priv->desc_pool, NULL);
    vkDestroyDescriptorSetLayout(vk->device, s_priv->desc_set_layout, NULL);
    ngli_free(s_priv->desc_sets);

    vkDestroyPipeline(vk->device, s_priv->pipeline, NULL);
    vkDestroyPipelineLayout(vk->device, s_priv->pipeline_layout, NULL);

    ngli_darray_reset(&s_priv->texture_bindings);
    ngli_darray_reset(&s_priv->buffer_bindings);
    ngli_darray_reset(&s_priv->attribute_bindings);

    ngli_darray_reset(&s_priv->vertex_attribute_descs);
    ngli_darray_reset(&s_priv->vertex_binding_descs);
    ngli_darray_reset(&s_priv->vertex_buffers);
    ngli_darray_reset(&s_priv->vertex_offsets);
    ngli_darray_reset(&s_priv->desc_set_layout_bindings);

    ngli_freep(sp);
}
