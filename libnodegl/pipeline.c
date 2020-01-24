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

#include "format.h"
#include "glcontext.h"
#include "log.h"
#include "nodes.h"
#include "pipeline.h"
#include "topology.h"
#include "type.h"

typedef void (*set_uniform_func)(struct glcontext *gl, GLint location, int count, const void *data);

struct uniform_pair {
    GLuint location;
    struct pipeline_uniform uniform;
    set_uniform_func set;
};

struct texture_pair {
    int type;
    GLuint location;
    GLuint binding;
    struct pipeline_texture texture;
};

struct buffer_pair {
    GLuint type;
    GLuint binding;
    struct pipeline_buffer buffer;
};

struct attribute_pair {
    int count;
    GLuint location;
    struct pipeline_attribute attribute;
};

static void set_uniform_1i(struct glcontext *gl, GLint location, int count, const void *data)
{
    ngli_glUniform1iv(gl, location, count, data);
}

static void set_uniform_1f(struct glcontext *gl, GLint location, int count, const void *data)
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
    [NGLI_TYPE_BOOL]  = set_uniform_1i,
    [NGLI_TYPE_INT]   = set_uniform_1i,
    [NGLI_TYPE_FLOAT] = set_uniform_1f,
    [NGLI_TYPE_VEC2]  = set_uniform_2fv,
    [NGLI_TYPE_VEC3]  = set_uniform_3fv,
    [NGLI_TYPE_VEC4]  = set_uniform_4fv,
    [NGLI_TYPE_MAT3]  = set_uniform_mat3fv,
    [NGLI_TYPE_MAT4]  = set_uniform_mat4fv,
};

static int build_uniform_pairs(struct pipeline *s, const struct pipeline_params *params)
{
    const struct program *program = params->program;

    if (!program->uniforms)
        return 0;

    for (int i = 0; i < params->nb_uniforms; i++) {
        const struct pipeline_uniform *uniform = &params->uniforms[i];
        const struct uniformprograminfo *info = ngli_hmap_get(program->uniforms, uniform->name);
        if (!info)
            continue;

        if (uniform->type != info->type && (uniform->type != NGLI_TYPE_INT ||
            (info->type != NGLI_TYPE_BOOL && info->type != NGLI_TYPE_INT))) {
            LOG(ERROR, "uniform '%s' type does not match the type declared in the shader", uniform->name);
            return NGL_ERROR_INVALID_ARG;
        }

        const set_uniform_func set_func = set_uniform_func_map[uniform->type];
        ngli_assert(set_func);
        struct uniform_pair pair = {
            .location = info->location,
            .uniform = *uniform,
            .set = set_func,
        };
        if (!ngli_darray_push(&s->uniform_pairs, &pair))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void set_uniforms(struct pipeline *s, struct glcontext *gl)
{
    const struct uniform_pair *pairs = ngli_darray_data(&s->uniform_pairs);
    for (int i = 0; i < ngli_darray_count(&s->uniform_pairs); i++) {
        const struct uniform_pair *pair = &pairs[i];
        const struct pipeline_uniform *uniform = &pair->uniform;
        if (uniform->data)
            pair->set(gl, pair->location, uniform->count, uniform->data);
    }
}

static int build_texture_pairs(struct pipeline *s, const struct pipeline_params *params)
{
    const struct program *program = params->program;

    if (!program->uniforms)
        return 0;

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture *texture = &params->textures[i];
        const struct uniformprograminfo *info = ngli_hmap_get(program->uniforms, texture->name);
        if (!info)
            continue;

        if (info->type == NGLI_TYPE_IMAGE_2D) {
            struct ngl_ctx *ctx =s->ctx;
            struct glcontext *gl = ctx->glcontext;

            int max_nb_textures = NGLI_MIN(gl->max_texture_image_units, sizeof(s->used_texture_units) * 8);
            if (info->binding >= max_nb_textures) {
                LOG(ERROR, "maximum number (%d) of texture unit reached", max_nb_textures);
                return NGL_ERROR_LIMIT_EXCEEDED;
            }
            if (s->used_texture_units & (1ULL << info->binding)) {
                LOG(ERROR, "texture unit %d is already used by another image", info->binding);
                return NGL_ERROR_INVALID_DATA;
            }
            s->used_texture_units |= 1ULL << info->binding;
        }

        struct texture_pair pair = {
            .type     = info->type,
            .location = info->location,
            .binding  = info->binding,
            .texture  = *texture,
        };
        if (!ngli_darray_push(&s->texture_pairs, &pair))
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
    uint64_t texture_units = s->used_texture_units;
    const struct texture_pair *pairs = ngli_darray_data(&s->texture_pairs);
    for (int i = 0; i < ngli_darray_count(&s->texture_pairs); i++) {
        const struct texture_pair *pair = &pairs[i];
        const struct pipeline_texture *pipeline_texture = &pair->texture;
        const struct texture *texture = pipeline_texture->texture;

        if (pair->type == NGLI_TYPE_IMAGE_2D) {
            GLuint texture_id = 0;
            GLenum access = GL_READ_WRITE;
            GLenum internal_format = GL_RGBA8;
            if (texture) {
                const struct texture_params *params = &texture->params;
                texture_id = texture->id;
                access = ngli_texture_get_gl_access(params->access);
                internal_format = texture->internal_format;
            }
            ngli_glBindImageTexture(gl, pair->binding, texture_id, 0, GL_FALSE, 0, access, internal_format);
        } else {
            const int texture_index = acquire_next_available_texture_unit(&texture_units);
            if (texture_index < 0)
                return;
            ngli_glUniform1i(gl, pair->location, texture_index);
            ngli_glActiveTexture(gl, GL_TEXTURE0 + texture_index);
            if (texture) {
                ngli_glBindTexture(gl, texture->target, texture->id);
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
    const struct buffer_pair *pairs = ngli_darray_data(&s->buffer_pairs);
    for (int i = 0; i < ngli_darray_count(&s->buffer_pairs); i++) {
        const struct buffer_pair *pair = &pairs[i];
        const struct pipeline_buffer *pipeline_buffer = &pair->buffer;
        const struct buffer *buffer = pipeline_buffer->buffer;
        ngli_glBindBufferBase(gl, pair->type, pair->binding, buffer->id);
    }
}

static int build_buffer_pairs(struct pipeline *s, const struct pipeline_params *params)
{
    const struct program *program = params->program;

    if (!program->buffer_blocks)
        return 0;

    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer *pipeline_buffer = &params->buffers[i];
        const struct buffer *buffer = pipeline_buffer->buffer;
        const struct blockprograminfo *info = ngli_hmap_get(program->buffer_blocks, pipeline_buffer->name);
        if (!info)
            continue;

        struct ngl_ctx *ctx = s->ctx;
        struct glcontext *gl = ctx->glcontext;
        if (info->type == NGLI_TYPE_UNIFORM_BUFFER &&
            buffer->size > gl->max_uniform_block_size) {
            LOG(ERROR, "buffer %s size (%d) exceeds max uniform block size (%d)",
                pipeline_buffer->name, buffer->size, gl->max_uniform_block_size);
            return NGL_ERROR_LIMIT_EXCEEDED;
        }

        struct buffer_pair pair = {
            .binding = info->binding,
            .type    = ngli_type_get_gl_type(info->type),
            .buffer  = *pipeline_buffer,
        };
        if (!ngli_darray_push(&s->buffer_pairs, &pair))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void set_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    const struct attribute_pair *pairs = ngli_darray_data(&s->attribute_pairs);
    for (int i = 0; i < ngli_darray_count(&s->attribute_pairs); i++) {
        const struct attribute_pair *pair = &pairs[i];
        const int count = pair->count;
        const GLuint location = pair->location;
        const struct pipeline_attribute *attribute = &pair->attribute;
        const struct buffer *buffer = attribute->buffer;
        const GLuint size = ngli_format_get_nb_comp(attribute->format);
        const GLint stride = attribute->stride * count;

        for (int i = 0; i < count; i++) {
            ngli_glEnableVertexAttribArray(gl, location + i);
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->id);
            ngli_glVertexAttribPointer(gl, location, size, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)(stride * i + attribute->offset));
            if ((gl->features & NGLI_FEATURE_INSTANCED_ARRAY) && attribute->rate > 0)
                ngli_glVertexAttribDivisor(gl, location + i, attribute->rate);
        }
    }
}

static void reset_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    const struct attribute_pair *pairs = ngli_darray_data(&s->attribute_pairs);
    for (int i = 0; i < ngli_darray_count(&s->attribute_pairs); i++) {
        const struct attribute_pair *pair = &pairs[i];
        const int count = pair->count;
        const GLuint location = pair->location;
        for (int i = 0; i < count; i++) {
            ngli_glDisableVertexAttribArray(gl, location + i);
            if (gl->features & NGLI_FEATURE_INSTANCED_ARRAY)
                ngli_glVertexAttribDivisor(gl, location + i, 0);
        }
    }
}

static int build_attribute_pairs(struct pipeline *s, const struct pipeline_params *params)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;
    const struct program *program = s->program;

    if (!program->attributes)
        return 0;

    for (int i = 0; i < params->nb_attributes; i++) {
        const struct pipeline_attribute *attribute = &params->attributes[i];
        const struct attributeprograminfo *info = ngli_hmap_get(program->attributes, attribute->name);
        if (!info)
            continue;

        if (attribute->count > 4) {
            LOG(ERROR, "attribute count could not exceed 4");
            return NGL_ERROR_INVALID_ARG;
        }
        const int type_count = info->type == NGLI_TYPE_MAT4 ? 4 : 1;
        const int attribute_count = NGLI_MAX(NGLI_MIN(attribute->count, type_count), 1);

        if (attribute->rate > 0 && !(gl->features & NGLI_FEATURE_INSTANCED_ARRAY)) {
            LOG(ERROR, "context does not support instanced arrays");
            return NGL_ERROR_UNSUPPORTED;
        }

        struct attribute_pair pair = {
            .count     = attribute_count,
            .location  = info->location,
            .attribute = *attribute,
        };
        if (!ngli_darray_push(&s->attribute_pairs, &pair))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void use_program(struct pipeline *s, struct glcontext *gl)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct program *program = s->program;
    if (ctx->program_id != program->id) {
        ngli_glUseProgram(gl, program->id);
        ctx->program_id = program->id;
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
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glBindVertexArray(gl, s->vao_id);
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
    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    const GLenum gl_indices_type = get_gl_indices_type(graphics->indices_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices->id);
    ngli_glDrawElements(gl, gl_topology, graphics->nb_indices, gl_indices_type, 0);

    unbind_vertex_attribs(s, gl);
}

static void draw_elements_instanced(const struct pipeline *s, struct glcontext *gl)
{
    bind_vertex_attribs(s, gl);

    const struct pipeline_graphics *graphics = &s->graphics;
    const struct buffer *indices = graphics->indices;
    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    const GLenum gl_indices_type = get_gl_indices_type(graphics->indices_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices->id);
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
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct pipeline_graphics *graphics = &s->graphics;

    if (graphics->nb_instances && !(gl->features & NGLI_FEATURE_DRAW_INSTANCED)) {
        LOG(ERROR, "context does not support instanced draws");
        return NGL_ERROR_UNSUPPORTED;
    }

    int ret = build_attribute_pairs(s, params);
    if (ret < 0)
        return ret;

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &s->vao_id);
        ngli_glBindVertexArray(gl, s->vao_id);
        set_vertex_attribs(s, gl);
    }

    if (graphics->indices)
        s->exec = graphics->nb_instances > 0 ? draw_elements_instanced : draw_elements;
    else
        s->exec = graphics->nb_instances > 0 ? draw_arrays_instanced : draw_arrays;

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;

    if ((gl->features & NGLI_FEATURE_COMPUTE_SHADER_ALL) != NGLI_FEATURE_COMPUTE_SHADER_ALL) {
        LOG(ERROR, "context does not support compute shaders");
        return NGL_ERROR_UNSUPPORTED;
    }

    const struct pipeline_compute *compute = &s->compute;
    const int *max_work_groups = gl->max_compute_work_group_counts;
    if (compute->nb_group_x > max_work_groups[0] ||
        compute->nb_group_y > max_work_groups[1] ||
        compute->nb_group_z > max_work_groups[2]) {
        LOG(ERROR,
            "compute work group size (%d, %d, %d) exceeds driver limit (%d, %d, %d)",
            compute->nb_group_x, compute->nb_group_y, compute->nb_group_z,
            max_work_groups[0], max_work_groups[1], max_work_groups[2]);
        return NGL_ERROR_LIMIT_EXCEEDED;
    }

    s->exec = dispatch_compute;

    return 0;
}

int ngli_pipeline_init(struct pipeline *s, struct ngl_ctx *ctx, const struct pipeline_params *params)
{
    memset(s, 0, sizeof(*s));

    s->ctx      = ctx;
    s->type     = params->type;
    s->graphics = params->graphics;
    s->compute  = params->compute;
    s->program  = params->program;

    ngli_darray_init(&s->uniform_pairs, sizeof(struct uniform_pair), 0);
    ngli_darray_init(&s->texture_pairs, sizeof(struct texture_pair), 0);
    ngli_darray_init(&s->buffer_pairs, sizeof(struct buffer_pair), 0);
    ngli_darray_init(&s->attribute_pairs, sizeof(struct attribute_pair), 0);

    int ret;
    if ((ret = build_uniform_pairs(s, params)) < 0 ||
        (ret = build_texture_pairs(s, params)) < 0 ||
        (ret = build_buffer_pairs(s, params)) < 0)
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

int ngli_pipeline_get_uniform_index(struct pipeline *s, const char *name)
{
    struct uniform_pair *pairs = ngli_darray_data(&s->uniform_pairs);
    for (int i = 0; i < ngli_darray_count(&s->uniform_pairs); i++) {
        struct uniform_pair *pair = &pairs[i];
        struct pipeline_uniform *uniform = &pair->uniform;
        if (!strcmp(uniform->name, name))
            return i;
    }
    return NGL_ERROR_NOT_FOUND;
}

int ngli_pipeline_get_texture_index(struct pipeline *s, const char *name)
{
    struct texture_pair *pairs = ngli_darray_data(&s->texture_pairs);
    for (int i = 0; i < ngli_darray_count(&s->texture_pairs); i++) {
        struct texture_pair *pair = &pairs[i];
        struct pipeline_texture *pipeline_texture = &pair->texture;
        if (!strcmp(pipeline_texture->name, name))
            return i;
    }
    return NGL_ERROR_NOT_FOUND;
}

int ngli_pipeline_update_uniform(struct pipeline *s, int index, const void *data)
{
    if (index < 0)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index < ngli_darray_count(&s->uniform_pairs));
    struct uniform_pair *pairs = ngli_darray_data(&s->uniform_pairs);
    struct uniform_pair *pair = &pairs[index];
    struct pipeline_uniform *pipeline_uniform = &pair->uniform;
    if (data) {
        struct ngl_ctx *ctx = s->ctx;
        struct glcontext *gl = ctx->glcontext;
        use_program(s, gl);
        pair->set(gl, pair->location, pipeline_uniform->count, data);
    }
    pipeline_uniform->data = NULL;

    return 0;
}

int ngli_pipeline_update_texture(struct pipeline *s, int index, struct texture *texture)
{
    if (index < 0)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index < ngli_darray_count(&s->texture_pairs));
    struct texture_pair *pairs = ngli_darray_data(&s->texture_pairs);
    struct pipeline_texture *pipeline_texture = &pairs[index].texture;
    pipeline_texture->texture = texture;

    return 0;
}

void ngli_pipeline_exec(struct pipeline *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;

    ngli_honor_pending_glstate(ctx);

    use_program(s, gl);
    set_uniforms(s, gl);
    set_buffers(s, gl);
    set_textures(s, gl);
    s->exec(s, gl);
}

void ngli_pipeline_reset(struct pipeline *s)
{
    if (!s->ctx)
        return;

    ngli_darray_reset(&s->uniform_pairs);
    ngli_darray_reset(&s->texture_pairs);
    ngli_darray_reset(&s->buffer_pairs);
    ngli_darray_reset(&s->attribute_pairs);

    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *gl = ctx->glcontext;
    ngli_glDeleteVertexArrays(gl, 1, &s->vao_id);

    memset(s, 0, sizeof(*s));
}
