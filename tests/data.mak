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

DATA_TEST_UNIFORM_NAMES =           \
    single_float                    \
    single_int                      \
    single_mat4                     \
    single_quat_mat4                \
    single_quat_vec4                \
    single_vec2                     \
    single_vec3                     \
    single_vec4                     \
    animated_quat_mat4              \
    animated_quat_vec4              \
    array_float                     \
    array_vec2                      \
    array_vec3                      \
    array_vec4                      \

DATA_TEST_BLOCK_NAMES =             \
    $(DATA_TEST_UNIFORM_NAMES)      \
    animated_buffer_float           \
    animated_buffer_vec2            \
    animated_buffer_vec3            \
    animated_buffer_vec4            \
    array_int                       \
    array_ivec2                     \
    array_ivec3                     \
    array_ivec4                     \
    array_mat4                      \

DATA_TEST_NAMES      += $(addsuffix _uniform,$(DATA_TEST_UNIFORM_NAMES))
DATA_TEST_NAMES      += $(addsuffix _std140,$(DATA_TEST_BLOCK_NAMES))

ifneq ($(TARGET_OS),Darwin)
DATA_TEST_NAMES      += $(addsuffix _std430,$(DATA_TEST_BLOCK_NAMES))
endif

$(eval $(call DECLARE_REF_TESTS,data,$(DATA_TEST_NAMES)))
