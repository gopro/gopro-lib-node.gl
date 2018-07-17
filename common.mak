#
# Copyright 2017 GoPro Inc.
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

#
# User configuration
#
PREFIX     ?= /usr/local
SHARED     ?= no
DEBUG      ?= no
SMALL      ?= no
PKG_CONFIG ?= pkg-config
PYTHON     ?= python
TARGET_OS  ?= $(shell uname -s)
ARCH       ?= $(shell uname -m)
#BACKEND    ?= gl
BACKEND    ?= vk

define capitalize
$(shell echo $1 | tr a-z- A-Z_)
endef

PROJECT_CFLAGS := $(CFLAGS) -Wall -O2 -Werror=missing-prototypes \
                  -std=c99 \
                  -DTARGET_$(call capitalize,$(TARGET_OS)) \
                  -DARCH_$(call capitalize,$(ARCH))

ifeq ($(DEBUG),yes)
	PROJECT_CFLAGS += -g
endif
ifeq ($(SMALL),yes)
	PROJECT_CFLAGS += -DCONFIG_SMALL
endif
PROJECT_LDLIBS := $(LDLIBS)

ifeq ($(TARGET_OS),MinGW-w64)
	EXESUF = .exe
endif # MinGW

ifeq ($(BACKEND),vk)
	PROJECT_CFLAGS += -DVULKAN_BACKEND
endif
