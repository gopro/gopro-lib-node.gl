#
# Copyright 2020 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

SHAPE_TEST_NAMES =              \
    precision_iovar             \
    triangle                    \
    triangle_cull_back          \
    triangle_cull_front         \
    triangles_mat4_attribute    \
    quad                        \
    quad_cull_back              \
    quad_cull_front             \
    circle                      \
    circle_cull_back            \
    circle_cull_front           \
    diamond_colormask           \
    geometry                    \
    geometry_normals            \
    geometry_indices            \
    geometry_normals_indices    \
    geometry_rtt                \
    geometry_rtt_depth          \
    morphing                    \
    cropboard                   \
    cropboard_indices           \

ifneq ($(DISABLE_TESTS_SAMPLES),yes)
SHAPE_TEST_NAMES += geometry_rtt_samples \
                    triangle_msaa        \

endif

$(eval $(call DECLARE_REF_TESTS,shape,$(SHAPE_TEST_NAMES)))
