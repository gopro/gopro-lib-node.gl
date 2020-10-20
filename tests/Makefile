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

ifeq ($(V),)
Q   := @
	ifneq ($(TARGET_OS),Windows)
		OUT := > /dev/null
	else
		OUT :=
	endif
else
Q   :=
OUT :=
endif

all: tests

# 1:category, 2:name
define DECLARE_TEST
test-$(1)-$(2): FUNC_NAME = $(1)_$(2)
TESTS_$(1) += test-$(1)-$(2)
endef

# 1:destvar 2:category, 3:namelist
define DECLARE_TESTS
$(foreach X,$(3),$(eval $(call DECLARE_TEST,$(2),$(X))))
test-$(2): $$(TESTS_$(2))
.PHONY: test-$(2)
$(1) += $$(TESTS_$(2))
$$(TESTS_$(2)): MODULE = $(2)
endef

# 1:category, 2:namelist
define DECLARE_SIMPLE_TESTS
$(call DECLARE_TESTS,SIMPLE_TESTS,$(1),$(2))
endef

# 1:category, 2:namelist
define DECLARE_REF_TESTS
$(call DECLARE_TESTS,REF_TESTS,$(1),$(2))
endef

ifeq ($(TARGET_OS),Darwin)
DISABLE_TESTS_STD430 = yes
endif

include anim.mak
include api.mak
include blending.mak
include compute.mak
include data.mak
include live.mak
include media.mak
include rtt.mak
include shape.mak
include text.mak
include texture.mak
include transform.mak

$(REF_TESTS):
	@echo $@
ifneq ($(TARGET_OS),Windows)
	$(Q)ngl-test $(MODULE).py $(FUNC_NAME) refs/$(FUNC_NAME).ref $(OUT)
else
	../cmake-build-debug/pyshell/RelWithDebInfo/pyshell.exe ../nodegl-env/Scripts/ngl-test-script.py $(MODULE).py $(FUNC_NAME) refs/$(FUNC_NAME).ref $(OUT)
endif

$(SIMPLE_TESTS):
	@echo $@
	$(Q)$(PYTHON) -c 'from $(MODULE) import $(FUNC_NAME); $(FUNC_NAME)()' $(OUT)

TESTS = $(SIMPLE_TESTS) $(REF_TESTS)

tests: $(TESTS)

tests-list:
	@for test in $(TESTS); do echo $$test; done

.PHONY: all tests tests-list $(TESTS)
