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

#ifndef PIPELINE_VK_H
#define PIPELINE_VK_H

#include <vulkan/vulkan.h>

#include "nodegl/pipeline.h"
#include "nodegl/darray.h"

struct gctx;

struct pipeline_vk {
    struct pipeline parent;

    struct darray vertex_attribute_descs;
    struct darray vertex_binding_descs;
    struct darray vertex_buffers;
    struct darray vertex_offsets;
    int nb_vertex_buffers;
    VkDescriptorPool desc_pool;
    struct darray desc_set_layout_bindings;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet *desc_sets;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkPipelineBindPoint bind_point;
};

struct pipeline *ngli_pipeline_vk_create(struct gctx *gctx);
int ngli_pipeline_vk_init(struct pipeline *s, const struct pipeline_desc_params *params);
int ngli_pipeline_vk_bind_resources(struct pipeline *s, const struct pipeline_desc_params *desc_params,
                                    const struct pipeline_resource_params *data_params);
int ngli_pipeline_vk_update_attribute(struct pipeline *s, int index, struct buffer *buffer);
int ngli_pipeline_vk_update_uniform(struct pipeline *s, int index, const void *value);
int ngli_pipeline_vk_update_texture(struct pipeline *s, int index, struct texture *texture);
void ngli_pipeline_vk_draw(struct pipeline *s, int nb_vertices, int nb_instances);
void ngli_pipeline_vk_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_vertices, int nb_instances);
void ngli_pipeline_vk_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z);
void ngli_pipeline_vk_freep(struct pipeline **sp);

#endif
