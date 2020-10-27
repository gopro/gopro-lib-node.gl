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

struct uniform_binding {
    GLuint location;
    set_uniform_func set;
    struct pipeline_uniform_desc desc;
    const void *data;
};

struct texture_binding {
    struct pipeline_texture_desc desc;
    const struct texture *texture;
};

struct buffer_binding {
    GLuint type;
    struct pipeline_buffer_desc desc;
    const struct buffer *buffer;
};

struct attribute_binding {
    struct pipeline_attribute_desc desc;
    const struct buffer *buffer;
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

static int build_uniform_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    const struct program *program = params->program;

    if (!program->uniforms)
        return 0;

    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    for (int i = 0; i < params->nb_uniforms; i++) {
        const struct pipeline_uniform_desc *uniform_desc = &params->uniforms_desc[i];
        const struct program_variable_info *info = ngli_hmap_get(program->uniforms, uniform_desc->name);
        if (!info)
            continue;

        if (!(gl->features & NGLI_FEATURE_UINT_UNIFORMS) &&
            (uniform_desc->type == NGLI_TYPE_UINT ||
             uniform_desc->type == NGLI_TYPE_UIVEC2 ||
             uniform_desc->type == NGLI_TYPE_UIVEC3 ||
             uniform_desc->type == NGLI_TYPE_UIVEC4)) {
            LOG(ERROR, "context does not support unsigned int uniform flavours");
            return NGL_ERROR_UNSUPPORTED;
        }

        const set_uniform_func set_func = set_uniform_func_map[uniform_desc->type];
        ngli_assert(set_func);
        struct uniform_binding binding = {
            .location = info->location,
            .set = set_func,
            .desc = *uniform_desc,
        };
        if (!ngli_darray_push(&s_priv->uniform_bindings, &binding))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void set_uniforms(struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    const struct uniform_binding *bindings = ngli_darray_data(&s_priv->uniform_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->uniform_bindings); i++) {
        const struct uniform_binding *uniform_binding = &bindings[i];
        if (uniform_binding->data)
            uniform_binding->set(gl, uniform_binding->location, uniform_binding->desc.count, uniform_binding->data);
    }
}

static int build_texture_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture_desc *texture_desc = &params->textures_desc[i];

        if (texture_desc->type == NGLI_TYPE_IMAGE_2D) {
            struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
            struct glcontext *gl = gctx_gl->glcontext;
            const struct limits *limits = &gl->limits;

            if (!(gl->features & NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE)) {
                LOG(ERROR, "context does not support shader image load store operations");
                return NGL_ERROR_UNSUPPORTED;
            }

            int max_nb_textures = NGLI_MIN(limits->max_texture_image_units, sizeof(s_priv->used_texture_units) * 8);
            if (texture_desc->binding >= max_nb_textures) {
                LOG(ERROR, "maximum number (%d) of texture unit reached", max_nb_textures);
                return NGL_ERROR_LIMIT_EXCEEDED;
            }
            if (s_priv->used_texture_units & (1ULL << texture_desc->binding)) {
                LOG(ERROR, "texture unit %d is already used by another image", texture_desc->binding);
                return NGL_ERROR_INVALID_DATA;
            }
            s_priv->used_texture_units |= 1ULL << texture_desc->binding;

            if (texture_desc->access & NGLI_ACCESS_WRITE_BIT)
                s_priv->barriers |= GL_ALL_BARRIER_BITS;
        }

        struct texture_binding binding = {
            .desc = *texture_desc,
        };
        if (!ngli_darray_push(&s_priv->texture_bindings, &binding))
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

static const GLenum gl_access_map[NGLI_ACCESS_NB] = {
    [NGLI_ACCESS_READ_BIT]   = GL_READ_ONLY,
    [NGLI_ACCESS_WRITE_BIT]  = GL_WRITE_ONLY,
    [NGLI_ACCESS_READ_WRITE] = GL_READ_WRITE,
};

static GLenum get_gl_access(int access)
{
    return gl_access_map[access];
}

static void set_textures(struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    uint64_t texture_units = s_priv->used_texture_units;
    const struct texture_binding *bindings = ngli_darray_data(&s_priv->texture_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->texture_bindings); i++) {
        const struct texture_binding *texture_binding = &bindings[i];
        const struct texture *texture = texture_binding->texture;
        const struct texture_gl *texture_gl = (const struct texture_gl *)texture;

        if (texture_binding->desc.type == NGLI_TYPE_IMAGE_2D) {
            GLuint texture_id = 0;
            const GLenum access = get_gl_access(texture_binding->desc.access);
            GLenum internal_format = GL_RGBA8;
            if (texture) {
                texture_id = texture_gl->id;
                internal_format = texture_gl->internal_format;
            }
            ngli_glBindImageTexture(gl, texture_binding->desc.binding, texture_id, 0, GL_FALSE, 0, access, internal_format);
        } else {
            const int texture_index = acquire_next_available_texture_unit(&texture_units);
            if (texture_index < 0)
                return;
            ngli_glUniform1i(gl, texture_binding->desc.location, texture_index);
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
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    const struct buffer_binding *bindings = ngli_darray_data(&s_priv->buffer_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->buffer_bindings); i++) {
        const struct buffer_binding *buffer_binding = &bindings[i];
        const struct buffer *buffer = buffer_binding->buffer;
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)buffer;
        ngli_glBindBufferBase(gl, buffer_binding->type, buffer_binding->desc.binding, buffer_gl->id);
    }
}

static int build_buffer_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer_desc *pipeline_buffer_desc = &params->buffers_desc[i];

        if (pipeline_buffer_desc->type == NGLI_TYPE_UNIFORM_BUFFER &&
            !(gl->features & NGLI_FEATURE_UNIFORM_BUFFER_OBJECT)) {
            LOG(ERROR, "context does not support uniform buffer objects");
            return NGL_ERROR_UNSUPPORTED;
        }

        if (pipeline_buffer_desc->type == NGLI_TYPE_STORAGE_BUFFER &&
            !(gl->features & NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)) {
            LOG(ERROR, "context does not support shader storage buffer objects");
            return NGL_ERROR_UNSUPPORTED;
        }

        if (pipeline_buffer_desc->access & NGLI_ACCESS_WRITE_BIT)
            s_priv->barriers |= GL_ALL_BARRIER_BITS;

        struct buffer_binding binding = {
            .type = ngli_type_get_gl_type(pipeline_buffer_desc->type),
            .desc = *pipeline_buffer_desc,
        };
        if (!ngli_darray_push(&s_priv->buffer_bindings, &binding))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void set_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    const struct attribute_binding *bindings = ngli_darray_data(&s_priv->attribute_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->attribute_bindings); i++) {
        const struct attribute_binding *attribute_binding = &bindings[i];
        const struct buffer *buffer = attribute_binding->buffer;
        const struct buffer_gl *buffer_gl = (const struct buffer_gl *)buffer;
        const GLuint location = attribute_binding->desc.location;
        const GLuint size = ngli_format_get_nb_comp(attribute_binding->desc.format);
        const GLint stride = attribute_binding->desc.stride;

        ngli_glEnableVertexAttribArray(gl, location);
        if ((gl->features & NGLI_FEATURE_INSTANCED_ARRAY) && attribute_binding->desc.rate > 0)
            ngli_glVertexAttribDivisor(gl, location, attribute_binding->desc.rate);

        if (buffer_gl) {
            ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer_gl->id);
            ngli_glVertexAttribPointer(gl, location, size, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)(attribute_binding->desc.offset));
        }
    }
}

static void reset_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    const struct attribute_binding *bindings = ngli_darray_data(&s_priv->attribute_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->attribute_bindings); i++) {
        const struct attribute_binding *attribute_binding = &bindings[i];
        const GLuint location = attribute_binding->desc.location;
        ngli_glDisableVertexAttribArray(gl, location);
        if (gl->features & NGLI_FEATURE_INSTANCED_ARRAY)
            ngli_glVertexAttribDivisor(gl, location, 0);
    }
}

static int build_attribute_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    for (int i = 0; i < params->nb_attributes; i++) {
        const struct pipeline_attribute_desc *pipeline_attribute_desc = &params->attributes_desc[i];

        if (pipeline_attribute_desc->rate > 0 && !(gl->features & NGLI_FEATURE_INSTANCED_ARRAY)) {
            LOG(ERROR, "context does not support instanced arrays");
            return NGL_ERROR_UNSUPPORTED;
        }

        struct attribute_binding desc = {
            .desc = *pipeline_attribute_desc,
        };
        if (!ngli_darray_push(&s_priv->attribute_bindings, &desc))
            return NGL_ERROR_MEMORY;
    }
    s_priv->nb_unbound_attributes = params->nb_attributes;

    return 0;
}

static const GLenum gl_indices_type_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R16_UNORM] = GL_UNSIGNED_SHORT,
    [NGLI_FORMAT_R32_UINT]  = GL_UNSIGNED_INT,
};

static GLenum get_gl_indices_type(int indices_format)
{
    return gl_indices_type_map[indices_format];
}

static void init_vertex_attribs(const struct pipeline *s, struct glcontext *gl)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    const struct attribute_binding *descs = ngli_darray_data(&s_priv->attribute_bindings);
    for (int i = 0; i < ngli_darray_count(&s_priv->attribute_bindings); i++) {
        const struct attribute_binding *attribute_binding = &descs[i];
        const GLuint location = attribute_binding->desc.location;
        ngli_glEnableVertexAttribArray(gl, location);
        if ((gl->features & NGLI_FEATURE_INSTANCED_ARRAY) && attribute_binding->desc.rate > 0)
            ngli_glVertexAttribDivisor(gl, location, attribute_binding->desc.rate);
    }
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

static int pipeline_graphics_init(struct pipeline *s, const struct pipeline_params *params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    int ret = build_attribute_bindings(s, params);
    if (ret < 0)
        return ret;

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &s_priv->vao_id);
        ngli_glBindVertexArray(gl, s_priv->vao_id);
        init_vertex_attribs(s, gl);
    }

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;

    if ((gl->features & NGLI_FEATURE_COMPUTE_SHADER_ALL) != NGLI_FEATURE_COMPUTE_SHADER_ALL) {
        LOG(ERROR, "context does not support compute shaders");
        return NGL_ERROR_UNSUPPORTED;
    }

    return 0;
}

static void insert_memory_barriers_noop(struct pipeline *s)
{
}

static void insert_memory_barriers(struct pipeline *s)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    ngli_glMemoryBarrier(gl, s_priv->barriers);
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
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    s->type     = params->type;
    s->graphics = params->graphics;
    s->program  = params->program;

    ngli_darray_init(&s_priv->uniform_bindings, sizeof(struct uniform_binding), 0);
    ngli_darray_init(&s_priv->texture_bindings, sizeof(struct texture_binding), 0);
    ngli_darray_init(&s_priv->buffer_bindings, sizeof(struct buffer_binding), 0);
    ngli_darray_init(&s_priv->attribute_bindings, sizeof(struct attribute_binding), 0);

    int ret;
    if ((ret = build_uniform_bindings(s, params)) < 0 ||
        (ret = build_texture_bindings(s, params)) < 0 ||
        (ret = build_buffer_bindings(s, params)) < 0)
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

    s_priv->insert_memory_barriers = s_priv->barriers
                                   ? insert_memory_barriers
                                   : insert_memory_barriers_noop;

    return 0;
}

int ngli_pipeline_gl_set_resources(struct pipeline *s, const struct pipeline_resource_params *data_params)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    ngli_assert(ngli_darray_count(&s_priv->attribute_bindings) == data_params->nb_attributes);
    for (int i = 0; i < data_params->nb_attributes; i++) {
        int ret = ngli_pipeline_gl_update_attribute(s, i, data_params->attributes[i]);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->buffer_bindings) == data_params->nb_buffers);
    for (int i = 0; i < data_params->nb_buffers; i++) {
        int ret = ngli_pipeline_gl_update_buffer(s, i, data_params->buffers[i]);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->texture_bindings) == data_params->nb_textures);
    for (int i = 0; i < data_params->nb_textures; i++) {
        int ret = ngli_pipeline_gl_update_texture(s, i, data_params->textures[i]);
        if (ret < 0)
            return ret;
    }

    ngli_assert(ngli_darray_count(&s_priv->uniform_bindings) == data_params->nb_uniforms);
    for (int i = 0; i < data_params->nb_uniforms; i++) {
        struct uniform_binding *uniform_binding = ngli_darray_get(&s_priv->uniform_bindings, i);
        ngli_assert(uniform_binding);
        const void *uniform_data = data_params->uniforms[i];
        uniform_binding->data = uniform_data;
    }

    return 0;
}

int ngli_pipeline_gl_update_attribute(struct pipeline *s, int index, struct buffer *buffer)
{
    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

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

    if (!buffer)
        return 0;

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        const GLuint location = attribute_binding->desc.location;
        const GLuint size = ngli_format_get_nb_comp(attribute_binding->desc.format);
        const GLint stride = attribute_binding->desc.stride;
        struct buffer_gl *buffer_gl = (struct buffer_gl *)buffer;
        ngli_glBindVertexArray(gl, s_priv->vao_id);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer_gl->id);
        ngli_glVertexAttribPointer(gl, location, size, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)(attribute_binding->desc.offset));
    }

    return 0;
}

int ngli_pipeline_gl_update_uniform(struct pipeline *s, int index, const void *data)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    struct uniform_binding *uniform_binding = ngli_darray_get(&s_priv->uniform_bindings, index);
    ngli_assert(uniform_binding);

    if (data) {
        struct gctx *gctx = s->gctx;
        struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
        struct glcontext *gl = gctx_gl->glcontext;
        struct program_gl *program_gl = (struct program_gl *)s->program;
        ngli_glstate_use_program(gctx, program_gl->id);
        uniform_binding->set(gl, uniform_binding->location, uniform_binding->desc.count, data);
    }
    uniform_binding->data = NULL;

    return 0;
}

int ngli_pipeline_gl_update_texture(struct pipeline *s, int index, struct texture *texture)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    struct texture_binding *texture_binding = ngli_darray_get(&s_priv->texture_bindings, index);
    ngli_assert(texture_binding);

    texture_binding->texture = texture;

    return 0;
}

int ngli_pipeline_gl_update_buffer(struct pipeline *s, int index, struct buffer *buffer)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    struct buffer_binding *buffer_binding = ngli_darray_get(&s_priv->buffer_bindings, index);
    ngli_assert(buffer_binding);

    if (buffer) {
        struct gctx_gl *gctx_gl = (struct gctx_gl *)s->gctx;
        struct glcontext *gl = gctx_gl->glcontext;
        const struct limits *limits = &gl->limits;
        if (buffer_binding->type == NGLI_TYPE_UNIFORM_BUFFER &&
            buffer->size > limits->max_uniform_block_size) {
            LOG(ERROR, "buffer %s size (%d) exceeds max uniform block size (%d)",
                buffer_binding->desc.name, buffer->size, limits->max_uniform_block_size);
            return NGL_ERROR_LIMIT_EXCEEDED;
        }
    }

    buffer_binding->buffer = buffer;

    return 0;
}

void ngli_pipeline_gl_draw(struct pipeline *s, int nb_vertices, int nb_instances)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct pipeline_graphics *graphics = &s->graphics;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    ngli_glstate_update(gctx, &graphics->state);
    ngli_glstate_update_scissor(gctx, gctx_gl->scissor);
    ngli_glstate_use_program(gctx, program_gl->id);
    set_uniforms(s, gl);
    set_buffers(s, gl);
    set_textures(s, gl);
    bind_vertex_attribs(s, gl);

    if (s_priv->nb_unbound_attributes) {
        LOG(ERROR, "pipeline has unbound vertex attributes");
        return;
    }

    if (nb_instances > 1 && !(gl->features & NGLI_FEATURE_DRAW_INSTANCED)) {
        LOG(ERROR, "context does not support instanced draws");
        return;
    }

    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    if (nb_instances > 1)
        ngli_glDrawArraysInstanced(gl, gl_topology, 0, nb_vertices, nb_instances);
    else
        ngli_glDrawArrays(gl, gl_topology, 0, nb_vertices);

    unbind_vertex_attribs(s, gl);

    s_priv->insert_memory_barriers(s);
}

void ngli_pipeline_gl_draw_indexed(struct pipeline *s, struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct pipeline_graphics *graphics = &s->graphics;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    ngli_glstate_update(gctx, &graphics->state);
    ngli_glstate_update_scissor(gctx, gctx_gl->scissor);
    ngli_glstate_use_program(gctx, program_gl->id);
    set_uniforms(s, gl);
    set_buffers(s, gl);
    set_textures(s, gl);
    bind_vertex_attribs(s, gl);

    if (s_priv->nb_unbound_attributes) {
        LOG(ERROR, "pipeline has unbound vertex attributes");
        return;
    }

    if (nb_instances > 1 && !(gl->features & NGLI_FEATURE_DRAW_INSTANCED)) {
        LOG(ERROR, "context does not support instanced draws");
        return;
    }

    ngli_assert(indices);
    const struct buffer_gl *indices_gl = (const struct buffer_gl *)indices;
    const GLenum gl_indices_type = get_gl_indices_type(indices_format);
    ngli_glBindBuffer(gl, GL_ELEMENT_ARRAY_BUFFER, indices_gl->id);

    const GLenum gl_topology = ngli_topology_get_gl_topology(graphics->topology);
    if (nb_instances > 1)
        ngli_glDrawElementsInstanced(gl, gl_topology, nb_indices, gl_indices_type, 0, nb_instances);
    else
        ngli_glDrawElements(gl, gl_topology, nb_indices, gl_indices_type, 0);

    unbind_vertex_attribs(s, gl);

    s_priv->insert_memory_barriers(s);
}

void ngli_pipeline_gl_dispatch(struct pipeline *s, int nb_group_x, int nb_group_y, int nb_group_z)
{
    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    struct program_gl *program_gl = (struct program_gl *)s->program;

    ngli_glstate_use_program(gctx, program_gl->id);
    set_uniforms(s, gl);
    set_buffers(s, gl);
    set_textures(s, gl);

    ngli_glDispatchCompute(gl, nb_group_x, nb_group_y, nb_group_z);

    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    s_priv->insert_memory_barriers(s);
}

void ngli_pipeline_gl_freep(struct pipeline **sp)
{
    if (!*sp)
        return;

    struct pipeline *s = *sp;
    struct pipeline_gl *s_priv = (struct pipeline_gl *)s;
    ngli_darray_reset(&s_priv->uniform_bindings);
    ngli_darray_reset(&s_priv->texture_bindings);
    ngli_darray_reset(&s_priv->buffer_bindings);
    ngli_darray_reset(&s_priv->attribute_bindings);

    struct gctx *gctx = s->gctx;
    struct gctx_gl *gctx_gl = (struct gctx_gl *)gctx;
    struct glcontext *gl = gctx_gl->glcontext;
    ngli_glDeleteVertexArrays(gl, 1, &s_priv->vao_id);

    ngli_freep(sp);
}
