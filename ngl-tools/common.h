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

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define ARRAY_NB(x) ((int)(sizeof(x) / sizeof(*(x))))

#define DEFAULT_WIDTH  640
#define DEFAULT_HEIGHT 360

int64_t gettime(void);
int64_t gettime_relative(void);
double clipd(double v, double min, double max);
int clipi(int v, int min, int max);
int64_t clipi64(int64_t v, int64_t min, int64_t max);
void get_viewport(int width, int height, const int *aspect_ratio, int *vp);
char *get_text_file_content(const char *filename);

#endif
