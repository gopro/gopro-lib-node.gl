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

include ../common.mak

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
SDL_LDLIBS := $(shell $(PKG_CONFIG) --libs   sdl2)

SXPLAYER_CFLAGS := $(shell $(PKG_CONFIG) --cflags libsxplayer)
SXPLAYER_LDLIBS := $(shell $(PKG_CONFIG) --libs   libsxplayer)

TOOLS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libnodegl)
TOOLS_LDLIBS := $(shell $(PKG_CONFIG) --libs   libnodegl) -lm

ifeq ($(TARGET_OS),Darwin)
TOOLS_LDLIBS += -framework AppKit
endif

# Warning: while all 3 "python{,2,3}-config" should work as expected,
# "pkg-config --exists python" never will, so an explicit version is needed.
HAS_PYTHON := $(if $(shell pkg-config --exists python$(PYTHON_MAJOR) && echo 1),yes,no)

TOOLS = desktop ipc player render
ifeq ($(HAS_PYTHON),yes)
PYTHON_CFLAGS := $(shell python$(PYTHON_MAJOR)-config --cflags)
#
# After the 2-to-3 migration fiasco, the Python team did a lot of retrospective
# on why it failed. Lesson learned: now instead of bumping major, they do major
# incompatible breakages in minor revisions:
#
#   To support both 3.8 and older, try python3-config --libs --embed first and
#   fallback to python3-config --libs (without --embed) if the previous command
#   fails.
#
#   Source: https://docs.python.org/3/whatsnew/3.8.html
#
# Note that we need something more complex than `python3-config ... --embed ||
# python3-config ...` because obviously on failure the tool prints on stdout.
#
# And because all of this wasn't enough, we use --ldflags instead of
# --libs because it seems to be the real equivalent of pkg-config --libs.
# Indeed, at least on MacOS --libs doesn't raise the -L flag needed for the
# link to succeed. Note that since they decided that --ldflags was also
# including the libs (when it shouldn't), there is no need to have an
# additional redundant --libs.
#
PYTHON_LDLIBS_EMBED-no  = $(shell python$(PYTHON_MAJOR)-config --ldflags)
PYTHON_LDLIBS_EMBED-yes = $(shell python$(PYTHON_MAJOR)-config --ldflags --embed)
PYTHON_HAS_EMBED := $(shell python$(PYTHON_MAJOR)-config --embed >/dev/null && echo yes || echo no)
PYTHON_LDLIBS := $(PYTHON_LDLIBS_EMBED-$(PYTHON_HAS_EMBED))
TOOLS += python serialize
endif

TOOLS_BINS = $(addprefix ngl-, $(addsuffix $(EXESUF), $(TOOLS)))

all: $(TOOLS_BINS)

WSI_OBJS_Linux  = wsi_linux.o
WSI_OBJS_Darwin = wsi_cocoa.o
WSI_OBJS_MinGW-w64 = wsi_windows.o
WSI_OBJS = wsi.o $(WSI_OBJS_$(TARGET_OS))

ngl-desktop$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(SDL_CFLAGS) $(TOOLS_CFLAGS)
ngl-desktop$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(SDL_LDLIBS) $(TOOLS_LDLIBS) -lpthread
ngl-desktop$(EXESUF): ngl-desktop.o ipc.o player.o opts.o $(WSI_OBJS)

ngl-ipc$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS)
ngl-ipc$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS)
ngl-ipc$(EXESUF): ngl-ipc.o ipc.o opts.o

ngl-serialize$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(PYTHON_CFLAGS)
ngl-serialize$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(PYTHON_LDLIBS)
ngl-serialize$(EXESUF): ngl-serialize.o python_utils.o

ngl-player$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(SDL_CFLAGS) $(SXPLAYER_CFLAGS)
ngl-player$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(SDL_LDLIBS) $(SXPLAYER_LDLIBS)
ngl-player$(EXESUF): ngl-player.o player.o opts.o $(WSI_OBJS)

ngl-render$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(SDL_CFLAGS)
ngl-render$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(SDL_LDLIBS)
ngl-render$(EXESUF): ngl-render.o opts.o $(WSI_OBJS)

ngl-python$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(SDL_CFLAGS) $(PYTHON_CFLAGS)
ngl-python$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(SDL_LDLIBS) $(PYTHON_LDLIBS)
ngl-python$(EXESUF): ngl-python.o player.o python_utils.o opts.o $(WSI_OBJS)

TOOLS_OBJS = common.o

$(TOOLS_BINS): $(TOOLS_OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) $(TOOLS_BINS)
	$(RM) ngl-*.o common.o player.o python_utils.o wsi*.o

install: $(TOOLS_BINS)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 755 $(TOOLS_BINS) $(DESTDIR)$(PREFIX)/bin

uninstall:
	$(RM) $(addprefix $(DESTDIR)$(PREFIX)/bin/, $(TOOLS_BINS))

.PHONY: all clean install uninstall
