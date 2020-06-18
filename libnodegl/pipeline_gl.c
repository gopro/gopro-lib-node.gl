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

#include <string.h>

#include "buffer_gl.h"
#include "format.h"
#include "gctx_gl.h"
#include "glcontext.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "pipeline_gl.h"
#include "program_gl.h"
#include "texture_gl.h"
#include "topology_gl.h"
#include "type_gl.h"

typedef void (*set_uniform_func)(struct glcontext *gl, GLint location, int count, const void *data);

struct uniform_desc {
    GLuint location;
    struct pipeline_uniform uniform;
    set_uniform_func set;
};

struct texture_desc {
    struct pipeline_texture texture;
};

struct buffer_desc {
    GLuint type;
    struct pipeline_buffer buffer;
};

struct attribute_desc {
    struct pipeline_attribute attribute;
};

static void set_uniform_1iv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform1iv(gl, location, count, data);
}

static void set_uniform_2iv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform2iv(gl, location, count, data);
}

static void set_uniform_3iv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform3iv(gl, location, count, data);
}

static void set_uniform_4iv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform4iv(gl, location, count, data);
}

static void set_uniform_1uiv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform1uiv(gl, location, count, data);
}

static void set_uniform_2uiv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform2uiv(gl, location, count, data);
}

static void set_uniform_3uiv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform3uiv(gl, location, count, data);
}

static void set_uniform_4uiv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform4uiv(gl, location, count, data);
}

static void set_uniform_1fv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform1fv(gl, location, count, data);
}

static void set_uniform_2fv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform2fv(gl, location, count, data);
}

static void set_uniform_3fv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform3fv(gl, location, count, data);
}

static void set_uniform_4fv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform4fv(gl, location, count, data);
}

static void set_uniform_mat3fv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniformMatrix3fv(gl, location, count, GL_FALSE, data);
}

static void set_uniform_mat4fv(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniformMatrix4fv(gl, location, count, GL_FALSE, data);
}

static const set_uniform_func set_uniform_func_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_BOOL]   = set_uniform_1iv,
    [NGLI_TYPE_INT]    = set_uniform_1iv,
    [NGLI_TYPE_IVEC2]  = set_uniform_2iv,
    [NGLI_TYPE_IVEC3]  = set_uniform_3iv,
    [NGLI_TYPE_IVEC4]  = set_uniform_4iv,
    [NGLI_TYPE_UINT]   = set_uniform_1uiv,
    [NGLI_TYPE_UIVEC2] = set_uniform_2uiv,
    [NGLI_TYPE_UIVEC3] = set_uniform_3uiv,
    [NGLI_TYPE_UIVEC4] = set_uniform_4uiv,
    [NGLI_TYPE_FLOAT]  = set_uniform_1fv,
    [NGLI_TYPE_VEC2]   = set_uniform_2fv,
    [NGLI_TYPE_VEC3]   = set_uniform_3fv,
    [NGLI_TYPE_VEC4]   = set_uniform_4fv,
    [NGLI_TYPE_MAT3]   = set_uniform_mat3fv,
    [NGLI_TYPE_MAT4]   = set_uniform_mat4fv,
};

static int build_uniform_descs(struct pipeline *s, const struct pipeline_params *params)
{
    const struct program *program = params->program;

    if (!program->uniforms)
        return 0;

    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    for (int i = 0; i < params->nb_uniforms; i++) {
        const struct pipeline_uniform *uniform = &params->uniforms[i];
        const struct program_variable_info *info = ngli_hmap_get(program->uniforms, uniform->name);
        if (!info)
            continue;

        if (!(gl->features & NGLI_FEATURE_UINT_UNIFORMS) &&
            (uniform->type == NGLI_TYPE_UINT ||
             uniform->type == NGLI_TYPE_UIVEC2 ||
             uniform->type == NGLI_TYPE_UIVEC3 ||
             uniform->type == NGLI_TYPE_UIVEC4)) {
            LOG(ERROR, "context does not support unsigned int uniform flavours");
            return NGL_ERROR_UNSUPPORTED;
        }

        const set_uniform_func set_func = set_uniform_func_map[uniform->type];
        ngli_assert(set_func);
        struct uniform_desc desc = {
            .location = info->location,
            .uniform = *uniform,
            .set = set_func,
        };
        if (!ngli_darray_push(&s->uniform_descs, &desc))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void set_uniforms(struct pipeline *s, struct glcontext *gl)
{
    const struct uniform_desc *descs = ngli_darray_data(&s->uniform_descs);
    for (int i = 0; i < ngli_darray_count(&s->uniform_descs); i++) {
        const struct uniform_desc *desc = &descs[i];
        const struct pipeline_uniform *uniform = &desc->uniform;
        if (uniform->data)
            desc->set(gl, desc->location, uniform->count, uniform->data);
    }
}

static int build_texture_descs(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture *texture = &params->textures[i];

        if (texture->type == NGLI_TYPE_IMAGE_2D) {
            struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
            struct glcontext *gl = gctx_gl->glcontext;
            const struct limits *limits = &gl->limits;

            int max_nb_textures = NGLI_MIN(limits->max_texture_image_units, sizeof(s_priv->used_texture_units) * 8);
            if (texture->binding >= max_nb_textures) {
                LOG(ERROR, "maximum number (%d) of texture unit reached", max_nb_textures);
                return NGL_ERROR_LIMIT_EXCEEDED;
            }
            if (s_priv->used_texture_units & (1ULL << texture->binding)) {
                LOG(ERROR, "texture unit %d is already used by another image", texture->binding);
                return NGL_ERROR_INVALID_DATA;
            }
            s_priv->used_texture_units |= 1ULL << texture->binding;
        }

        struct texture_desc desc = {
            .texture  = *texture,
        };
        if (!ngli_darray_push(&s->texture_descs, &desc))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int acquire_next_available_texture_unit(uint64_t *texture_units)
{
    for (int i = 0; i < sizeof(*texture_units) * 8; i++) {
        if (!(*texture_units & (1ULL << i))) {
            *texture_units |= (1ULL << i);
            return i;
        }
    }
    LOG(ERROR, "no texture unit available");
    return NGL_ERROR_LIMIT_EXCEEDED;
}

static void set_textures(struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    uint64_t texture_units = s_priv->used_texture_units;
    const struct texture_desc *descs = ngli_darray_data(&s->texture_descs);
    for (int i = 0; i < ngli_darray_count(&s->texture_descs); i++) {
        const struct texture_desc *desc = &descs[i];
        const struct pipeline_texture *pipeline_texture = &desc->texture;
        const struct texture *texture = pipeline_texture->texture;
        const struct texture_gl *texture_gl = (const struct texture_gl *)texture;

        if (pipeline_texture->type == NGLI_TYPE_IMAGE_2D) {
            GLuint texture_id = 0;
            GLenum access = GL_READ_WRITE;
            GLenum internal_format = GL_RGBA8;
            if (texture) {
                const struct texture_params *params = &texture->params;
                texture_id = texture_gl->id;
                access = ngli_texture_get_gl_access(params->access);
                internal_format = texture_gl->internal_format;
            }
            ngli_glBindImageTexture(gl, pipeline_texture->binding, texture_id, 0, GL_FALSE, 0, access, internal_format);
        } else {
            const int texture_index = acquire_next_available_texture_unit(&texture_units);
            if (texture_index < 0)
                return;
            ngli_glUniform1i(gl, pipeline_texture->location, texture_index);
            ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
            if (texture) {
                ngli_glBindTexture(gl, texture_gl->target, texture_gl->id);
            } else {
                ngli_glBindTexture(gl, GL_TEXTURE_2D, 0);
                if (gl->features & NGLI_FEATURE_TEXTURE_3D)
                    ngli_glBindTexture(gl, GL_TEXTURE_3D, 0);
                if (gl->features & NGLI_FEATURE_OES_EGL_EXTERNAL_IMAGE)
                    ngli_glBindTexture(gl, GL_TEXTURE_EXTERNAL_OES, 0);
            }
        }
    }
}

static void set_buffers(struct pipeline *s, struct glcontext *gl)
{
    const struct buffer_desc *descs = ngli_darray_data(&s->buffer_descs);
    for (int i = 0; i < ngli_darray_count(&s->buffer_descs); i++) {
        const struct buffer_desc *desc = &descs[i];
        const struct pipeline_buffer *pipeline_buffer = &desc->buffer;
        const struct buffer *buffer = pipeline_buffer->buffer;
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)buffer;
        ngli_glBindBufferBase(gl, desc->type, pipeline_buffer->binding, buffer_gl->id);
    }
}

static int build_buffer_descs(struct pipeline *s, const struct pipeline_params *params)
{
    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer *pipeline_buffer = &params->buffers[i];
        const struct buffer *buffer = pipeline_buffer->buffer;

        struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
        struct glcontext *gl = gctx_gl->glcontext;
        const struct limits *limits = &gl->limits;
        if (pipeline_buffer->type == NGLI_TYPE_UNIFORM_BUFFER &&
            buffer->size > limits->max_uniform_block_size) {
            LOG(ERROR, "buffer %s size (%d) exceeds max uniform block size (%d)",
                pipeline_buffer->name, buffer->size, limits->max_uniform_block_size);
            return NGL_ERROR_LIMIT_EXCEEDED;
        }

        struct buffer_desc desc = {
            .type    = ngli_type_get_gl_type(pipeline_buffer->type),
            .buffer  = *pipeline_buffer,
        };
        if (!ngli_darray_push(&s->buffer_descs, &desc))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void set_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    const struct attribute_desc *descs = ngli_darray_data(&s->attribute_descs);
    for (int i = 0; i < ngli_darray_count(&s->attribute_descs); i++) {
        const struct attribute_desc *desc = &descs[i];
        const struct pipeline_attribute *attribute = &desc->attribute;
        const struct buffer *buffer = attribute->buffer;
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)buffer;
        const GLuint location = attribute->location;
        const GLuint size = ngli_format_get_nb_comp(attribute->format);
        const GLint stride = attribute->stride;

        ngli_glEnableVertexAttribArray(gl, location);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer_gl->id);
        ngli_glVertexAttribPointer(gl, location, size, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)(attribute->offset));
        if ((gl->features & NGLI_FEATURE_INSTANCED_ARRAY) && attribute->rate > 0)
            ngli_glVertexAttribDivisor(gl, location, attribute->rate);
    }
}

static void reset_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    const struct attribute_desc *descs = ngli_darray_data(&s->attribute_descs);
    for (int i = 0; i < ngli_darray_count(&s->attribute_descs); i++) {
        const struct attribute_desc *desc = &descs[i];
        const struct pipeline_attribute *attribute = &desc->attribute;
        const GLuint location = attribute->location;
        ngli_glDisableVertexAttribArray(gl, location);
        if (gl->features & NGLI_FEATURE_INSTANCED_ARRAY)
            ngli_glVertexAttribDivisor(gl, location, 0);
    }
}

static int build_attribute_descs(struct pipeline *s, const struct pipeline_params *params)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    for (int i = 0; i < params->nb_attributes; i++) {
        const struct pipeline_attribute *attribute = &params->attributes[i];

        if (attribute->rate > 0 && !(gl->features & NGLI_FEATURE_INSTANCED_ARRAY)) {
            LOG(ERROR, "context does not support instanced arrays");
            return NGL_ERROR_UNSUPPORTED;
        }

        struct attribute_desc desc = {
            .attribute = *attribute,
        };
        if (!ngli_darray_push(&s->attribute_descs, &desc))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void use_program(struct pipeline *s, struct glcontext *gl)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    const struct program_gl *program_gl = (const struct program_gl *)s->program;

    if (gctx_gl->program_id != program_gl->id) {
        ngli_glUseProgram(gl, program_gl->id);
        gctx_gl->program_id = program_gl->id;
    }
}

static const GLenum gl_indices_type_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R16_UNORM] = GL_UNSIGNED_SHORT,
    [NGLI_FORMAT_R32_UINT]  = GL_UNSIGNED_INT,
};

static GLenum get_gl_indices_type(int indices_format)
{
    return gl_indices_type_map[indices_format];
}

static void bind_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    const struct pipeline_gl *s_priv = (const struct pipeline_gl *)s;
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glBindVertexArray(gl, s_priv->vao_id);
    else
        set_vertex_attribs(s, gl);
}

static void unbind_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    if (!(gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT))
        reset_vertex_attribs(s, gl);
}

static void draw_elements(const struct pipeline *s, struct glcontext *gl)
{
    bind_vertex_attribs(s, gl);

    const struct pipeline_graphics *graphics = &s->graphics;
    const struct buffer *indices = graphics->indices;
    const struct buffer_gl *indices_gl = (const struct buffer_gl *)indices;
    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    const GLenum gl_indices_type = get_gl_indices_type(graphics->indices_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_gl->id);
    ngli_glDrawElements(gl, gl_topology, graphics->nb_indices, gl_indices_type, 0);

    unbind_vertex_attribs(s, gl);
}

static void draw_elements_instanced(const struct pipeline *s, struct glcontext *gl)
{
    bind_vertex_attribs(s, gl);

    const struct pipeline_graphics *graphics = &s->graphics;
    const struct buffer *indices = graphics->indices;
    const struct buffer_gl *indices_gl = (const struct buffer_gl *)indices;
    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    const GLenum gl_indices_type = get_gl_indices_type(graphics->indices_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_gl->id);
    ngli_glDrawElementsInstanced(gl, gl_topology, graphics->nb_indices, gl_indices_type, 0, graphics->nb_instances);

    unbind_vertex_attribs(s, gl);
}

static void draw_arrays(const struct pipeline *s, struct glcontext *gl)
{
    bind_vertex_attribs(s, gl);

    const struct pipeline_graphics *graphics = &s->graphics;
    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    ngli_glDrawArrays(gl, gl_topology, 0, graphics->nb_vertices);

    unbind_vertex_attribs(s, gl);
}

static void draw_arrays_instanced(const struct pipeline *s, struct glcontext *gl)
{
    bind_vertex_attribs(s, gl);

    const struct pipeline_graphics *graphics = &s->graphics;
    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    ngli_glDrawArraysInstanced(gl, gl_topology, 0, graphics->nb_vertices, graphics->nb_instances);

    unbind_vertex_attribs(s, gl);
}

static void dispatch_compute(const struct pipeline *s, struct glcontext *gl)
{
    const struct pipeline_compute *compute = &s->compute;
    ngli_glMemoryBarrier(gl, GL_ALL_BARRIER_BITS);
    ngli_glDispatchCompute(gl, compute->nb_group_x, compute->nb_group_y, compute->nb_group_z);
    ngli_glMemoryBarrier(gl, GL_ALL_BARRIER_BITS);
}

static int pipeline_graphics_init(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct pipeline_graphics *graphics = &s->graphics;

    if (graphics->nb_instances && !(gl->features & NGLI_FEATURE_DRAW_INSTANCED)) {
        LOG(ERROR, "context does not support instanced draws");
        return NGL_ERROR_UNSUPPORTED;
    }

    int ret = build_attribute_descs(s, params);
    if (ret < 0)
        return ret;

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &s_priv->vao_id);
        ngli_glBindVertexArray(gl, s_priv->vao_id);
        set_vertex_attribs(s, gl);
    }

    if (graphics->indices)
        s_priv->exec = graphics->nb_instances > 0 ? draw_elements_instanced : draw_elements;
    else
        s_priv->exec = graphics->nb_instances > 0 ? draw_arrays_instanced : draw_arrays;

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    const struct limits *limits = &gl->limits;

    if ((gl->features & NGLI_FEATURE_COMPUTE_SHADER_ALL) != NGLI_FEATURE_COMPUTE_SHADER_ALL) {
        LOG(ERROR, "context does not support compute shaders");
        return NGL_ERROR_UNSUPPORTED;
    }

    const struct pipeline_compute *compute = &s->compute;
    const int *max_work_groups = limits->max_compute_work_group_counts;
    if (compute->nb_group_x > max_work_groups[0] ||
        compute->nb_group_y > max_work_groups[1] ||
        compute->nb_group_z > max_work_groups[2]) {
        LOG(ERROR,
            "compute work group size (%d, %d, %d) exceeds driver limit (%d, %d, %d)",
            compute->nb_group_x, compute->nb_group_y, compute->nb_group_z,
            max_work_groups[0], max_work_groups[1], max_work_groups[2]);
        return NGL_ERROR_LIMIT_EXCEEDED;
    }

    s_priv->exec = dispatch_compute;

    return 0;
}

struct pipeline *ngli_pipeline_gl_create(struct gctx *gctx)
{
    struct pipeline_gl *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct pipeline *)s;
}

int ngli_pipeline_gl_init(struct pipeline *s, const struct pipeline_params *params)
{
    s->type     = params->type;
    s->graphics = params->graphics;
    s->compute  = params->compute;
    s->program  = params->program;

    ngli_darray_init(&s->uniform_descs, sizeof(struct uniform_desc), 0);
    ngli_darray_init(&s->texture_descs, sizeof(struct texture_desc), 0);
    ngli_darray_init(&s->buffer_descs, sizeof(struct buffer_desc), 0);
    ngli_darray_init(&s->attribute_descs, sizeof(struct attribute_desc), 0);

    int ret;
    if ((ret = build_uniform_descs(s, params)) < 0 ||
        (ret = build_texture_descs(s, params)) < 0 ||
        (ret = build_buffer_descs(s, params)) < 0)
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

int ngli_pipeline_gl_update_uniform(struct pipeline *s, int index, const void *data)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < ngli_darray_count(&s->uniform_descs));
    struct uniform_desc *descs = ngli_darray_data(&s->uniform_descs);
    struct uniform_desc *desc = &descs[index];
    struct pipeline_uniform *pipeline_uniform = &desc->uniform;
    if (data) {
        struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
        struct glcontext *gl = gctx_gl->glcontext;
        use_program(s, gl);
        desc->set(gl, desc->location, pipeline_uniform->count, data);
    }
    pipeline_uniform->data = NULL;

    return 0;
}

int ngli_pipeline_gl_update_texture(struct pipeline *s, int index, struct texture *texture)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < ngli_darray_count(&s->texture_descs));
    struct texture_desc *descs = ngli_darray_data(&s->texture_descs);
    struct pipeline_texture *pipeline_texture = &descs[index].texture;
    pipeline_texture->texture = texture;

    return 0;
}

void ngli_pipeline_gl_exec(struct pipeline *s)
{
    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        struct pipeline_graphics *graphics = &s->graphics;
        ngli_glstate_update(gctx, &graphics->state);
    }

    use_program(s, gl);
    set_uniforms(s, gl);
    set_buffers(s, gl);
    set_textures(s, gl);
    s_priv->exec(s, gl);
}

void ngli_pipeline_gl_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    ngli_darray_reset(&s->uniform_descs);
    ngli_darray_reset(&s->texture_descs);
    ngli_darray_reset(&s->buffer_descs);
    ngli_darray_reset(&s->attribute_descs);

    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    ngli_glDeleteVertexArrays(gl, 1, &s_priv->vao_id);

    ngli_freep(sp);
}
