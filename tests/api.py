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
    ctx = ngl.Context()
    assert ctx.configure(backend=0x1234) < 0
    del ctx


def api_reconfigure():
    ctx = ngl.Context()
    assert ctx.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert ctx.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    assert ctx.draw(1) == 0
    del ctx


def api_reconfigure_clearcolor(width=16, height=16):
    import zlib
    ctx = ngl.Context()
    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    assert ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0xb4bd32fa
    assert ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer,
                         clear_color=(0.4, 0.4, 0.4, 1.0)) == 0
    assert ctx.draw(0) == 0
    assert zlib.crc32(capture_buffer) == 0x05c44869
    del capture_buffer
    del ctx


def api_reconfigure_fail():
    ctx = ngl.Context()
    assert ctx.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert ctx.configure(offscreen=0, backend=_backend) != 0
    assert ctx.draw(1) != 0
    del ctx


def api_ctx_ownership():
    ctx = ngl.Context()
    ctx2 = ngl.Context()
    assert ctx.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    assert ctx2.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    assert ctx2.set_scene(scene) != 0
    assert ctx2.draw(0) == 0
    del ctx
    del ctx2


def api_ctx_ownership_subgraph():
    for shared in (True, False):
        ctx = ngl.Context()
        ctx2 = ngl.Context()
        assert ctx.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
        assert ctx2.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
        quad = ngl.Quad()
        render1 = _get_scene(quad)
        if not shared:
            quad = ngl.Quad()
        render2 = _get_scene(quad)
        scene = ngl.Group([render1, render2])
        assert ctx.set_scene(render2) == 0
        assert ctx.draw(0) == 0
        assert ctx2.set_scene(scene) != 0
        assert ctx2.draw(0) == 0  # XXX: drawing with no scene is allowed?
        del ctx
        del ctx2


def api_capture_buffer_lifetime(width=1024, height=1024):
    capture_buffer = bytearray(width * height * 4)
    ctx = ngl.Context()
    assert ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0
    del capture_buffer
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    assert ctx.draw(0) == 0
    del ctx


# Exercise the HUD rasterization. We can't really check the output, so this is
# just for blind coverage and similar code instrumentalization.
def api_hud(width=234, height=123):
    ctx = ngl.Context()
    assert ctx.configure(offscreen=1, width=width, height=height, backend=_backend, hud=1) == 0
    scene = _get_scene()
    assert ctx.set_scene(scene) == 0
    for i in range(60 * 3):
        assert ctx.draw(i / 60.) == 0
    del ctx


def api_text_live_change(width=320, height=240):
    import zlib
    ctx = ngl.Context()
    capture_buffer = bytearray(width * height * 4)
    assert ctx.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0

    # An empty string forces the text node to deal with a pipeline with nul
    # attributes, this is what we exercise here, along with a varying up and
    # down number of characters
    text_strings = ["foo", "", "foobar", "world", "hello\nworld", "\n\n", "last"]

    # Exercise the diamond-form/prepare mechanism
    text_node = ngl.Text()
    ctx.set_scene(autogrid_simple([text_node] * 4))

    ctx.draw(0)
    last_crc = zlib.crc32(capture_buffer)
    for i, s in enumerate(text_strings, 1):
        text_node.set_text(s)
        ctx.draw(i)
        crc = zlib.crc32(capture_buffer)
        assert crc != last_crc
        last_crc = crc


def _ret_to_fourcc(ret):
    if ret >= 0:
        return None
    x = -ret
    return chr(x>>24) + chr(x>>16 & 0xff) + chr(x>>8 & 0xff) + chr(x&0xff)


def api_media_sharing_failure():
    import struct
    ctx = ngl.Context()
    assert ctx.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    m = ngl.Media('/dev/null')
    scene = ngl.Group(children=(m, m))
    assert _ret_to_fourcc(ctx.set_scene(scene)) == 'Eusg'  # Usage error
