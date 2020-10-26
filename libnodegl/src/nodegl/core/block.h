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

#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#include "buffer.h"
#include "darray.h"
#include "program.h" // MAX_ID_LEN

#ifdef __cplusplus
extern "C" {
#endif

enum block_layout {
    NGLI_BLOCK_LAYOUT_STD140,
    NGLI_BLOCK_LAYOUT_STD430,
    NGLI_BLOCK_NB_LAYOUTS
};

struct block_field {
    char name[MAX_ID_LEN];
    int type;
    int count;
    int offset;
    int size;
    int stride;
};

struct block {
    int type; // NGLI_TYPE_UNIFORM_BUFFER or NGLI_TYPE_STORAGE_BUFFER
    enum block_layout layout;
    struct darray fields; // block_field
    int size;
};

void ngli_block_init(struct block *s, enum block_layout layout);
int ngli_block_add_field(struct block *s, const char *name, int type, int count);
void ngli_block_datacopy(const struct block_field *fi, uint8_t *dst, const uint8_t *src);
void ngli_block_reset(struct block *s);

#ifdef __cplusplus
}
#endif

#endif
