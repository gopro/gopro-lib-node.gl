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

#include "glincludes.h"
#include "topology_gl.h"

static const GLenum gl_primitive_topology_map[NGLI_PRIMITIVE_TOPOLOGY_NB] = {
    [NGLI_PRIMITIVE_TOPOLOGY_POINT_LIST]     = GL_POINTS,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_LIST]      = GL_LINES,
    [NGLI_PRIMITIVE_TOPOLOGY_LINE_STRIP]     = GL_LINE_STRIP,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST]  = GL_TRIANGLES,
    [NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = GL_TRIANGLE_STRIP,
};

GLenum ngli_topology_get_gl_topology(int topology)
{
    return gl_primitive_topology_map[topology];
}
