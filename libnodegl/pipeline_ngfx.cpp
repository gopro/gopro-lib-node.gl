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

#include "pipeline_ngfx.h"
#include "log.h"
#include "memory.h"
#include "graphics/Graphics.h"
#include "compute/ComputePipeline.h"
#include "graphics/GraphicsPipeline.h"
using namespace ngfx;

struct pipeline *ngli_pipeline_ngfx_create(struct gctx *gctx)
{
    struct pipeline_ngfx *s = (pipeline_ngfx*)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct pipeline *)s;
}
int ngli_pipeline_ngfx_init(struct pipeline *s, const struct pipeline_desc_params *params)
{
    TODO("ComputePipeline::create / GraphicsPipeline::create");
    return 0;
}
int ngli_pipeline_ngfx_bind_resources(struct pipeline *s, const struct pipeline_desc_params *desc_params,
                                      const struct pipeline_resource_params *data_params)
{
    TODO("Graphics::bindVertexBuffer, Graphics::bindUniformBuffer, Graphics::bindIndexBuffer, Graphics::bindStorageBuffer, Graphics::bindTexture, etc");
    return 0;
}
int ngli_pipeline_ngfx_update_attribute(struct pipeline *s, int index, struct buffer *buffer) {
    TODO();
    return 0;
}
int ngli_pipeline_ngfx_update_uniform(struct pipeline *s, int index, const void *value) { TODO(); return 0; }
int ngli_pipeline_ngfx_update_texture(struct pipeline *s, int index, struct texture *texture) { TODO(); return 0; }
void ngli_pipeline_ngfx_draw(struct pipeline *s, int nb_vertices, int nb_instances) { TODO(); }
void ngli_pipeline_ngfx_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances) { TODO();}
void ngli_pipeline_ngfx_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z) { TODO(); }
void ngli_pipeline_ngfx_freep(struct pipeline **sp) { TODO();}