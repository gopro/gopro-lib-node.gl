#!/usr/bin/env python
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

import array
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint


@test_fingerprint(tolerance=1)
@scene()
def text_0_to_127(cfg):
    s = ''
    for y in range(8):
        for x in range(16):
            c = y << 4 | x
            s += chr(c) if c else ' '
        s += '\n'
    return ngl.Text(s)


@test_fingerprint(tolerance=1)
@scene()
def text_128_to_255(cfg):
    '''Note: this is currently incorrectly displaying characters it shouldn't
    display but this test is mainly meant to check for crashes'''
    s = ''
    for y in range(8):
        for x in range(16):
            c = 1<<7 | y << 4 | x
            s += chr(c) if c else ' '
        s += '\n'
    return ngl.Text(s)


def _text(**params):
    return ngl.Text('This\nis\nnode.gl', font_scale=0.7, padding=8, **params)


@test_fingerprint(tolerance=1)
@scene()
def text_colors(cfg):
    return _text(fg_color=COLORS['rose'], bg_color=COLORS['cgreen'])


@test_fingerprint(tolerance=1)
@scene()
def text_align_cc(cfg):
    return _text(valign="center", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_cr(cfg):
    return _text(valign="center", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_cl(cfg):
    return _text(valign="center", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_align_bc(cfg):
    return _text(valign="bottom", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_br(cfg):
    return _text(valign="bottom", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_bl(cfg):
    return _text(valign="bottom", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tc(cfg):
    return _text(valign="top", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tr(cfg):
    return _text(valign="top", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tl(cfg):
    return _text(valign="top", halign="left")
