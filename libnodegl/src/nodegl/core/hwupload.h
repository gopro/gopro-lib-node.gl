/*
 * Copyright 2017 GoPro Inc.
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

#ifndef HWUPLOAD_H
#define HWUPLOAD_H

#include <stdlib.h>
#include <sxplayer.h>

#include "hwconv.h"
#include "image.h"
#include "nodegl.h"

#define HWMAP_FLAG_FRAME_OWNER (1 << 0)

#ifdef __cplusplus
extern "C" {
#endif

struct hwmap_class {
    const char *name;
    int flags;
    size_t priv_size;
    int (*init)(struct ngl_node *node, struct sxplayer_frame *frame);
    int (*map_frame)(struct ngl_node *node, struct sxplayer_frame *frame);
    void (*uninit)(struct ngl_node *node);
};

struct hwupload {
    const struct hwmap_class *hwmap_class;
    void *hwmap_priv_data;
    struct image mapped_image;
    int require_hwconv;
    struct hwconv hwconv;
    int hwconv_initialized;
};

int ngli_hwupload_upload_frame(struct ngl_node *node);
void ngli_hwupload_uninit(struct ngl_node *node);

#ifdef __cplusplus
}
#endif

#endif /* HWUPLOAD_H */
