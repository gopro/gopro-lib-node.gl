#!/usr/bin/env python
#
# Copyright 2019 GoPro Inc.
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

import os
import pynodegl as ngl
from pynodegl_utils.misc import get_backend
from pynodegl_utils.toolbox.grid import autogrid_simple


_backend_str = os.environ.get('BACKEND')
_backend = get_backend(_backend_str) if _backend_str else ngl.BACKEND_AUTO

_vert = 'void main() { ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position; }'
_frag = 'void main() { ngl_out_color = color; }'

def _get_scene(geometry=None):
    program = ngl.Program(vertex=_vert, fragment=_frag)
    if geometry is None:
        geometry = ngl.Quad()
    scene = ngl.Render(geometry, program)
    scene.update_frag_resources(color=ngl.UniformVec4(value=(1.0, 1.0, 1.0, 1.0)))
    return scene

def api_backend():
    viewer = ngl.Context()
    assert viewer.configure(backend=0x1234) < 0
    del viewer


def api_reconfigure():
    viewer = ngl.Context()
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    assert viewer.set_scene(scene) == 0
    assert viewer.draw(0) == 0
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    assert viewer.draw(1) == 0
    del viewer


def api_reconfigure_clearcolor(width=16, height=16):
    import zlib
    viewer = ngl.Context()
    capture_buffer = bytearray(width * height * 4)
    viewer = ngl.Context()
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0
    scene = _get_scene()
    assert viewer.set_scene(scene) == 0
    assert viewer.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0xb4bd32fa
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer,
                            clear_color=(0.4, 0.4, 0.4, 1.0)) == 0
    assert viewer.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0x05c44869
    del capture_buffer
    del viewer


def api_reconfigure_fail():
    viewer = ngl.Context()
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    assert viewer.set_scene(scene) == 0
    assert viewer.draw(0) == 0
    assert viewer.configure(offscreen=0, backend=_backend) != 0
    assert viewer.draw(1) != 0
    del viewer


def api_ctx_ownership():
    viewer = ngl.Context()
    viewer2 = ngl.Context()
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    assert viewer2.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    assert viewer.set_scene(scene) == 0
    assert viewer.draw(0) == 0
    assert viewer2.set_scene(scene) != 0
    assert viewer2.draw(0) == 0
    del viewer
    del viewer2


def api_ctx_ownership_subgraph():
    for shared in (True, False):
        viewer = ngl.Context()
        viewer2 = ngl.Context()
        assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
        assert viewer2.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
        quad = ngl.Quad()
        render1 = _get_scene(quad)
        if not shared:
            quad = ngl.Quad()
        render2 = _get_scene(quad)
        scene = ngl.Group([render1, render2])
        assert viewer.set_scene(render2) == 0
        assert viewer.draw(0) == 0
        assert viewer2.set_scene(scene) != 0
        assert viewer2.draw(0) == 0  # XXX: drawing with no scene is allowed?
        del viewer
        del viewer2


def api_capture_buffer_lifetime(width=1024, height=1024):
    capture_buffer = bytearray(width * height * 4)
    viewer = ngl.Context()
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0
    del capture_buffer
    scene = _get_scene()
    assert viewer.set_scene(scene) == 0
    assert viewer.draw(0) == 0
    del viewer


# Exercise the HUD rasterization. We can't really check the output, so this is
# just for blind coverage and similar code instrumentalization.
def api_hud(width=234, height=123):
    viewer = ngl.Context()
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend) == 0
    render = _get_scene()
    scene = ngl.HUD(render)
    scene = render
    assert viewer.set_scene(scene) == 0
    for i in range(60 * 3):
        assert viewer.draw(i / 60.) == 0
    del viewer


def api_text_live_change(width=320, height=240):
    import zlib
    viewer = ngl.Context()
    capture_buffer = bytearray(width * height * 4)
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0

    # An empty string forces the text node to deal with a pipeline with nul
    # attributes, this is what we exercise here, along with a varying up and
    # down number of characters
    text_strings = ["foo", "", "foobar", "world", "hello\nworld", "\n\n", "last"]

    # Exercise the diamond-form/prepare mechanism
    text_node = ngl.Text()
    viewer.set_scene(autogrid_simple([text_node] * 4))

    viewer.draw(0)
    last_crc = zlib.crc32(capture_buffer)
    for i, s in enumerate(text_strings, 1):
        text_node.set_text(s)
        viewer.draw(i)
        crc = zlib.crc32(capture_buffer)
        assert crc != last_crc
        last_crc = crc
