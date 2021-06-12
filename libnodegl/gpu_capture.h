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

#ifndef GPU_CAPTURE_H
#define GPU_CAPTURE_H

struct gpu_ctx;
struct gpu_capture_ctx;

struct gpu_capture_ctx *ngli_gpu_capture_ctx_create(struct gpu_ctx *s);
int ngli_gpu_capture_init(struct gpu_capture_ctx *s);
int ngli_gpu_capture_begin(struct gpu_capture_ctx *s);
int ngli_gpu_capture_end(struct gpu_capture_ctx *s);
void ngli_gpu_capture_freep(struct gpu_capture_ctx **sp);

#endif
