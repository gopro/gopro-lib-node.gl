/*
 * Copyright 2020 GoPro Inc.
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

#include "memory.h"
#include "text.h"

extern const struct text_cls ngli_text_builtin;
extern const struct text_cls ngli_text_external;

struct text *ngli_text_create(struct ngl_ctx *ctx)
{
    struct text *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_text_init(struct text *s, const struct text_config *cfg)
{
    s->config = *cfg;

    ngli_darray_init(&s->chars, sizeof(struct char_info), 0);

    s->cls = cfg->fontfile ? &ngli_text_external : &ngli_text_builtin;
    if (s->cls->priv_size) {
        s->priv_data = ngli_calloc(1, s->cls->priv_size);
        if (!s->priv_data)
            return NGL_ERROR_MEMORY;
    }
    return s->cls->init(s);
}

int ngli_text_set_string(struct text *s, const char *str)
{
    ngli_darray_clear(&s->chars);
    return s->cls->set_string(s, str);
}

void ngli_text_freep(struct text **sp)
{
    struct text *s = *sp;
    if (s->cls && s->cls->reset)
        s->cls->reset(s);
    ngli_freep(&s->priv_data);
    ngli_darray_reset(&s->chars);
    ngli_freep(sp);
}
