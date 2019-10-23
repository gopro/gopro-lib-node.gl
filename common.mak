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

PYTHON_MAJOR = 2

#
# User configuration
#
SHARED     ?= no
DEBUG      ?= no
SMALL      ?= no
COVERAGE   ?= no
CURL       ?= curl
INSTALL    ?= install
PKG_CONFIG ?= pkg-config
PYTHON     ?= python$(if $(shell which python$(PYTHON_MAJOR) 2> /dev/null),$(PYTHON_MAJOR),)
TAR        ?= tar
TARGET_OS  ?= $(shell uname -s)
ARCH       ?= $(shell uname -m)

ifneq ($(shell $(PYTHON) -c "import sys;print(sys.version_info.major)"),$(PYTHON_MAJOR))
$(error "Python $(PYTHON_MAJOR) not found")
endif

ifeq ($(OS),Windows_NT)
	TARGET_OS   = MinGW-w64
	PREFIX     ?= C:/msys64/usr/local
else
	PREFIX     ?= /usr/local
endif

define capitalize
$(shell echo $1 | tr a-z- A-Z_)
endef

PROJECT_CFLAGS := $(CFLAGS) -Wall -O2 -Werror=missing-prototypes \
                  -std=c99 \
                  -DTARGET_$(call capitalize,$(TARGET_OS)) \
                  -DARCH_$(call capitalize,$(ARCH))
PROJECT_LDLIBS := $(LDLIBS)

ifeq ($(COVERAGE),yes)
	PROJECT_CFLAGS += --coverage
	PROJECT_LDLIBS += --coverage
	DEBUG = yes
endif
ifeq ($(DEBUG),yes)
	PROJECT_CFLAGS += -g
endif
ifeq ($(SMALL),yes)
	PROJECT_CFLAGS += -DCONFIG_SMALL
endif

ifeq ($(TARGET_OS),MinGW-w64)
	EXESUF = .exe
endif # MinGW
