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

#ifndef SPIRV_H
#define SPIRV_H

#include <stdint.h>

struct shader_variable_reflection
{
    const char* name; //TODO: should be hash
    uint32_t hash;
    uint16_t offset;
};

struct shader_buffer_reflection
{
    struct shader_variable_reflection* variables;
    uint8_t nb_variables;
    uint16_t size;
};

struct shader_reflection
{
    struct shader_buffer_reflection *buffers;
    uint8_t nb_buffers;
};

int ngli_spirv_get_name_location(const uint32_t *code, size_t size,
                                 const char *name);

int ngli_spirv_create_reflection(const uint32_t *code, size_t size, struct shader_reflection *s);
void ngli_spirv_destroy_reflection(struct shader_reflection *s);

#endif
