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

#ifndef PIPELINE_GL_H
#define PIPELINE_GL_H

#include "pipeline.h"
#include "glincludes.h"

struct gctx;
struct glcontext;

struct pipeline_gl {
    struct pipeline parent;

    struct darray uniform_bindings;   // uniform_binding
    struct darray texture_bindings;   // texture_binding
    struct darray buffer_bindings;    // buffer_binding
    struct darray attribute_bindings; // attribute_binding
    int nb_unbound_attributes;

    uint64_t used_texture_units;
    GLuint vao_id;
    GLenum barriers;
    void (*insert_memory_barriers)(struct pipeline *s);
};

struct pipeline *ngli_pipeline_gl_create(struct gctx *gctx);
int ngli_pipeline_gl_init(struct pipeline *s, const struct pipeline_params *params);
int ngli_pipeline_gl_set_resources(struct pipeline *s, const struct pipeline_resource_params *data_params);
int ngli_pipeline_gl_update_attribute(struct pipeline *s, int index, struct buffer *buffer);
int ngli_pipeline_gl_update_uniform(struct pipeline *s, int index, const void *value);
int ngli_pipeline_gl_update_texture(struct pipeline *s, int index, struct texture *texture);
int ngli_pipeline_gl_update_buffer(struct pipeline *s, int index, struct buffer *buffer);
void ngli_pipeline_gl_draw(struct pipeline *s, int nb_vertices, int nb_instances);
void ngli_pipeline_gl_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances);
void ngli_pipeline_gl_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z);
void ngli_pipeline_gl_freep(struct pipeline **sp);

#endif
