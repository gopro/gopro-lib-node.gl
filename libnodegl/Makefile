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

include ../common.mak

DEBUG_GL ?= no
WGET     ?= wget

ifeq ($(DEBUG_GL),yes)
	PROJECT_CFLAGS += -DDEBUG_GL
endif

LD_SYM_FILE   = $(LIB_BASENAME).symexport
LD_SYM_OPTION = --version-script
LD_SYM_DATA   = "{\n\tglobal: ngl_*;\n\tlocal: *;\n};\n"
DYLIBSUFFIX = so
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),Darwin iPhone))
	DYLIBSUFFIX = dylib
	LD_SYM_OPTION = -exported_symbols_list
	LD_SYM_DATA   = "_ngl_*\n"
else
ifeq ($(TARGET_OS),MinGW-w64)
	DYLIBSUFFIX = dll
endif # MinGW
endif # Darwin/iPhone

ifeq ($(SHARED),yes)
	LIBSUFFIX = $(DYLIBSUFFIX)
else
	LIBSUFFIX = a
endif
LIB_BASENAME = libnodegl
LIB_NAME     = $(LIB_BASENAME).$(LIBSUFFIX)
LIB_PCNAME   = $(LIB_BASENAME).pc

LIB_OBJS = api.o                    \
           backend_gl.o             \
           bstr.o                   \
           deserialize.o            \
           dot.o                    \
           glcontext.o              \
           glstate.o                \
           hmap.o                   \
           hwupload.o               \
           log.o                    \
           math_utils.o             \
           node_animatedbuffer.o    \
           node_animation.o         \
           node_animkeyframe.o      \
           node_buffer.o            \
           node_camera.o            \
           node_circle.o            \
           node_compute.o           \
           node_computeprogram.o    \
           node_geometry.o          \
           node_graphicconfig.o     \
           node_group.o             \
           node_hud.o               \
           node_identity.o          \
           node_media.o             \
           node_program.o           \
           node_quad.o              \
           node_render.o            \
           node_rotate.o            \
           node_rtt.o               \
           node_scale.o             \
           node_texture.o           \
           node_timerangefilter.o   \
           node_timerangemodes.o    \
           node_transform.o         \
           node_translate.o         \
           node_triangle.o          \
           node_uniform.o           \
           nodes.o                  \
           params.o                 \
           serialize.o              \
           transforms.o             \
           utils.o                  \

LIB_OBJS_ARCH_aarch64 = asm_aarch64.o

LIB_OBJS += $(LIB_OBJS_ARCH_$(ARCH))

LIB_EXTRA_OBJS_Linux     = glcontext_x11.o glcontext_egl.o
LIB_EXTRA_OBJS_Darwin    = glcontext_nsgl.o
LIB_EXTRA_OBJS_Android   = glcontext_egl.o jni_utils.o android_utils.o android_looper.o android_surface.o android_handler.o android_handlerthread.o
LIB_EXTRA_OBJS_iPhone    = glcontext_eagl.o
LIB_EXTRA_OBJS_MinGW-w64 = glcontext_wgl.o

LIB_CFLAGS                 = -fPIC
LIB_EXTRA_CFLAGS_Linux     = -DHAVE_PLATFORM_GLX -DHAVE_PLATFORM_EGL
LIB_EXTRA_CFLAGS_Darwin    = -DHAVE_PLATFORM_NSGL
LIB_EXTRA_CFLAGS_Android   = -DHAVE_PLATFORM_EGL
LIB_EXTRA_CFLAGS_iPhone    = -DHAVE_PLATFORM_EAGL
LIB_EXTRA_CFLAGS_MinGW-w64 = -DHAVE_PLATFORM_WGL

LIB_LDLIBS                 = -lm -lpthread
LIB_EXTRA_LDLIBS_Linux     =
LIB_EXTRA_LDLIBS_Darwin    = -framework OpenGL -framework CoreVideo -framework CoreFoundation -framework AppKit
LIB_EXTRA_LDLIBS_Android   = -legl
LIB_EXTRA_LDLIBS_iPhone    = -framework CoreMedia
LIB_EXTRA_LDLIBS_MinGW-w64 = -lopengl32

LIB_PKG_CONFIG_LIBS               = "libsxplayer >= 9.2.0"
LIB_EXTRA_PKG_CONFIG_LIBS_Linux   = x11 gl egl
LIB_EXTRA_PKG_CONFIG_LIBS_Darwin  =
LIB_EXTRA_PKG_CONFIG_LIBS_Android = libavcodec
LIB_EXTRA_PKG_CONFIG_LIBS_iPhone  =

LIB_OBJS   += $(LIB_EXTRA_OBJS_$(TARGET_OS))
LIB_CFLAGS += $(LIB_EXTRA_CFLAGS_$(TARGET_OS))
LIB_LDLIBS += $(LIB_EXTRA_LDLIBS_$(TARGET_OS))
LIB_CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIB_PKG_CONFIG_LIBS) $(LIB_EXTRA_PKG_CONFIG_LIBS_$(TARGET_OS)))
LIB_LDLIBS += $(shell $(PKG_CONFIG) --libs   $(LIB_PKG_CONFIG_LIBS) $(LIB_EXTRA_PKG_CONFIG_LIBS_$(TARGET_OS)))

LIB_DEPS = $(LIB_OBJS:.o=.d)


all: $(LIB_PCNAME) $(LIB_NAME)


#
# Library
#
$(LIB_NAME): CFLAGS  = $(PROJECT_CFLAGS) $(LIB_CFLAGS)
$(LIB_NAME): LDLIBS  = $(PROJECT_LDLIBS) $(LIB_LDLIBS)
$(LIB_NAME): LDFLAGS = -Wl,$(LD_SYM_OPTION),$(LD_SYM_FILE)
$(LIB_NAME): CPPFLAGS += -MMD -MP
$(LIB_NAME): $(LD_SYM_FILE) $(LIB_OBJS)
ifeq ($(SHARED),yes)
	$(CC) $(LDFLAGS) $(LIB_OBJS) -shared -o $@ $(LDLIBS)
else
	$(AR) rcs $@ $(LIB_OBJS)
endif


#
# Symbols
#
$(LD_SYM_FILE):
	$(shell printf $(LD_SYM_DATA) > $(LD_SYM_FILE))


#
# pkg-config
#
define headver
$(shell sed -nE 's/^\#define NODEGL_VERSION_$(1)[^0-9]*([0-9]*)/\1/p' nodegl.h)
endef

$(LIB_PCNAME): VERSION = $(call headver,MAJOR).$(call headver,MINOR).$(call headver,MICRO)
$(LIB_PCNAME): LDLIBS = $(PROJECT_LDLIBS) $(LIB_LDLIBS)
ifeq ($(SHARED),yes)
$(LIB_PCNAME): DEP_PRIVATE_LIBS = $(LDLIBS)
else
$(LIB_PCNAME): DEP_LIBS = $(LDLIBS)
endif
$(LIB_PCNAME): $(LIB_PCNAME).tpl
	sed -e "s#PREFIX#$(PREFIX)#" \
	    -e "s#DEP_LIBS#$(DEP_LIBS)#" \
	    -e "s#DEP_PRIVATE_LIBS#$(DEP_PRIVATE_LIBS)#" \
	    -e "s#VERSION#$(VERSION)#" \
	    $^ > $@


#
# Specifications
#
SPECS_FILE = nodes.specs
gen_specs$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(LIB_CFLAGS)
gen_specs$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(LIB_LDLIBS)
gen_specs$(EXESUF): gen_specs.o $(LIB_OBJS)
gen_specs$(EXESUF):
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

updatespecs: gen_specs$(EXESUF)
	./gen_specs$(EXESUF) > $(SPECS_FILE)


#
# Doc generation
#
gen_doc$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(LIB_CFLAGS)
gen_doc$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(LIB_LDLIBS)
gen_doc$(EXESUF): gen_doc.o $(LIB_OBJS)
gen_doc$(EXESUF):
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

updatedoc: gen_doc$(EXESUF)
	./gen_doc$(EXESUF) > doc/libnodegl.md


#
# OpenGL function wrappers
#
gen-gl-wrappers: gl.xml
	$(PYTHON) gen-gl-wrappers.py $^
gl.xml:
	$(WGET) https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/master/xml/gl.xml


#
# Tests
#
TESTS = asm             \
        hmap            \
        utils           \

TESTPROGS = $(addprefix test_,$(TESTS))
$(TESTPROGS): CFLAGS = $(PROJECT_CFLAGS) $(LIB_CFLAGS)
$(TESTPROGS): LDLIBS = $(PROJECT_LDLIBS) $(LIB_LDLIBS)

testprogs: $(TESTPROGS)

test_asm: LDLIBS = $(PROJECT_LDLIBS) -lm
test_asm: test_asm.o math_utils.o $(LIB_OBJS_ARCH_$(ARCH))
test_hmap: test_hmap.o utils.o
test_utils: test_utils.o utils.o


#
# Misc/general
#
clean:
	$(RM) $(LIB_BASENAME).so $(LIB_BASENAME).dylib $(LIB_BASENAME).a $(LIB_BASENAME).dll
	$(RM) $(LIB_OBJS) $(LIB_DEPS)
	$(RM) gen_specs.o gen_specs$(EXESUF)
	$(RM) gen_doc.o gen_doc$(EXESUF)
	$(RM) $(LIB_PCNAME)
	$(RM) $(LD_SYM_FILE)
	$(RM) $(TESTPROGS)
	$(RM) $(addsuffix .o,$(TESTPROGS))

install: $(LIB_NAME) $(LIB_PCNAME)
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -d $(DESTDIR)$(PREFIX)/include
	install -d $(DESTDIR)$(PREFIX)/share
	install -d $(DESTDIR)$(PREFIX)/share/nodegl
	install -d $(DESTDIR)$(PREFIX)/share/nodegl/java
	install -d $(DESTDIR)$(PREFIX)/share/nodegl/java/org
	install -d $(DESTDIR)$(PREFIX)/share/nodegl/java/org/nodegl
ifeq ($(TARGET_OS),MinGW-w64)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 644 $(LIB_NAME) $(DESTDIR)$(PREFIX)/bin
endif
	install -m 644 $(LIB_NAME) $(DESTDIR)$(PREFIX)/lib
	install -m 644 $(LIB_PCNAME) $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 nodegl.h $(DESTDIR)$(PREFIX)/include/nodegl.h
	install -m 644 $(SPECS_FILE) $(DESTDIR)$(PREFIX)/share/nodegl
	install -m 644 android/java/OnFrameAvailableListener.java $(DESTDIR)$(PREFIX)/share/nodegl/java/org/nodegl

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME)
	$(RM) $(DESTDIR)$(PREFIX)/include/nodegl.h
	$(RM) -r $(DESTDIR)$(PREFIX)/share/nodegl

.PHONY: all updatespecs clean install uninstall gen-gl-wrappers

-include $(LIB_DEPS)
