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

#include "buffer_gl.h"
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
#include "format_vk.h"
#include "buffer_vk.h"
#include "topology_vk.h"
#include "program_vk.h"
#include "rendertarget_vk.h"

struct attribute_desc {
    struct pipeline_attribute attribute;
};

struct buffer_desc {
    struct pipeline_buffer buffer;
};

struct texture_desc {
    struct pipeline_texture texture;
};

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
        const struct pipeline_attribute *attribute = &params->attributes[i];

        struct attribute_desc pair = {
            .attribute = *attribute,
        };
        if (!ngli_darray_push(&s->attribute_descs, &pair))
            return NGL_ERROR_MEMORY;

        VkVertexInputBindingDescription binding_desc = {
            .binding   = ngli_darray_count(&s_priv->vertex_buffers),
            .stride    = attribute->stride,
            .inputRate = attribute->rate ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        if (!ngli_darray_push(&s_priv->vertex_binding_descs, &binding_desc))
            return NGL_ERROR_MEMORY;

        VkFormat format = VK_FORMAT_UNDEFINED;
        int ret = ngli_format_get_vk_format(vk, attribute->format, &format);
        if (ret < 0)
            return ret;

        VkVertexInputAttributeDescription attr_desc = {
            .binding  = ngli_darray_count(&s_priv->vertex_buffers),
            .location = attribute->location,
            .format   = format,
            .offset   = attribute->offset,
        };
        if (!ngli_darray_push(&s_priv->vertex_attribute_descs, &attr_desc))
            return NGL_ERROR_MEMORY;

        struct buffer_vk *buffer_vk = (struct buffer_vk *)attribute->buffer;
        VkBuffer buffer = buffer_vk ? buffer_vk->buffer : VK_NULL_HANDLE;
        if (!buffer)
            s->nb_unbound_attributes++;

        if (!ngli_darray_push(&s_priv->vertex_buffers, &buffer))
            return NGL_ERROR_MEMORY;

        VkDeviceSize offset = 0;
        if (!ngli_darray_push(&s_priv->vertex_offsets, &offset))
            return NGL_ERROR_MEMORY;
    }

    return 0;
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

static int pipeline_graphics_init(struct pipeline *s, const struct pipeline_params *params)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;
    struct pipeline_graphics *graphics = &s->graphics;
    struct graphicstate *state = &graphics->state;

    int ret = build_attribute_descs(s, params);
    if (ret < 0)
        return ret;

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = ngli_darray_count(&s_priv->vertex_binding_descs),
        .pVertexBindingDescriptions = ngli_darray_data(&s_priv->vertex_binding_descs),
        .vertexAttributeDescriptionCount = ngli_darray_count(&s_priv->vertex_attribute_descs),
        .pVertexAttributeDescriptions = ngli_darray_data(&s_priv->vertex_attribute_descs),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = ngli_topology_get_vk_topology(graphics->topology),
    };

    VkViewport viewport = {
        .width = gctx_vk->width,
        .height = gctx_vk->height,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {
        .extent = gctx_vk->extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkCullModeFlags cull_mode = get_vk_cull_mode(state->cull_mode);
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.f,
        .cullMode = cull_mode,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    const struct rendertarget_desc *desc = &graphics->rt_desc;
    const struct attachment_desc *attachment_desc = &desc->colors[0];
    const VkSampleCountFlagBits samples = ngli_vk_get_sample_count(attachment_desc->samples);
    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = samples,
    };

    VkPipelineDepthStencilStateCreateInfo depthstencil_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = state->depth_test,
        .depthWriteEnable = state->depth_write_mask,
        .depthCompareOp = get_vk_compare_op(state->depth_func),
        .depthBoundsTestEnable = 0,
        .stencilTestEnable = state->stencil_test,
        .front = {
            .failOp = get_vk_stencil_op(state->stencil_fail),
            .passOp = get_vk_stencil_op(state->stencil_depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_depth_fail),
            .compareOp = get_vk_compare_op(state->stencil_func),
            .compareMask = state->stencil_read_mask,
            .writeMask = state->stencil_write_mask,
            .reference = state->stencil_ref,
        },
        .back = {
            .failOp = get_vk_stencil_op(state->stencil_fail),
            .passOp = get_vk_stencil_op(state->stencil_depth_pass),
            .depthFailOp = get_vk_stencil_op(state->stencil_depth_fail),
            .compareOp = get_vk_compare_op(state->stencil_func),
            .compareMask = state->stencil_read_mask,
            .writeMask = state->stencil_write_mask,
            .reference = state->stencil_ref,
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };

    VkPipelineColorBlendAttachmentState colorblend_attachment_states[NGLI_MAX_COLOR_ATTACHMENTS] = {0};
    for (int i = 0; i < graphics->rt_desc.nb_colors; i++) {
        colorblend_attachment_states[i] = (VkPipelineColorBlendAttachmentState) {
            .blendEnable = state->blend,
            .srcColorBlendFactor = get_vk_blend_factor(state->blend_src_factor),
            .dstColorBlendFactor = get_vk_blend_factor(state->blend_dst_factor),
            .colorBlendOp = get_vk_blend_op(state->blend_op),
            .srcAlphaBlendFactor = get_vk_blend_factor(state->blend_src_factor_a),
            .dstAlphaBlendFactor = get_vk_blend_factor(state->blend_dst_factor_a),
            .alphaBlendOp = get_vk_blend_op(state->blend_op_a),
            .colorWriteMask = get_vk_color_write_mask(state->color_write_mask),
        };
    }

    VkPipelineColorBlendStateCreateInfo colorblend_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = graphics->rt_desc.nb_colors,
        .pAttachments = colorblend_attachment_states,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NGLI_ARRAY_NB(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    const struct program_vk *program_vk = (struct program_vk *)s->program;
    VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = program_vk->shaders[NGLI_PROGRAM_SHADER_VERT].vkmodule,
            .pName = "main",
        },{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = program_vk->shaders[NGLI_PROGRAM_SHADER_FRAG].vkmodule,
            .pName = "main",
        },
    };

    VkRenderPass render_pass;
    ret = ngli_vk_create_renderpass(s->gctx, &graphics->rt_desc, &render_pass, 0);
    if (ret < 0)
        return ret;

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = NGLI_ARRAY_NB(shader_stage_create_info),
        .pStages = shader_stage_create_info,
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisampling_state_create_info,
        .pDepthStencilState = &depthstencil_state_create_info,
        .pColorBlendState = &colorblend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = s_priv->pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };

    VkResult res = vkCreateGraphicsPipelines(vk->device, NULL, 1, &graphics_pipeline_create_info, NULL, &s_priv->pipeline);

    vkDestroyRenderPass(vk->device, render_pass, NULL);

    if (res != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    s_priv->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    const struct program_vk *program_vk = (struct program_vk *)s->program;

    VkPipelineShaderStageCreateInfo shader_stage_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = program_vk->shaders[NGLI_PROGRAM_SHADER_COMP].vkmodule,
        .pName = "main",
    };

    VkComputePipelineCreateInfo compute_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader_stage_create_info,
        .layout = s_priv->pipeline_layout,
    };

    VkResult vkret = vkCreateComputePipelines(vk->device, NULL, 1, &compute_pipeline_create_info, NULL, &s_priv->pipeline);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    s_priv->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

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

static int create_desc_set_layout_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
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
        const struct pipeline_buffer *pipeline_buffer = &params->buffers[i];

        const VkDescriptorType type = get_descriptor_type(pipeline_buffer->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = pipeline_buffer->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = stage_flag_map[pipeline_buffer->stage],
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return NGL_ERROR_MEMORY;

        struct buffer_desc pair = {
            .buffer = *pipeline_buffer,
        };
        if (!ngli_darray_push(&s->buffer_descs, &pair))
            return NGL_ERROR_MEMORY;

        desc_pool_size_map[pipeline_buffer->type].descriptorCount += gctx_vk->nb_in_flight_frames;
    }

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture *pipeline_texture = &params->textures[i];

        const VkDescriptorType type = get_descriptor_type(pipeline_texture->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = pipeline_texture->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = stage_flag_map[pipeline_texture->stage],
        };
        if (!ngli_darray_push(&s_priv->desc_set_layout_bindings, &binding))
            return NGL_ERROR_MEMORY;

        struct texture_desc pair = {
            .texture = *pipeline_texture,
        };
        if (!ngli_darray_push(&s->texture_descs, &pair))
            return NGL_ERROR_MEMORY;

        desc_pool_size_map[pipeline_texture->type].descriptorCount += gctx_vk->nb_in_flight_frames;
    }

    struct VkDescriptorPoolSize desc_pool_sizes[NGLI_ARRAY_NB(desc_pool_size_map)];
    int nb_desc_pool_sizes = 0;
    for (int i = 0; i < NGLI_ARRAY_NB(desc_pool_size_map); i++) {
        if (desc_pool_size_map[i].descriptorCount) {
            desc_pool_sizes[nb_desc_pool_sizes++] = desc_pool_size_map[i];
        }
    }

    if (!nb_desc_pool_sizes)
        return 0;

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = nb_desc_pool_sizes,
        .pPoolSizes = desc_pool_sizes,
        .maxSets = gctx_vk->nb_in_flight_frames,
    };

    VkResult vkret = vkCreateDescriptorPool(vk->device, &descriptor_pool_create_info, NULL, &s_priv->desc_pool);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ngli_darray_count(&s_priv->desc_set_layout_bindings),
        .pBindings = (const VkDescriptorSetLayoutBinding *)ngli_darray_data(&s_priv->desc_set_layout_bindings),
    };

    vkret = vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, NULL, &s_priv->desc_set_layout);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    VkDescriptorSetLayout *desc_set_layouts = ngli_calloc(gctx_vk->nb_in_flight_frames, sizeof(*desc_set_layouts));
    if (!desc_set_layouts)
        return NGL_ERROR_MEMORY;

    for (int i = 0; i < gctx_vk->nb_in_flight_frames; i++)
        desc_set_layouts[i] = s_priv->desc_set_layout;

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = s_priv->desc_pool,
        .descriptorSetCount = gctx_vk->nb_in_flight_frames,
        .pSetLayouts = desc_set_layouts
    };

    s_priv->desc_sets = ngli_calloc(gctx_vk->nb_in_flight_frames, sizeof(*s_priv->desc_sets));
    if (!s_priv->desc_sets) {
        ngli_free(desc_set_layouts);
        return NGL_ERROR_MEMORY;
    }

    vkret = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, s_priv->desc_sets);
    if (vkret != VK_SUCCESS) {
        ngli_free(desc_set_layouts);
        return NGL_ERROR_EXTERNAL;
    }

    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer *pipeline_buffer = &params->buffers[i];
        struct buffer *buffer = pipeline_buffer->buffer;
        struct buffer_vk *buffer_vk = (struct buffer_vk *)pipeline_buffer->buffer;

        for (int i = 0; i < gctx_vk->nb_in_flight_frames; i++) {
            const VkDescriptorBufferInfo descriptor_buffer_info = {
                .buffer = buffer_vk->buffer,
                .offset = 0,
                .range  = buffer->size,
            };
            const VkDescriptorType type = get_descriptor_type(pipeline_buffer->type);
            const VkWriteDescriptorSet write_descriptor_set = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = s_priv->desc_sets[i],
                .dstBinding = pipeline_buffer->binding,
                .dstArrayElement = 0,
                .descriptorType = type,
                .descriptorCount = 1,
                .pBufferInfo = &descriptor_buffer_info,
                .pImageInfo = NULL,
                .pTexelBufferView = NULL,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
        }
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

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture *pipeline_texture = &params->textures[i];
        struct texture_vk *texture_vk = (struct texture_vk *)pipeline_texture->texture;
        if (!texture_vk)
            continue;

        for (int i = 0; i < gctx_vk->nb_in_flight_frames; i++) {
            VkDescriptorImageInfo image_info = {
                .imageLayout = texture_vk->image_layout,
                .imageView   = texture_vk->image_view,
                .sampler     = texture_vk->image_sampler,
            };
            VkWriteDescriptorSet write_descriptor_set = {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet           = s_priv->desc_sets[i],
                .dstBinding       = pipeline_texture->binding,
                .dstArrayElement  = 0,
                .descriptorType   = get_descriptor_type(pipeline_texture->type),
                .descriptorCount  = 1,
                .pImageInfo       = &image_info,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
        }
    }

    ngli_free(desc_set_layouts);
    return 0;
}

static int create_pipeline_layout(struct pipeline *s)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = s_priv->desc_set_layout ? 1 : 0,
        .pSetLayouts    = &s_priv->desc_set_layout,
    };

    VkResult vkret = vkCreatePipelineLayout(vk->device, &pipeline_layout_create_info, NULL, &s_priv->pipeline_layout);
    if (vkret != VK_SUCCESS)
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
        int ret = ngli_buffer_upload(ubuffer, udata, ublock->size);
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
    s->type     = params->type;
    s->graphics = params->graphics;
    s->program  = params->program;

    ngli_assert(ngli_darray_count(&s->uniform_descs) == 0);
    ngli_darray_init(&s->texture_descs, sizeof(struct texture_desc), 0);
    ngli_darray_init(&s->buffer_descs,  sizeof(struct buffer_desc), 0);
    ngli_darray_init(&s->attribute_descs, sizeof(struct attribute_desc), 0);

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

int ngli_pipeline_vk_update_attribute(struct pipeline *s, int index, struct buffer *buffer)
{
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(s->type == NGLI_PIPELINE_TYPE_GRAPHICS);
    ngli_assert(index >= 0 && index < ngli_darray_count(&s->attribute_descs));
    ngli_assert(index >= 0 && ngli_darray_count(&s_priv->vertex_buffers));

    struct attribute_desc *descs = ngli_darray_data(&s->attribute_descs);
    struct attribute_desc *desc = &descs[index];
    struct pipeline_attribute *attribute = &desc->attribute;

    if (!attribute->buffer && buffer)
        s->nb_unbound_attributes--;
    else if (attribute->buffer && !buffer)
        s->nb_unbound_attributes++;

    attribute->buffer = buffer;

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
    const struct block_field *field_info = ngli_darray_data(&block->fields);
    const struct block_field *fi = &field_info[field_index];
    uint8_t *dst = s->udata[stage] + fi->offset;
    ngli_block_datacopy(fi, dst, value);
    return 0;
}

int ngli_pipeline_vk_update_texture(struct pipeline *s, int index, struct texture *texture)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    ngli_assert(index < ngli_darray_count(&s->texture_descs));
    struct pipeline_texture *pairs = ngli_darray_data(&s->texture_descs);
    struct pipeline_texture *pipeline_texture = &pairs[index];
    pipeline_texture->texture = texture;

    ngli_texture_vk_transition_layout(texture, VK_IMAGE_LAYOUT_GENERAL);

    struct texture_vk *texture_vk = (struct texture_vk *)texture;
    VkDescriptorImageInfo image_info = {
        .imageLayout = texture_vk->image_layout,
        .imageView   = texture_vk->image_view,
        .sampler     = texture_vk->image_sampler,
    };
    VkWriteDescriptorSet write_descriptor_set = {
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = s_priv->desc_sets[gctx_vk->frame_index],
        .dstBinding       = pipeline_texture->binding,
        .dstArrayElement  = 0,
        .descriptorType   = get_descriptor_type(pipeline_texture->type),
        .descriptorCount  = 1,
        .pImageInfo       = &image_info,
    };
    vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);

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

    if (s->nb_unbound_attributes) {
        LOG(ERROR, "pipeline has unbound vertex attributes");
        return;
    }

    set_uniforms(s);

    VkCommandBuffer cmd_buf = gctx_vk->cur_command_buffer;

    ngli_gctx_vk_commit_render_pass(s->gctx);

    vkCmdBindPipeline(cmd_buf, s_priv->bind_point, s_priv->pipeline);

    struct texture_desc *pairs = ngli_darray_data(&s->texture_descs);
    for (int i = 0; i < ngli_darray_count(&s->texture_descs); i++) {
        struct pipeline_texture *texture = &pairs[i].texture;
        if (!texture->texture)
            continue;
        ngli_texture_vk_transition_layout(texture->texture, VK_IMAGE_LAYOUT_GENERAL);
    }

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, s_priv->bind_point, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[gctx_vk->frame_index], 0, NULL);

    struct pipeline_graphics *graphics = &s->graphics;

    VkViewport viewport = {
        .x = gctx_vk->viewport[0],
        .y = gctx_vk->viewport[1],
        .width = gctx_vk->viewport[2],
        .height = gctx_vk->viewport[3],
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    vkCmdSetLineWidth(cmd_buf, 1.0f);

    struct rendertarget *rt = gctx_vk->rendertarget;
    VkRect2D scissor;
    if (graphics->state.scissor_test) {
        scissor.offset.x = gctx_vk->scissor[0];
        scissor.offset.y = NGLI_MAX(rt->height - gctx_vk->scissor[1] - gctx_vk->scissor[3], 0);
        scissor.extent.width = gctx_vk->scissor[2];
        scissor.extent.height = gctx_vk->scissor[3];
    } else {
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = rt->width;
        scissor.extent.height = rt->height;
    }

    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const int nb_vertex_buffers = ngli_darray_count(&s_priv->vertex_buffers);
    VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    VkDeviceSize *vertex_offsets = ngli_darray_data(&s_priv->vertex_offsets);
    vkCmdBindVertexBuffers(cmd_buf, 0, nb_vertex_buffers, vertex_buffers, vertex_offsets);

    vkCmdDraw(cmd_buf, nb_vertices, nb_instances, 0, 0);
}

void ngli_pipeline_vk_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    if (s->nb_unbound_attributes) {
        LOG(ERROR, "pipeline has unbound vertex attributes");
        return;
    }

    set_uniforms(s);

    VkCommandBuffer cmd_buf = gctx_vk->cur_command_buffer;

    ngli_gctx_vk_commit_render_pass(s->gctx);

    vkCmdBindPipeline(cmd_buf, s_priv->bind_point, s_priv->pipeline);

    struct texture_desc *pairs = ngli_darray_data(&s->texture_descs);
    for (int i = 0; i < ngli_darray_count(&s->texture_descs); i++) {
        struct pipeline_texture *texture = &pairs[i].texture;
        if (!texture->texture)
            continue;
        ngli_texture_vk_transition_layout(texture->texture, VK_IMAGE_LAYOUT_GENERAL);
    }

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, s_priv->bind_point, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[gctx_vk->frame_index], 0, NULL);

    struct pipeline_graphics *graphics = &s->graphics;

    VkViewport viewport = {
        .x = gctx_vk->viewport[0],
        .y = gctx_vk->viewport[1],
        .width = gctx_vk->viewport[2],
        .height = gctx_vk->viewport[3],
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    vkCmdSetLineWidth(cmd_buf, 1.0f);

    struct rendertarget *rt = gctx_vk->rendertarget;
    VkRect2D scissor;
    if (graphics->state.scissor_test) {
        scissor.offset.x = gctx_vk->scissor[0];
        scissor.offset.y = NGLI_MAX(rt->height - gctx_vk->scissor[1] - gctx_vk->scissor[3], 0);
        scissor.extent.width = gctx_vk->scissor[2];
        scissor.extent.height = gctx_vk->scissor[3];
    } else {
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = rt->width;
        scissor.extent.height = rt->height;
    }

    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const int nb_vertex_buffers = ngli_darray_count(&s_priv->vertex_buffers);
    VkBuffer *vertex_buffers = ngli_darray_data(&s_priv->vertex_buffers);
    VkDeviceSize *vertex_offsets = ngli_darray_data(&s_priv->vertex_offsets);
    vkCmdBindVertexBuffers(cmd_buf, 0, nb_vertex_buffers, vertex_buffers, vertex_offsets);

    struct buffer_vk *indices_vk = (struct buffer_vk *)indices;
    VkIndexType indices_type = get_vk_indices_type(indices_format);
    vkCmdBindIndexBuffer(cmd_buf, indices_vk->buffer, 0, indices_type);

    vkCmdDrawIndexed(cmd_buf, nb_indices, nb_instances, 0, 0, 0);
}

void ngli_pipeline_vk_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z)
{
    struct gctx_vk *gctx_vk = (struct gctx_vk *)s->gctx;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    set_uniforms(s);

    VkCommandBuffer cmd_buf = gctx_vk->cur_command_buffer;

    ngli_gctx_vk_commit_render_pass(s->gctx);

    vkCmdBindPipeline(cmd_buf, s_priv->bind_point, s_priv->pipeline);

    struct texture_desc *pairs = ngli_darray_data(&s->texture_descs);
    for (int i = 0; i < ngli_darray_count(&s->texture_descs); i++) {
        struct pipeline_texture *texture = &pairs[i].texture;
        if (!texture->texture)
            continue;
        ngli_texture_vk_transition_layout(texture->texture, VK_IMAGE_LAYOUT_GENERAL);
    }

    if (s_priv->desc_sets)
        vkCmdBindDescriptorSets(cmd_buf, s_priv->bind_point, s_priv->pipeline_layout,
                                0, 1, &s_priv->desc_sets[gctx_vk->frame_index], 0, NULL);

    ngli_gctx_vk_end_render_pass(s->gctx);
    vkCmdDispatch(cmd_buf, nb_group_x, nb_group_y, nb_group_z);
}

void ngli_pipeline_vk_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    ngli_darray_reset(&s->uniform_descs);
    ngli_darray_reset(&s->texture_descs);
    ngli_darray_reset(&s->buffer_descs);
    ngli_darray_reset(&s->attribute_descs);

    struct gctx *gctx = s->gctx;
    struct gctx_vk *gctx_vk = (struct gctx_vk *)gctx;
    struct vkcontext *vk = gctx_vk->vkcontext;
    struct pipeline_vk *s_priv = (struct pipeline_vk *)s;

    vkDestroyDescriptorPool(vk->device, s_priv->desc_pool, NULL);
    vkDestroyDescriptorSetLayout(vk->device, s_priv->desc_set_layout, NULL);
    ngli_free(s_priv->desc_sets);

    vkDestroyPipeline(vk->device, s_priv->pipeline, NULL);
    vkDestroyPipelineLayout(vk->device, s_priv->pipeline_layout, NULL);

    ngli_darray_reset(&s_priv->vertex_attribute_descs);
    ngli_darray_reset(&s_priv->vertex_binding_descs);
    ngli_darray_reset(&s_priv->vertex_buffers);
    ngli_darray_reset(&s_priv->vertex_offsets);
    ngli_darray_reset(&s_priv->desc_set_layout_bindings);

    ngli_freep(sp);
}
