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

export TARGET_OS ?= $(shell uname -s)

PREFIX          ?= venv
PREFIX_DONE     = .venv-done
ifeq ($(TARGET_OS),Windows)
PREFIX_FULLPATH = $(shell wslpath -wa .)\$(PREFIX)
else
PREFIX_FULLPATH = $(PWD)/$(PREFIX)
endif

DEBUG      ?= no
COVERAGE   ?= no
ifeq ($(TARGET_OS),Windows)
PYTHON     ?= python.exe
PIP = $(PREFIX)/Scripts/pip.exe
else
PYTHON     ?= python3
PIP = PKG_CONFIG_PATH=$(PREFIX_FULLPATH)/lib/pkgconfig LDFLAGS=$(RPATH_LDFLAGS) $(PREFIX)/bin/pip
endif

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
VCPKG_DIR ?= C:\\vcpkg
VCPKG_DIR_WSL = $(shell wslpath -a "$(VCPKG_DIR)")
PKG_CONF_DIR = external\\pkgconf\\build
endif

ifeq ($(TARGET_OS),Windows)
ACTIVATE = . $(PREFIX)/Scripts/activate
else
ACTIVATE = . $(PREFIX)/bin/activate
endif

RPATH_LDFLAGS ?= -Wl,-rpath,$(PREFIX_FULLPATH)/lib

ifeq ($(TARGET_OS),Windows)
MESON_BACKEND ?= vs
else ifeq ($(TARGET_OS),Darwin)
MESON_BACKEND ?= xcode
else
MESON_BACKEND ?= ninja
endif

BUILDDIR ?= builddir

ifeq ($(TARGET_OS),Windows)
MESON = meson.exe
MESON_SETUP_PARAMS  = \
    --prefix="$(PREFIX_FULLPATH)" --bindir="$(PREFIX_FULLPATH)\\Scripts" --includedir="$(PREFIX_FULLPATH)\\Include" \
    --libdir="$(PREFIX_FULLPATH)\\Lib" --pkg-config-path="$(VCPKG_DIR)\\installed\x64-windows\\lib\\pkgconfig;$(PREFIX_FULLPATH)\\Lib\\pkgconfig" -Drpath=true
MESON_SETUP         = $(MESON) setup $(MESON_SETUP_PARAMS)
else
MESON = meson
MESON_SETUP         = $(MESON) setup --prefix=$(PREFIX_FULLPATH) --pkg-config-path=$(PREFIX_FULLPATH)/lib/pkgconfig -Drpath=true
endif
MESON_SETUP += --backend $(MESON_BACKEND)

# MAKEFLAGS= is a workaround (not working on Windows due to incompatible Make
# syntax) for the issue described here:
# https://github.com/ninja-build/ninja/issues/1139#issuecomment-724061270
MESON_COMPILE = MAKEFLAGS= $(MESON) compile -j8

MESON_INSTALL = $(MESON) install

ifeq ($(COVERAGE),yes)
MESON_SETUP += -Db_coverage=true
DEBUG = yes
endif
ifeq ($(DEBUG),yes)
MESON_SETUP += --buildtype=debug
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
NODEGL_DEBUG_OPTS-$(DEBUG_VK)    += vk
NODEGL_DEBUG_OPTS-$(DEBUG_MEM)   += mem
NODEGL_DEBUG_OPTS-$(DEBUG_SCENE) += scene
NODEGL_DEBUG_OPTS-$(DEBUG_GPU_CAPTURE) += gpu_capture
ifneq ($(NODEGL_DEBUG_OPTS-yes),)
NODEGL_DEBUG_OPTS = -Ddebug_opts=$(shell echo $(NODEGL_DEBUG_OPTS-yes) | tr ' ' ',')
endif

ifeq ($(DEBUG_GPU_CAPTURE),yes)
ifeq ($(TARGET_OS),Windows)
RENDERDOC_DIR = $(shell wslpath -wa .)\external\renderdoc
NODEGL_DEBUG_OPTS += -Drenderdoc_dir="$(RENDERDOC_DIR)"
else ifeq ($(TARGET_OS),Linux)
RENDERDOC_DIR = $(PWD)/external/renderdoc
NODEGL_DEBUG_OPTS += -Drenderdoc_dir="$(RENDERDOC_DIR)"
endif
endif

NODEGL_MESON_SETUP_OPTIONS = $(NODEGL_DEBUG_OPTS) --default-library shared

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

ifeq ($(TARGET_OS),Windows)
CMAKE ?= cmake.exe
else
CMAKE ?= cmake
endif

ifeq ($(TARGET_OS),Windows)
CMAKE_GENERATOR ?= "Visual Studio 16 2019"
else ifeq ($(TARGET_OS),Linux)
CMAKE_GENERATOR ?= "CodeBlocks - Ninja"
else ifeq ($(TARGET_OS),Darwin)
CMAKE_GENERATOR ?= "Xcode"
endif

ifeq ($(DEBUG),yes)
CMAKE_BUILD_TYPE = Debug
else
CMAKE_BUILD_TYPE = Release
endif

CMAKE_SETUP_OPTIONS = -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -G$(CMAKE_GENERATOR) -DCMAKE_INSTALL_PREFIX=$(PREFIX)
ifeq ($(TARGET_OS),Windows)
# Python venv uses a non-conventional directory structure on Windows
CMAKE_SETUP_OPTIONS += -DCMAKE_INSTALL_INCLUDEDIR=Include -DCMAKE_INSTALL_LIBDIR=Lib -DCMAKE_INSTALL_BINDIR=Scripts
endif
ifeq ($(TARGET_OS),Windows)
# Always use MultiThreadedDLL (/MD), not MultiThreadedDebugDLL (/MDd)
# Some external libraries are only available in Release mode, not in Debug mode
# MSVC toolchain doesn't allow mixing libraries built in /MD mode with libraries built in /MDd mode
CMAKE_SETUP_OPTIONS += -DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreadedDLL
endif

ifeq ($(TARGET_OS),Windows)
# Set Windows SDK Version
CMAKE_SYSTEM_VERSION ?= 10.0.18362.0
CMAKE_SETUP_OPTIONS += -DCMAKE_SYSTEM_VERSION=$(CMAKE_SYSTEM_VERSION)
# Set VCPKG Directory
CMAKE_SETUP_OPTIONS += -DVCPKG_DIR='$(VCPKG_DIR)'
endif

# Set External Directory
ifeq ($(TARGET_OS),Windows)
EXTERNAL_DIR = $(shell wslpath -wa external)
else
EXTERNAL_DIR = $(PWD)/external
endif

CMAKE_SETUP_OPTIONS += -DEXTERNAL_DIR='$(EXTERNAL_DIR)'

CMAKE_COMPILE_OPTIONS = --config $(CMAKE_BUILD_TYPE) -j8
ifeq ($(V),1)
CMAKE_COMPILE_OPTIONS += -v
endif
CMAKE_INSTALL_OPTIONS = --config $(CMAKE_BUILD_TYPE)


all: ngl-tools-install pynodegl-utils-install
	@echo
	@echo "    Install completed."
	@echo
	@echo "    You can now enter the venv with:"
	@echo "        $(ACTIVATE)"
	@echo

ngl-tools-install: nodegl-install
	($(ACTIVATE) && $(MESON_SETUP) ngl-tools $(BUILDDIR)/ngl-tools && \
	$(MESON_COMPILE) -C $(BUILDDIR)/ngl-tools && $(MESON_INSTALL) -C $(BUILDDIR)/ngl-tools)

ngl-debug-tools-install:
	$(CMAKE) -S ngl-debug-tools -B $(BUILDDIR)/ngl-debug-tools $(CMAKE_SETUP_OPTIONS) && \
	$(CMAKE) --build $(BUILDDIR)/ngl-debug-tools $(CMAKE_COMPILE_OPTIONS) && \
	$(CMAKE) --install $(BUILDDIR)/ngl-debug-tools $(CMAKE_INSTALL_OPTIONS)

pynodegl-utils-install: pynodegl-utils-deps-install
	($(ACTIVATE) && $(PIP) -v install -e ./pynodegl-utils)

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
ifneq ($(TARGET_OS),MinGW-w64)
	($(ACTIVATE) && $(PIP) install -r ./pynodegl-utils/requirements.txt)
endif

pynodegl-install: pynodegl-deps-install
	($(ACTIVATE) && $(PIP) -v install -e ./pynodegl)
ifeq ($(TARGET_OS),Windows)
	# TODO: call add_dll_directory in pynodegl to add DLL search paths
	cp $(PREFIX)/Scripts/*.dll pynodegl/.
endif

pynodegl-deps-install: $(PREFIX_DONE) nodegl-install
	($(ACTIVATE) && $(PIP) install -r ./pynodegl/requirements.txt)

nodegl-install: nodegl-setup
	($(ACTIVATE) && $(MESON_COMPILE) -C $(BUILDDIR)/libnodegl && $(MESON_INSTALL) -C $(BUILDDIR)/libnodegl)

NODEGL_DEPS = sxplayer-install
ifeq ($(DEBUG_GPU_CAPTURE),yes)
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),MinGW-w64 Windows))
NODEGL_DEPS += renderdoc-install
endif
endif

nodegl-setup: $(NODEGL_DEPS)
	($(ACTIVATE) && $(MESON_SETUP) $(NODEGL_MESON_SETUP_OPTIONS) libnodegl $(BUILDDIR)/libnodegl)

pkg-config-install: external-download $(PREFIX_DONE)
ifeq ($(TARGET_OS),Windows)
	($(ACTIVATE) && $(MESON_SETUP) -Dtests=false external/pkgconf $(BUILDDIR)/pkgconf && $(MESON_COMPILE) -C $(BUILDDIR)/pkgconf && $(MESON_INSTALL) -C $(BUILDDIR)/pkgconf)
endif

sxplayer-install: external-download pkg-config-install $(PREFIX)
	($(ACTIVATE) && $(MESON_SETUP) external/sxplayer $(BUILDDIR)/sxplayer && $(MESON_COMPILE) -C $(BUILDDIR)/sxplayer && $(MESON_INSTALL) -C $(BUILDDIR)/sxplayer)

renderdoc-install: external-download pkg-config-install $(PREFIX)
ifeq ($(TARGET_OS),Windows)
	cp external/renderdoc/renderdoc.dll $(PREFIX)/Scripts/.
endif

external-download:
	$(MAKE) -C external
ifeq ($(TARGET_OS),Darwin)
	$(MAKE) -C external MoltenVK
endif
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),Darwin Windows))
	$(MAKE) -C external shaderc
endif

EXTERNAL_DEPS = sxplayer-install
ifeq ($(TARGET_OS),Darwin)
EXTERNAL_DEPS += MoltenVK-install
endif
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),Darwin Windows))
EXTERNAL_DEPS += shaderc-install
endif

external-install: $(EXTERNAL_DEPS)

shaderc-install: SHADERC_CMAKE_SETUP_OPTIONS = $(filter-out -G$(CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE),$(CMAKE_SETUP_OPTIONS))
shaderc-install: SHADERC_CMAKE_SETUP_OPTIONS +=  -GNinja -DCMAKE_BUILD_TYPE=Release
shaderc-install: SHADERC_CMAKE_COMPILE_OPTIONS = $(subst --config $(CMAKE_BUILD_TYPE),--config Release,$(CMAKE_COMPILE_OPTIONS))
shaderc-install: SHADERC_CMAKE_INSTALL_OPTIONS = $(subst --config $(CMAKE_BUILD_TYPE),--config Release,$(CMAKE_INSTALL_OPTIONS))
ifeq ($(TARGET_OS),Darwin)
shaderc-install: SHADERC_LIB_FILENAME = libshaderc_shared.1.dylib
endif
shaderc-install: external-download $(PREFIX)
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),Darwin Windows))
	cd external/shaderc && ./utils/git-sync-deps
	$(CMAKE) -S external/shaderc -B $(BUILDDIR)/shaderc $(SHADERC_CMAKE_SETUP_OPTIONS) -DSHADERC_SKIP_TESTS=ON && \
	$(CMAKE) --build $(BUILDDIR)/shaderc $(SHADERC_CMAKE_COMPILE_OPTIONS) && \
	$(CMAKE) --install $(BUILDDIR)/shaderc $(SHADERC_CMAKE_INSTALL_OPTIONS)
ifeq ($(TARGET_OS),Darwin)
	install_name_tool -id @rpath/$(SHADERC_LIB_FILENAME) $(PREFIX)/lib/$(SHADERC_LIB_FILENAME)
endif
endif

# Note: somehow xcodebuild sets name @rpath/libMoltenVK.dylib automatically
# (according to otool -l) so we don't have to do anything special
MoltenVK-install: external-download $(PREFIX)
	cd external/MoltenVK && ./fetchDependencies -v --macos
	$(MAKE) -C external/MoltenVK macos
	install -d $(PREFIX)/include
	install -d $(PREFIX)/lib
	cp -v external/MoltenVK/Package/Latest/MoltenVK/dylib/macOS/libMoltenVK.dylib $(PREFIX)/lib
	cp -vr external/MoltenVK/Package/Latest/MoltenVK/include $(PREFIX)

#
# We do not pull meson from pip on MinGW for the same reasons we don't pull
# Pillow and PySide2. We require the users to have it on their system.
#
$(PREFIX_DONE):
ifeq ($(TARGET_OS),Windows)
	$(PYTHON) -m venv $(PREFIX)
	# Convert windows to unix style line endings in the activate bash script
	# See https://bugs.python.org/issue43437
	sed -i 's/\r$///' $(PREFIX)/Scripts/activate
	cp $(VCPKG_DIR_WSL)/installed/x64-windows/bin/*.dll $(PREFIX)/Scripts/.
	($(ACTIVATE) && $(PIP) install meson ninja)
else ifeq ($(TARGET_OS),MinGW-w64)
	$(PYTHON) -m venv --system-site-packages $(PREFIX)
else
	$(PYTHON) -m venv $(PREFIX)
	($(ACTIVATE) && $(PIP) install meson ninja)
endif
	touch $(PREFIX_DONE)

$(PREFIX): $(PREFIX_DONE)

tests: nodegl-tests tests-setup
	($(ACTIVATE) && $(MESON) test $(MESON_TESTS_SUITE_OPTS) -C $(BUILDDIR)/tests)

tests-setup: TESTS_MESON_SETUP = $(subst --backend $(MESON_BACKEND),--backend ninja, $(MESON_SETUP))
tests-setup: ngl-tools-install pynodegl-utils-install
	($(ACTIVATE) && $(TESTS_MESON_SETUP) $(BUILDDIR)/tests tests)

nodegl-tests: nodegl-install
	($(ACTIVATE) && $(MESON) test -C $(BUILDDIR)/libnodegl)

nodegl-%: nodegl-setup
	($(ACTIVATE) && $(MESON_COMPILE) -C $(BUILDDIR)/libnodegl $(subst nodegl-,,$@))

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
	$(RM) -r $(BUILDDIR)/sxplayer
	$(RM) -r $(BUILDDIR)/libnodegl
	$(RM) -r $(BUILDDIR)/ngl-tools
	$(RM) -r $(BUILDDIR)/tests

# You need to build and run with COVERAGE set to generate data.
# For example: `make clean && make -j8 tests COVERAGE=yes`
# We don't use `meson coverage` here because of
# https://github.com/mesonbuild/meson/issues/7895
coverage-html:
	($(ACTIVATE) && ninja -C $(BUILDDIR)/libnodegl coverage-html)
coverage-xml:
	($(ACTIVATE) && ninja -C $(BUILDDIR)/libnodegl coverage-xml)

.PHONY: all
.PHONY: ngl-tools-install
.PHONY: ngl-debug-tools-install
.PHONY: pynodegl-utils-install pynodegl-utils-deps-install
.PHONY: pynodegl-install pynodegl-deps-install
.PHONY: nodegl-install nodegl-setup
.PHONY: sxplayer-install
.PHONY: shaderc-install
.PHONY: MoltenVK-install
.PHONY: tests tests-setup
.PHONY: clean clean_py
.PHONY: coverage-html coverage-xml
.PHONY: external-download external-install
