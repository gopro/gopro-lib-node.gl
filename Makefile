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

include common.mak

SXPLAYER_VERSION ?= 9.5.1

# Prevent headers from being rewritten, which would cause unecessary
# recompilations between `make` calls.
INSTALL = install -C

VIRTUALENV ?= virtualenv

ACTIVATE = $(PREFIX)/bin/activate

RPATH_LDFLAGS ?= -Wl,-rpath,$(PREFIX)/lib
ifeq ($(TARGET_OS),Darwin)
	LIBNODEGL_EXTRA_LDFLAGS   = -Wl,-install_name,@rpath/libnodegl.dylib
	LIBSXPLAYER_EXTRA_LDFLAGS = -Wl,-install_name,@rpath/libsxplayer.dylib
endif

all: ngl-tools-install pynodegl-utils-install
	@echo
	@echo "    Install completed."
	@echo
	@echo "    You can now enter the venv with:"
	@echo "        . $(ACTIVATE)"
	@echo

ngl-tools-install: nodegl-install
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig LDFLAGS=$(RPATH_LDFLAGS) $(MAKE) -C ngl-tools install PREFIX=$(PREFIX) DEBUG=$(DEBUG)

pynodegl-utils-install: pynodegl-utils-deps-install
	(. $(ACTIVATE) && pip -v install -e ./pynodegl-utils)

# pynodegl-install is in dependency to prevent from trying to install pynodegl
# from its requirements. Pulling pynodegl from requirements has two main issue:
# it tries to get it from PyPi (and we want to install the local pynodegl
# version), and it would fail anyway because pynodegl is currently not
# available on PyPi.
pynodegl-utils-deps-install: pynodegl-install
	(. $(ACTIVATE) && pip install -r ./pynodegl-utils/requirements.txt)

pynodegl-install: pynodegl-deps-install
	(. $(ACTIVATE) && PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig LDFLAGS=$(RPATH_LDFLAGS) pip -v install -e ./pynodegl)

pynodegl-deps-install: $(PREFIX) nodegl-install
	(. $(ACTIVATE) && pip install -r ./pynodegl/requirements.txt)

nodegl-install: sxplayer-install
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig LDFLAGS="$(RPATH_LDFLAGS) $(LIBNODEGL_EXTRA_LDFLAGS)" $(MAKE) -C libnodegl install PREFIX=$(PREFIX) DEBUG=$(DEBUG) SHARED=yes INSTALL="$(INSTALL)"

sxplayer-install: sxplayer $(PREFIX)
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig LDFLAGS="$(RPATH_LDFLAGS) $(LIBSXPLAYER_EXTRA_LDFLAGS)" $(MAKE) -C sxplayer install PREFIX=$(PREFIX) DEBUG=$(DEBUG) SHARED=yes INSTALL="$(INSTALL)"

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
	ln -snf $< $@

sxplayer-$(SXPLAYER_VERSION): sxplayer-$(SXPLAYER_VERSION).tar.gz
	$(TAR) xf $<

sxplayer-$(SXPLAYER_VERSION).tar.gz:
	$(CURL) -L https://github.com/Stupeflix/sxplayer/archive/v$(SXPLAYER_VERSION).tar.gz -o $@

$(PREFIX):
	$(VIRTUALENV) -p $(PYTHON) $(PREFIX)

tests: ngl-tools-install pynodegl-utils-install nodegl-tests
	(. $(ACTIVATE) && $(MAKE) -C tests)

nodegl-%: nodegl-install
	(. $(ACTIVATE) && PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig LDFLAGS=$(RPATH_LDFLAGS) $(MAKE) -C libnodegl $(subst nodegl-,,$@) DEBUG=$(DEBUG))

clean_py:
	$(RM) pynodegl/nodes_def.pyx
	$(RM) pynodegl/pynodegl.c
	$(RM) pynodegl/pynodegl.so
	$(RM) -r pynodegl/build
	$(RM) -r pynodegl/pynodegl.egg-info
	$(RM) -r pynodegl/.eggs
	$(RM) -r pynodegl-utils/pynodegl_utils.egg-info
	$(RM) -r pynodegl-utils/.eggs

clean: clean_py
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig $(MAKE) -C libnodegl clean
	PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig $(MAKE) -C ngl-tools clean

# You need to build and run with COVERAGE set to generate data.
# For example: `make clean && make -j8 tests COVERAGE=yes`
coverage:
	mkdir -p ngl-cov
	gcovr -r libnodegl --html-details -o ngl-cov/index.html

.PHONY: all
.PHONY: ngl-tools-install
.PHONY: pynodegl-utils-install pynodegl-utils-deps-install
.PHONY: pynodegl-install pynodegl-deps-install
.PHONY: nodegl-install
.PHONY: sxplayer-install
.PHONY: tests
.PHONY: clean clean_py
.PHONY: coverage
