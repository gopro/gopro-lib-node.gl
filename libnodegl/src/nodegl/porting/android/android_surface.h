/*
 * Copyright 2016-2017 GoPro Inc.
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

#ifndef ANDROID_SURFACE_H
#define ANDROID_SURFACE_H

#include <libavcodec/mediacodec.h>

struct android_surface;
struct android_surface *ngli_android_surface_new(int tex_id, void *handler);
void ngli_android_surface_free(struct android_surface **surface);
void *ngli_android_surface_get_surface(struct android_surface *surface);
int ngli_android_surface_attach_to_gl_context(struct android_surface *surface, int tex_id);
int ngli_android_surface_detach_from_gl_context(struct android_surface *surface);
int ngli_android_surface_render_buffer(struct android_surface *surface, AVMediaCodecBuffer *buffer, float *matrix);
void ngli_android_surface_signal_frame(struct android_surface *surface);

#endif /* ANDROID_SURFACE_H */
