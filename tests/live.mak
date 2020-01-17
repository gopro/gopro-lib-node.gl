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

LIVE_TEST_BASE_NAMES =     \
    single_float           \
    single_int             \
    single_mat4            \
    single_quat_mat4       \
    single_quat_vec4       \
    single_vec2            \
    single_vec3            \
    single_vec4            \
    trf_single_mat4        \

LIVE_TEST_NAMES += $(addsuffix _uniform,$(LIVE_TEST_BASE_NAMES))
LIVE_TEST_NAMES += $(addsuffix _std140,$(LIVE_TEST_BASE_NAMES))

ifneq ($(TARGET_OS),Darwin)
LIVE_TEST_NAMES += $(addsuffix _std430,$(LIVE_TEST_BASE_NAMES))
endif

$(eval $(call DECLARE_REF_TESTS,live,$(LIVE_TEST_NAMES)))
