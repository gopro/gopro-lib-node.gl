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

#include <stdio.h>
#include <string.h>

#include "spirv.h"
#include "utils.h"

int ngli_spirv_get_name_location(const uint32_t *code, size_t size,
                                 const char *name)
{
    if (size < 5 * sizeof(*code))
        return -1;

    const uint32_t magic       = code[0];
    const uint32_t version     = code[1];
    //const uint32_t gen_magic = code[2];
    //const uint32_t bound     = code[3];
    //const uint32_t reserved  = code[4];

    if (magic != 0x07230203)
        return -1;
    if (version != 0x00010000) // XXX: allow more?
        return -1;

    code += 5;
    size -= 5 * sizeof(*code);

    int target_id = -1;

    while (size > 0) {
        const uint32_t opcode0    = code[0];
        const uint16_t opcode     = opcode0 & 0xffff;
        const uint16_t word_count = opcode0 >> 16;

        // check instruction size
        const uint32_t instruction_size = word_count * sizeof(*code);
        if (size < instruction_size)
            return -1;

        switch(opcode) {
            // OpName
            case 5: {
                const uint32_t target = code[1];
                const char *target_name = (const char *)&code[2];
                if (!strncmp(name, target_name, (word_count - 2) * sizeof(*code)))
                    target_id = target;
                break;
            }

            // OpMemberName - TODO: implement proper reflection
            // this is a workaround to check if there is an uniforn
            case 6: {
                const uint32_t type = code[1];
                const uint32_t index = code[2];
                const char *target_name = (const char *)&code[3];
                if (!strncmp(name, target_name, (word_count - 3 * sizeof(*code))))
                    return index;
                break;
            }

            // OpDecorate
            case 71: {
                const uint32_t target     = code[1];
                const uint32_t decoration = code[2];
                if (target == target_id && decoration == 30 /* location */)
                    return code[3];
                break;
            }
        }
        code += word_count;
        size -= instruction_size;
    }

    return -1;
}

struct spirv_header
{
    uint32_t magic;
    uint32_t version;
    uint32_t gen_magic;
    uint32_t bound;
    uint32_t reserved;
};

struct shader_type_internal
{
    const char *name;
    struct shader_variable_reflection variables[8];
    uint16_t size;
    uint8_t nb_variables;
};

struct shader_internal
{
    struct shader_type_internal types[64];
    uint16_t sizes[64];

    uint8_t buffer_type_indices[64];
    uint8_t nb_buffers;
};

int ngli_spirv_create_reflection(const uint32_t *code, size_t size, struct shader_reflection *s)
{
    // header
    if (size < sizeof(struct spirv_header))
        return -1;

    struct spirv_header *header = (struct spirv_header*)code;
    if (header->magic != 0x07230203)
        return -1;
    if (header->version != 0x00010000) // XXX: allow more?
        return -1;

    code += sizeof(struct spirv_header) / sizeof(uint32_t);
    size -= sizeof(struct spirv_header);

    // data
    struct shader_internal internal = {0};
    while (size > 0) {
        const uint32_t opcode0    = code[0];
        const uint16_t opcode     = opcode0 & 0xffff;
        const uint16_t word_count = opcode0 >> 16;

        // check instruction size
        const uint32_t instruction_size = word_count * sizeof(uint32_t);
        if (size < instruction_size)
            return -1;

        switch(opcode) {
            // OpName
            case 5: {
                const uint32_t type_id = code[1];
                const char *name = (const char *)&code[2];

                struct shader_type_internal *type = &internal.types[type_id];
                type->name = name;
                break;
            }

            // OpMemberName
            case 6: {
                const uint32_t type_id = code[1];
                const uint32_t variable_index = code[2];
                const char *name = (const char *)&code[3];

                struct shader_type_internal *type = &internal.types[type_id];
                type->nb_variables++;

                struct shader_variable_reflection *variable = &type->variables[variable_index];
                variable->name = name;
                //variable->hash = ngli_crc32(name);
                break;
            }

            // OpTypeFloat
            case 22: {
                const uint32_t type_id = code[1];
                const uint32_t type_size = code[2];
                internal.sizes[type_id] = type_size / 8;
                break;
            }

            // OpTypeVector
            case 23: {
                const uint32_t type_id = code[1];
                const uint32_t component_type_id = code[2];
                const uint32_t component_count = code[3];
                internal.sizes[type_id] = internal.sizes[component_type_id] * component_count;
                break;
            }

            // OpTypeMatrix
            case 24: {
                const uint32_t type_id = code[1];
                const uint32_t column_type_id = code[2];
                const uint32_t column_count = code[3];
                internal.sizes[type_id] = internal.sizes[column_type_id] * column_count;
                break;
            }

            // OpTypeStruct
            case 30: {
                const uint32_t type_id = code[1];

                struct shader_type_internal *type = &internal.types[type_id];
                const uint8_t last_variable_index = type->nb_variables-1;
                const uint32_t member_type_id = code[2 + last_variable_index];
                type->size = type->variables[last_variable_index].offset + internal.sizes[member_type_id];
                break;
            }

            // OpDecorate
            case 71: {
                const uint32_t type_id = code[1];
                const uint32_t decoration = code[2];
                switch (decoration) {
                    // Block
                    case 2: {
                        internal.buffer_type_indices[internal.nb_buffers++] = type_id;
                        break;
                    }
                }
                break;
            }

            // OpMemberDecorate
            case 72: {
                const uint32_t type_id = code[1];
                const uint32_t variable_index = code[2];
                const uint32_t decoration = code[3];

                // Offset
                if(decoration == 35)
                {
                    const uint32_t offset = code[4];
                    struct shader_variable_reflection *variable = &internal.types[type_id].variables[variable_index];
                    variable->offset = offset;
                }
                break;
            }
        }
        code += word_count;
        size -= instruction_size;
    }

    // allocate buffers
    s->nb_buffers = internal.nb_buffers;
    s->buffers = calloc(s->nb_buffers, sizeof(*s->buffers));
    for (uint32_t i=0; i<s->nb_buffers; i++) {
        const uint8_t buffer_type_id = internal.buffer_type_indices[i];
        struct shader_type_internal *type = &internal.types[buffer_type_id];

        struct shader_buffer_reflection *buffer = &s->buffers[i];
        buffer->size = type->size;
        buffer->nb_variables = type->nb_variables;
        buffer->variables = calloc(buffer->nb_variables, sizeof(*buffer->variables));
        memcpy(buffer->variables, type->variables, buffer->nb_variables * sizeof(*buffer->variables));
    }

    return 0;
}

void ngli_spirv_destroy_reflection(struct shader_reflection *s)
{
    if (s->buffers) {
        for (uint8_t i=0; i<s->nb_buffers; i++) {
            struct shader_buffer_reflection *buffer = &s->buffers[i];
            if (buffer->variables)
                free(buffer->variables);
        }
        free(s->buffers);
    }
}
