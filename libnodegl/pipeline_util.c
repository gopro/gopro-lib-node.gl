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

#include "pipeline_util.h"
#include "memory.h"
#include "nodegl.h"
#include <string.h>

int pipeline_update_blocks(struct pipeline *s,  const struct pipeline_resource_params *params) {
    /* Uniform blocks */
    memcpy(s->ublock, params->ublock, sizeof(s->ublock));
    memcpy(s->ubuffer, params->ubuffer, sizeof(s->ubuffer));
    for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const struct buffer *ubuffer = params->ubuffer[i];
        if (!ubuffer)
            continue;
        const struct block *ublock = params->ublock[i];
        s->udata[i] = (uint8_t*)ngli_realloc(s->udata[i], ublock->size);
        if (!s->udata[i])
            return NGL_ERROR_MEMORY;
        memset(s->udata[i], 0, ublock->size);
    }
    return 0;
}

int pipeline_set_uniforms(struct pipeline *s)
{
    for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const uint8_t *udata = s->udata[i];
        if (!udata)
            continue;
        struct buffer *ubuffer = s->ubuffer[i];
        const struct block *ublock = s->ublock[i];
        int ret = ngli_buffer_upload(ubuffer, udata, ublock->size, 0);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int pipeline_update_uniform(struct pipeline *s, int index, const void *value) {
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    const int stage = index >> 16;
    const int field_index = index & 0xffff;
    const struct block *p_block = s->ublock[stage];
    const struct block_field *field_info = (const struct block_field *)ngli_darray_data(&p_block->fields);
    const struct block_field *fi = &field_info[field_index];
    uint8_t *dst = s->udata[stage] + fi->offset;
    ngli_block_field_copy(fi, dst, (const uint8_t *)value);
    return 0;
}
