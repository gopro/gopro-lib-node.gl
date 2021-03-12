/*
 * Copyright 2016 GoPro Inc.
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

#include "topology_ngfx.h"
#include <map>
using namespace ngfx;
using namespace std;

static const map<uint32_t, PrimitiveTopology> ngfx_primitive_topology_map = {
    { NGLI_PRIMITIVE_TOPOLOGY_POINT_LIST     , PRIMITIVE_TOPOLOGY_POINT_LIST },
    { NGLI_PRIMITIVE_TOPOLOGY_LINE_LIST      , PRIMITIVE_TOPOLOGY_LINE_LIST },
    { NGLI_PRIMITIVE_TOPOLOGY_LINE_STRIP     , PRIMITIVE_TOPOLOGY_LINE_STRIP },
    { NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST  , PRIMITIVE_TOPOLOGY_TRIANGLE_LIST },
    { NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP , PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP },
};

PrimitiveTopology to_ngfx_topology(int topology)
{
    return ngfx_primitive_topology_map.at(topology);
}
