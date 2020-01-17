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

MEDIA_TEST_NAMES =        \
    phases_display        \
    phases_resources      \

$(eval $(call DECLARE_REF_TESTS,media,$(MEDIA_TEST_NAMES)))

MEDIA_TEST_FILE = ngl-media-test.nut

$(MEDIA_TEST_FILE):
	ffmpeg -nostdin -nostats -f lavfi -i testsrc=d=30:r=60 -c:v ffv1 -y $@

$(TESTS_media): $(MEDIA_TEST_FILE)
