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

TAR  ?= tar
CURL ?= curl

SXPLAYER_VERSION ?= 9.7.0
PKGCONF_VERSION ?= 1.7.3
RENDERDOC_VERSION ?= 1.12

DEPENDENCIES = sxplayer
ifeq ($(TARGET_OS),Windows)
DEPENDENCIES += pkgconf
endif

ifeq ($(DEBUG_GPU_CAPTURE),yes)
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),MinGW-w64 Windows))
RENDERDOC_TARGET = RenderDoc_$(RENDERDOC_VERSION)_64
else ifeq ($(TARGET_OS),Linux)
RENDERDOC_TARGET = renderdoc_$(RENDERDOC_VERSION)
else
$(error RenderDoc is not supported on this system)
endif
DEPENDENCIES += renderdoc
endif

all: $(DEPENDENCIES)

# Note for developers: in order to customize the sxplayer you're building
# against, you can use your own sources post-install:
#
#     % unlink sxplayer
#     % ln -snf /path/to/sxplayer.git sxplayer
#     % touch /path/to/sxplayer.git
#
# The `touch` command makes sure the source target directory is more recent
# than the prerequisite directory of the sxplayer rule. If this isn't true, the
# symlink will be re-recreated on the next `make` call
sxplayer: sxplayer-$(SXPLAYER_VERSION)
ifneq ($(TARGET_OS),$(filter $(TARGET_OS),MinGW-w64 Windows))
	ln -snf $< $@
else
	cp -r $< $@
endif

sxplayer-$(SXPLAYER_VERSION): sxplayer-$(SXPLAYER_VERSION).tar.gz
	$(TAR) -xf $<

sxplayer-$(SXPLAYER_VERSION).tar.gz:
	$(CURL) -L https://github.com/Stupeflix/sxplayer/archive/v$(SXPLAYER_VERSION).tar.gz -o $@

pkgconf: pkgconf-$(PKGCONF_VERSION)
	cp -r $< $@

pkgconf-$(PKGCONF_VERSION): pkgconf-$(PKGCONF_VERSION).tar.xz
	$(TAR) -xf $<

pkgconf-$(PKGCONF_VERSION).tar.xz:
	$(CURL) -L https://distfiles.dereferenced.org/pkgconf/$@ -o $@

renderdoc: $(RENDERDOC_TARGET)
ifneq ($(TARGET_OS),$(filter $(TARGET_OS),MinGW-w64 Windows))
	ln -snf $< $@
else
	cp -r $< $@
endif

RenderDoc_$(RENDERDOC_VERSION)_64: RenderDoc_$(RENDERDOC_VERSION)_64.zip
	unzip -q -o $<

RenderDoc_$(RENDERDOC_VERSION)_64.zip renderdoc_$(RENDERDOC_VERSION).tar.gz:
	$(CURL) -L https://renderdoc.org/stable/$(RENDERDOC_VERSION)/$@ -o $@

renderdoc_$(RENDERDOC_VERSION): renderdoc_$(RENDERDOC_VERSION).tar.gz
	$(TAR) -xf $<

.PHONY: all
