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

PREFIX          ?= venv
ifeq ($(TARGET_OS),Windows)
PREFIX_FULLPATH = $(shell wslpath -wa .)\$(PREFIX)
else
PREFIX_FULLPATH = $(PWD)/$(PREFIX)
endif

PYTHON_MAJOR = 3

#
# User configuration
#
DEBUG      ?= no
COVERAGE   ?= no
ifeq ($(TARGET_OS),Windows)
PYTHON     ?= python.exe
else
PYTHON     ?= python$(if $(shell which python$(PYTHON_MAJOR) 2> /dev/null),$(PYTHON_MAJOR),)
endif
export TARGET_OS ?= $(shell uname -s)

DEBUG_GL    ?= no
DEBUG_MEM   ?= no
DEBUG_SCENE ?= no
export DEBUG_GPU_CAPTURE ?= no
TESTS_SUITE ?=
V           ?=

$(info PYTHON: $(PYTHON))
$(info PREFIX: $(PREFIX))
$(info PREFIX_FULLPATH: $(PREFIX_FULLPATH))

ifeq ($(TARGET_OS),Windows)
# Initialize VCVARS64 and VCPKG_DIR to a default value
# Note: the user should override this environment variable if needed
VCVARS64 ?= "$(shell powershell.exe .\\scripts\\find_vcvars64.ps1)"
VCPKG_DIR ?= C:\\vcpkg
PKG_CONF_DIR = external\\pkgconf\\build
CMD = PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 PKG_CONFIG_ALLOW_SYSTEM_LIBS=1 PKG_CONFIG="$(PREFIX_FULLPATH)\\Scripts\\pkg-config.exe" PKG_CONFIG_PATH="$(VCPKG_DIR)\\installed\\x64-windows\\lib\\pkgconfig" WSLENV=PKG_CONFIG/w:PKG_CONFIG_PATH/w:PKG_CONFIG_ALLOW_SYSTEM_LIBS/w:PKG_CONFIG_ALLOW_SYSTEM_CFLAGS/w cmd.exe /C
else
CMD =
endif

ifneq ($(shell $(CMD) $(PYTHON) -c "import sys;print(sys.version_info.major)"),$(PYTHON_MAJOR))
$(error "Python $(PYTHON_MAJOR) not found")
endif

ifeq ($(TARGET_OS),Windows)
ACTIVATE = $(CMD) $(VCVARS64) \&\& "$(PREFIX_FULLPATH)\\Scripts\\activate.bat"
else
ACTIVATE = . $(PREFIX_FULLPATH)/bin/activate
endif

RPATH_LDFLAGS ?= -Wl,-rpath,$(PREFIX_FULLPATH)/lib

ifeq ($(TARGET_OS),Windows)
MESON_SETUP_PARAMS  = \
    --prefix="$(PREFIX_FULLPATH)" --bindir="$(PREFIX_FULLPATH)\\Scripts" --includedir="$(PREFIX_FULLPATH)\\Include" \
    --libdir="$(PREFIX_FULLPATH)\\Lib" --pkg-config-path="$(VCPKG_DIR)\\installed\x64-windows\\lib\\pkgconfig;$(PREFIX_FULLPATH)\\Lib\\pkgconfig" -Drpath=true
MESON_SETUP         = meson setup --backend vs $(MESON_SETUP_PARAMS)
MESON_SETUP_NINJA   = meson setup --backend ninja $(MESON_SETUP_PARAMS)
else
MESON_SETUP         = meson setup --prefix=$(PREFIX_FULLPATH) --pkg-config-path=$(PREFIX_FULLPATH)/lib/pkgconfig -Drpath=true
endif
# MAKEFLAGS= is a workaround (not working on Windows due to incompatible Make
# syntax) for the issue described here:
# https://github.com/ninja-build/ninja/issues/1139#issuecomment-724061270
ifeq ($(TARGET_OS),Windows)
MESON_COMPILE = meson compile
else
MESON_COMPILE = MAKEFLAGS= meson compile
endif
MESON_INSTALL = meson install
ifeq ($(COVERAGE),yes)
MESON_SETUP += -Db_coverage=true
DEBUG = yes
endif
ifeq ($(DEBUG),yes)
MESON_SETUP += --buildtype=debugoptimized
else
MESON_SETUP += --buildtype=release
ifneq ($(TARGET_OS),MinGW-w64)
MESON_SETUP += -Db_lto=true
endif
endif
ifneq ($(V),)
MESON_COMPILE += -v
endif
NODEGL_DEBUG_OPTS-$(DEBUG_GL)    += gl
NODEGL_DEBUG_OPTS-$(DEBUG_MEM)   += mem
NODEGL_DEBUG_OPTS-$(DEBUG_SCENE) += scene
NODEGL_DEBUG_OPTS-$(DEBUG_GPU_CAPTURE) += gpu_capture
ifneq ($(NODEGL_DEBUG_OPTS-yes),)
NODEGL_DEBUG_OPTS = -Ddebug_opts=$(shell echo $(NODEGL_DEBUG_OPTS-yes) | tr ' ' ',')
endif
ifeq ($(DEBUG_GPU_CAPTURE),yes)
ifeq ($(TARGET_OS),Windows)
RENDERDOC_DIR = $(shell wslpath -wa .)\external\renderdoc
else
RENDERDOC_DIR = $(PWD)/external/renderdoc
endif
NODEGL_DEBUG_OPTS += -Drenderdoc_dir="$(RENDERDOC_DIR)"
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
ifeq ($(TARGET_OS),Windows)
	@echo "        (via Windows Command Prompt)"
	@echo "            cmd.exe"
	@echo "            $(PREFIX)\\\Scripts\\\activate.bat"
	@echo "        (via Windows PowerShell)"
	@echo "            powershell.exe"
	@echo "            $(PREFIX)\\\Scripts\\\Activate.ps1"
else
	@echo "        $(ACTIVATE)"
endif
	@echo

ngl-tools-install: nodegl-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_SETUP) ngl-tools builddir\\ngl-tools \&\& $(MESON_COMPILE) -C builddir\\ngl-tools \&\& $(MESON_INSTALL) -C builddir\\ngl-tools)
	$(CMD) xcopy /Y builddir\\ngl-tools\\*.dll "$(PREFIX_FULLPATH)\\Scripts\\."
else
	($(ACTIVATE) && $(MESON_SETUP) ngl-tools builddir/ngl-tools && $(MESON_COMPILE) -C builddir/ngl-tools && $(MESON_INSTALL) -C builddir/ngl-tools)
endif

pynodegl-utils-install: pynodegl-utils-deps-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& pip -v install -e pynodegl-utils)
else
	($(ACTIVATE) && pip -v install -e ./pynodegl-utils)
endif

#
# pynodegl-install is in dependency to prevent from trying to install pynodegl
# from its requirements. Pulling pynodegl from requirements has two main issue:
# it tries to get it from PyPi (and we want to install the local pynodegl
# version), and it would fail anyway because pynodegl is currently not
# available on PyPi.
#
# We do not pull the requirements on MinGW because of various issues:
# - PySide2 can't be pulled (required to be installed by the user outside the
#   Python virtualenv)
# - Pillow fails to find zlib (required to be installed by the user outside the
#   Python virtualenv)
# - ngl-control works partially, export cannot work because of our subprocess
#   usage, passing fd is not supported on Windows
#
# Still, we want the module to be installed so we can access the scene()
# decorator and other related utils.
#
pynodegl-utils-deps-install: pynodegl-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& pip install -r pynodegl-utils\\requirements.txt)
else ifneq ($(TARGET_OS),MinGW-w64)
	($(ACTIVATE) && pip install -r ./pynodegl-utils/requirements.txt)
endif

pynodegl-install: pynodegl-deps-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& pip -v install -e .\\pynodegl)
	$(CMD) xcopy /Y builddir\\sxplayer\\*.dll pynodegl\\.
	$(CMD) xcopy /Y /C builddir\\libnodegl\\*.dll pynodegl\\.
else
	($(ACTIVATE) && PKG_CONFIG_PATH=$(PREFIX_FULLPATH)/lib/pkgconfig LDFLAGS=$(RPATH_LDFLAGS) pip -v install -e ./pynodegl)
endif

pynodegl-deps-install: $(PREFIX) nodegl-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& pip install -r pynodegl\\requirements.txt)
else
	($(ACTIVATE) && pip install -r ./pynodegl/requirements.txt)
endif

nodegl-install: nodegl-setup
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_COMPILE) -C builddir\\libnodegl \&\& $(MESON_INSTALL) -C builddir\\libnodegl)
else
	($(ACTIVATE) && $(MESON_COMPILE) -C builddir/libnodegl && $(MESON_INSTALL) -C builddir/libnodegl)
endif

NODEGL_DEPS = sxplayer-install
ifeq ($(DEBUG_GPU_CAPTURE),yes)
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),MinGW-w64 Windows))
NODEGL_DEPS += renderdoc-install
endif
endif

nodegl-setup: $(NODEGL_DEPS)
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_SETUP) $(NODEGL_DEBUG_OPTS) libnodegl builddir\\libnodegl)
else
	($(ACTIVATE) && $(MESON_SETUP) $(NODEGL_DEBUG_OPTS) libnodegl builddir/libnodegl)
endif

pkg-config-install: external-download $(PREFIX)
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_SETUP) -Dtests=false external\\pkgconf builddir\\pkgconf \&\& $(MESON_COMPILE) -C builddir\\pkgconf \&\& $(MESON_INSTALL) -C builddir\\pkgconf)
	($(CMD) copy "$(PREFIX_FULLPATH)\\Scripts\\pkgconf.exe" "$(PREFIX_FULLPATH)\\Scripts\\pkg-config.exe")
endif

sxplayer-install: external-download pkg-config-install $(PREFIX)
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_SETUP) external\\sxplayer builddir\\sxplayer \&\& $(MESON_COMPILE) -C builddir\\sxplayer \&\& $(MESON_INSTALL) -C builddir\\sxplayer)
else
	($(ACTIVATE) && $(MESON_SETUP) external/sxplayer builddir/sxplayer && $(MESON_COMPILE) -C builddir/sxplayer && $(MESON_INSTALL) -C builddir/sxplayer)
endif

renderdoc-install: external-download pkg-config-install $(PREFIX)
ifeq ($(TARGET_OS),Windows)
	$(CMD) xcopy /Y "$(RENDERDOC_DIR)\renderdoc.dll" "$(PREFIX_FULLPATH)\Scripts\."
else
	cp $(RENDERDOC_DIR)/renderdoc.dll $(PREFIX_FULLPATH)/bin/
endif

external-download:
	$(MAKE) -C external

#
# We do not pull meson from pip on MinGW for the same reasons we don't pull
# Pillow and PySide2. We require the users to have it on their system.
#
$(PREFIX):
ifeq ($(TARGET_OS),Windows)
	($(CMD) $(PYTHON) -m venv "$@")
	($(ACTIVATE) \&\& pip install meson ninja)
else ifeq ($(TARGET_OS),MinGW-w64)
	$(PYTHON) -m venv --system-site-packages $@
else
	$(PYTHON) -m venv $@
	($(ACTIVATE) && pip install meson ninja)
endif

tests: nodegl-tests tests-setup
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& meson test $(MESON_TESTS_SUITE_OPTS) -C builddir\\tests)
else
	($(ACTIVATE) && meson test $(MESON_TESTS_SUITE_OPTS) -C builddir/tests)
endif

tests-setup: ngl-tools-install pynodegl-utils-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_SETUP_NINJA) builddir\\tests tests)
else
	($(ACTIVATE) && $(MESON_SETUP) builddir/tests tests)
endif

nodegl-tests: nodegl-install
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& meson test -C builddir\\libnodegl)
else
	($(ACTIVATE) && meson test -C builddir/libnodegl)
endif

nodegl-%: nodegl-setup
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) \&\& $(MESON_COMPILE) -C builddir\\libnodegl $(subst nodegl-,,$@))
else
	($(ACTIVATE) && $(MESON_COMPILE) -C builddir/libnodegl $(subst nodegl-,,$@))
endif

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
	($(ACTIVATE) && ninja -C builddir/libnodegl coverage-html)
coverage-xml:
	($(ACTIVATE) && ninja -C builddir/libnodegl coverage-xml)

.PHONY: all
.PHONY: ngl-tools-install
.PHONY: pynodegl-utils-install pynodegl-utils-deps-install
.PHONY: pynodegl-install pynodegl-deps-install
.PHONY: nodegl-install nodegl-setup
.PHONY: sxplayer-install
.PHONY: tests tests-setup
.PHONY: clean clean_py
.PHONY: coverage-html coverage-xml
.PHONY: external-download
