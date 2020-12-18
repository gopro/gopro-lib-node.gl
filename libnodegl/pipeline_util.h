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

#ifndef PIPELINE_UTIL_H
#define PIPELINE_UTIL_H
#include "pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

int pipeline_update_blocks(struct pipeline *s,  const struct pipeline_resource_params *params);
int pipeline_set_uniforms(struct pipeline *s);
int pipeline_update_uniform(struct pipeline *s, int index, const void *value);

#ifdef __cplusplus
}
#endif

#endif
