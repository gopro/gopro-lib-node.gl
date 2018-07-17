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

        if (opcode == 5) { // OpName
            if (size < 3 * sizeof(*code))
                return -1;
            const uint32_t target = code[1];
            const char *target_name = (const char *)&code[2];
            if (!strncmp(name, target_name, (size - 2) * sizeof(*code)))
                target_id = target;
        } else if (opcode == 71 && size >= 4 * sizeof(*code)) { // OpDecorate
            const uint32_t target     = code[1];
            const uint32_t decoration = code[2];
            if (target == target_id && decoration == 30 /* location */)
                return code[3];
        }

        code += word_count;
        size -= word_count * sizeof(*code);
    }

    return -1;
}
