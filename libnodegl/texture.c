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

#include "gpu_ctx.h"
#include "texture.h"

struct texture *ngli_texture_create(struct gpu_ctx *gpu_ctx)
{
    return gpu_ctx->cls->texture_create(gpu_ctx);
}

int ngli_texture_init(struct texture *s, const struct texture_params *params)
{
    return s->gpu_ctx->cls->texture_init(s, params);
}

int ngli_texture_upload(struct texture *s, const uint8_t *data, int linesize)
{
    return s->gpu_ctx->cls->texture_upload(s, data, linesize);
}

int ngli_texture_generate_mipmap(struct texture *s)
{
    return s->gpu_ctx->cls->texture_generate_mipmap(s);
}

void ngli_texture_freep(struct texture **sp)
{
    if (!*sp)
        return;
    (*sp)->gpu_ctx->cls->texture_freep(sp);
}
