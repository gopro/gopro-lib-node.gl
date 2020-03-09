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

TOOLS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libnodegl libsxplayer glfw3)
TOOLS_LDLIBS := $(shell $(PKG_CONFIG) --libs   libnodegl libsxplayer glfw3)

# OpenGL
ifeq ($(TARGET_OS),MinGW-w64)
TOOLS_LDLIBS += -lopengl32
else
ifeq ($(TARGET_OS),Darwin)
TOOLS_LDLIBS += -framework OpenGL -framework AppKit
else
TOOLS_CFLAGS += $(shell $(PKG_CONFIG) --cflags gl)
TOOLS_LDLIBS += $(shell $(PKG_CONFIG) --libs   gl)
endif
endif

# Warning: while all 3 "python{,2,3}-config" should work as expected,
# "pkg-config --exists python" never will, so an explicit version is needed.
HAS_PYTHON := $(if $(shell pkg-config --exists python$(PYTHON_MAJOR) && echo 1),yes,no)

TOOLS = player render serialize
ifeq ($(HAS_PYTHON),yes)
PYTHON_CFLAGS := $(shell python$(PYTHON_MAJOR)-config --cflags)
PYTHON_LDLIBS := $(shell python$(PYTHON_MAJOR)-config --libs)
TOOLS += python
endif

TOOLS_BINS = $(addprefix ngl-, $(addsuffix $(EXESUF), $(TOOLS)))

all: $(TOOLS_BINS)

ngl-serialize$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(PYTHON_CFLAGS)
ngl-serialize$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(PYTHON_LDLIBS)
ngl-serialize$(EXESUF): ngl-serialize.o python_utils.o

ngl-player$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS)
ngl-player$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS)
ngl-player$(EXESUF): ngl-player.o player.o

ngl-render$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS)
ngl-render$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS)
ngl-render$(EXESUF): ngl-render.o

ngl-python$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(PYTHON_CFLAGS)
ngl-python$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(PYTHON_LDLIBS)
ngl-python$(EXESUF): ngl-python.o player.o python_utils.o

TOOLS_OBJS_Linux  = wsi_x11.o
TOOLS_OBJS_Darwin = wsi_cocoa.o
TOOLS_OBJS_MinGW-w64 = wsi_windows.o
TOOLS_OBJS = common.o $(TOOLS_OBJS_$(TARGET_OS))

$(TOOLS_BINS): $(TOOLS_OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) $(TOOLS_BINS)
	$(RM) ngl-*.o common.o player.o python_utils.o wsi_*.o

install: $(TOOLS_BINS)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 755 $(TOOLS_BINS) $(DESTDIR)$(PREFIX)/bin

uninstall:
	$(RM) $(addprefix $(DESTDIR)$(PREFIX)/bin/, $(TOOLS_BINS))

.PHONY: all clean install uninstall
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
