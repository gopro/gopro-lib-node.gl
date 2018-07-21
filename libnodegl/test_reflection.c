/*
 * Copyright 2016 GoPro Inc.
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
#include <stdlib.h>
#include <inttypes.h>

#include "spirv.h"

int main(int argc, char *argv[])
{
    const char *filepath = argv[1];
    FILE *file = fopen(filepath, "rb");
    if (!file)
        return -1;

    fseek(file, 0 ,SEEK_END);
    uint32_t shader_size = ftell(file);
    fseek(file, 0 ,SEEK_SET);

    char *shader_code = malloc(shader_size);
    if (fread(shader_code, 1, shader_size, file) != shader_size)
        return -1;
    fclose(file);

    struct shader_reflection s;
    if (ngli_spirv_create_reflection((uint32_t*)shader_code, shader_size, &s) != 0)
        return -1;

    // output reflection
    printf("%s\n", filepath);

    printf("\t%d buffers\n", s.nb_buffers);
    for (uint32_t i=0; i<s.nb_buffers; i++) {
        struct shader_buffer_reflection *buffer = &s.buffers[i];

        printf("\t\tsize: %d\n", s.buffers[i].size);
        for (uint32_t j=0; j<buffer->nb_variables; j++) {
            struct shader_variable_reflection *variable = &buffer->variables[j];
            printf("\t\t\toffset: %d\n", variable->offset);
            printf("\t\t\tname: %s\n", variable->name);
        }
    }
    ngli_spirv_destroy_reflection(&s);

    //TODO: replace variable name with hash - need to delete shader code after since we kept pointer on variable names
    free(shader_code);

    return 0;
}
