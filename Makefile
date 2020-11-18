#
# Copyright 2016 GoPro Inc.
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

PREFIX ?= $(PWD)/nodegl-env

PYTHON_MAJOR = 3

#
# User configuration
#
DEBUG      ?= no
COVERAGE   ?= no
CURL       ?= curl
PYTHON     ?= python$(if $(shell which python$(PYTHON_MAJOR) 2> /dev/null),$(PYTHON_MAJOR),)
TAR        ?= tar
TARGET_OS  ?= $(shell uname -s)

ifneq ($(shell $(PYTHON) -c "import sys;print(sys.version_info.major)"),$(PYTHON_MAJOR))
$(error "Python $(PYTHON_MAJOR) not found")
endif

SXPLAYER_VERSION ?= 9.6.0

ACTIVATE = $(PREFIX)/bin/activate

RPATH_LDFLAGS ?= -Wl,-rpath,$(PREFIX)/lib

MESON_SETUP   = meson setup --prefix=$(PREFIX) --pkg-config-path=$(PREFIX)/lib/pkgconfig -Drpath=true
# MAKEFLAGS= is a workaround for the issue described here:
# https://github.com/ninja-build/ninja/issues/1139#issuecomment-724061270
MESON_COMPILE = MAKEFLAGS= meson compile
MESON_INSTALL = meson install
ifeq ($(COVERAGE),yes)
MESON_SETUP += -Db_coverage=true
DEBUG = yes
endif
ifeq ($(DEBUG),yes)
MESON_SETUP += --buildtype=debugoptimized
else
MESON_SETUP += --buildtype=release -Db_lto=true
endif
ifneq ($(V),)
MESON_COMPILE += -v
endif
NODEGL_DEBUG_OPTS-$(DEBUG_GL)    += gl
NODEGL_DEBUG_OPTS-$(DEBUG_MEM)   += mem
NODEGL_DEBUG_OPTS-$(DEBUG_SCENE) += scene
ifneq ($(NODEGL_DEBUG_OPTS-yes),)
NODEGL_DEBUG_OPTS = -Ddebug_opts=$(shell echo $(NODEGL_DEBUG_OPTS-yes) | tr ' ' ',')
endif

# Workaround Debian/Ubuntu bug; see https://github.com/mesonbuild/meson/issues/5925
ifeq ($(TARGET_OS),Linux)
DISTRIB_ID := $(or $(shell lsb_release -si 2>/dev/null),none)
ifeq ($(DISTRIB_ID),$(filter $(DISTRIB_ID),Ubuntu Debian))
MESON_SETUP += --libdir lib
endif
endif

ifneq ($(TESTS_SUITE),)
MESON_TESTS_SUITE_OPTS += --suite $(TESTS_SUITE)
endif

all: ngl-tools-install pynodegl-utils-install
	@echo
	@echo "    Install completed."
	@echo
	@echo "    You can now enter the venv with:"
	@echo "        . $(ACTIVATE)"
	@echo

ngl-tools-install: nodegl-install
	(. $(ACTIVATE) && $(MESON_SETUP) ngl-tools builddir/ngl-tools && $(MESON_COMPILE) -C builddir/ngl-tools && $(MESON_INSTALL) -C builddir/ngl-tools)

pynodegl-utils-install: pynodegl-utils-deps-install
	(. $(ACTIVATE) && pip -v install -e ./pynodegl-utils)

#
# pynodegl-install is in dependency to prevent from trying to install pynodegl
# from its requirements. Pulling pynodegl from requirements has two main issue:
# it tries to get it from PyPi (and we want to install the local pynodegl
# version), and it would fail anyway because pynodegl is currently not
# available on PyPi.
#
# We do not pull the requirements on Windows because of various issues:
# - PySide2 can't be pulled
# - Pillow fails to find zlib (required to be installed by the user outside the
#   Python virtualenv)
# - ngl-control can not currently work because of temporary files handling
#
# Still, we want the module to be installed so we can access the scene()
# decorator and other related utils.
#
pynodegl-utils-deps-install: pynodegl-install
ifneq ($(TARGET_OS),MinGW-w64)
	(. $(ACTIVATE) && pip install -r ./pynodegl-utils/requirements.txt)
endif

pynodegl-install: pynodegl-deps-install
	(. $(ACTIVATE) && PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig LDFLAGS=$(RPATH_LDFLAGS) pip -v install -e ./pynodegl)

pynodegl-deps-install: $(PREFIX) nodegl-install
	(. $(ACTIVATE) && pip install -r ./pynodegl/requirements.txt)

nodegl-install: nodegl-setup
	(. $(ACTIVATE) && $(MESON_COMPILE) -C builddir/libnodegl && $(MESON_INSTALL) -C builddir/libnodegl)

nodegl-setup: sxplayer-install
	(. $(ACTIVATE) && $(MESON_SETUP) $(NODEGL_DEBUG_OPTS) libnodegl builddir/libnodegl)

sxplayer-install: sxplayer $(PREFIX)
	(. $(ACTIVATE) && $(MESON_SETUP) sxplayer builddir/sxplayer && $(MESON_COMPILE) -C builddir/sxplayer && $(MESON_INSTALL) -C builddir/sxplayer)

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
ifneq ($(TARGET_OS),MinGW-w64)
	ln -snf $< $@
else
	cp -r $< $@
endif

sxplayer-$(SXPLAYER_VERSION): sxplayer-$(SXPLAYER_VERSION).tar.gz
	$(TAR) xf $<

sxplayer-$(SXPLAYER_VERSION).tar.gz:
	$(CURL) -L https://github.com/Stupeflix/sxplayer/archive/v$(SXPLAYER_VERSION).tar.gz -o $@

#
# We do not pull meson from pip on Windows for the same reasons we don't pull
# Pillow and PySide2. We require the users to have it on their system.
#
$(PREFIX):
ifeq ($(TARGET_OS),MinGW-w64)
	$(PYTHON) -m venv --system-site-packages $(PREFIX)
else
	$(PYTHON) -m venv $(PREFIX)
	(. $(ACTIVATE) && pip install meson ninja)
endif

tests: nodegl-tests tests-setup
	(. $(ACTIVATE) && meson test $(MESON_TESTS_SUITE_OPTS) -C builddir/tests)

tests-setup: ngl-tools-install pynodegl-utils-install
	(. $(ACTIVATE) && $(MESON_SETUP) builddir/tests tests)

nodegl-tests: nodegl-install
	(. $(ACTIVATE) && meson test -C builddir/libnodegl)

nodegl-%: nodegl-setup
	(. $(ACTIVATE) && $(MESON_COMPILE) -C builddir/libnodegl $(subst nodegl-,,$@))

clean_py:
	$(RM) pynodegl/nodes_def.pyx
	$(RM) pynodegl/pynodegl.c
	$(RM) pynodegl/pynodegl.*.so
	$(RM) -r pynodegl/build
	$(RM) -r pynodegl/pynodegl.egg-info
	$(RM) -r pynodegl/.eggs
	$(RM) -r pynodegl-utils/pynodegl_utils.egg-info
	$(RM) -r pynodegl-utils/.eggs

clean: clean_py
	$(RM) -r builddir/sxplayer
	$(RM) -r builddir/libnodegl
	$(RM) -r builddir/ngl-tools
	$(RM) -r builddir/tests

# You need to build and run with COVERAGE set to generate data.
# For example: `make clean && make -j8 tests COVERAGE=yes`
# We don't use `meson coverage` here because of
# https://github.com/mesonbuild/meson/issues/7895
coverage-html:
	(. $(ACTIVATE) && ninja -C builddir/libnodegl coverage-html)
coverage-xml:
	(. $(ACTIVATE) && ninja -C builddir/libnodegl coverage-xml)

.PHONY: all
.PHONY: ngl-tools-install
.PHONY: pynodegl-utils-install pynodegl-utils-deps-install
.PHONY: pynodegl-install pynodegl-deps-install
.PHONY: nodegl-install nodegl-setup
.PHONY: sxplayer-install
.PHONY: tests tests-setup
.PHONY: clean clean_py
.PHONY: coverage-html coverage-xml
