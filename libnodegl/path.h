/*
 * Copyright 2020-2021 GoPro Inc.
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

#ifndef PATH_H
#define PATH_H

#include "distmap.h"

struct path;

struct path *ngli_path_create(void);

int ngli_path_move_to(struct path *s, const float *to);
int ngli_path_line_to(struct path *s, const float *to);
int ngli_path_bezier2_to(struct path *s, const float *ctl, const float *to);
int ngli_path_bezier3_to(struct path *s, const float *ctl0, const float *ctl1, const float *to);

int ngli_path_init(struct path *s, int precision);

void ngli_path_evaluate(struct path *s, float *dst, float distance);

int ngli_path_init_distmap(const struct path *s, struct distmap *d);

void ngli_path_freep(struct path **sp);

#endif
