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
    viewer = ngl.Viewer()
    assert viewer.configure(backend=0x1234) < 0
    del viewer


def api_reconfigure():
    viewer = ngl.Viewer()
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    viewer.set_scene(scene)
    viewer.draw(0)
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    # FIXME: errors should be raised by the draw call so we can assert here
    viewer.draw(1)
    del viewer


def api_reconfigure_fail():
    viewer = ngl.Viewer()
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    viewer.set_scene(scene)
    viewer.draw(0)
    assert viewer.configure(offscreen=0, backend=_backend) != 0
    viewer.draw(1)
    del viewer


def api_ctx_ownership():
    viewer = ngl.Viewer()
    viewer2 = ngl.Viewer()
    assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    assert viewer2.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
    scene = _get_scene()
    viewer.set_scene(scene)
    viewer.draw(0)
    assert viewer2.set_scene(scene) != 0
    viewer2.draw(0)
    del viewer
    del viewer2


def api_ctx_ownership_subgraph():
    for shared in (True, False):
        viewer = ngl.Viewer()
        viewer2 = ngl.Viewer()
        assert viewer.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
        assert viewer2.configure(offscreen=1, width=16, height=16, backend=_backend) == 0
        quad = ngl.Quad()
        render1 = _get_scene(quad)
        if not shared:
            quad = ngl.Quad()
        render2 = _get_scene(quad)
        scene = ngl.Group([render1, render2])
        viewer.set_scene(render2)
        viewer.draw(0)
        assert viewer2.set_scene(scene) != 0
        viewer2.draw(0)
        del viewer
        del viewer2


def api_capture_buffer_lifetime(width=1024, height=1024):
    capture_buffer = bytearray(width * height * 4)
    viewer = ngl.Viewer()
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend, capture_buffer=capture_buffer) == 0
    del capture_buffer
    scene = _get_scene()
    viewer.set_scene(scene)
    viewer.draw(0)
    del viewer


# Exercise the HUD rasterization. We can't really check the output, so this is
# just for blind coverage and similar code instrumentalization.
def api_hud(width=234, height=123):
    viewer = ngl.Viewer()
    assert viewer.configure(offscreen=1, width=width, height=height, backend=_backend) == 0
    render = _get_scene()
    scene = ngl.HUD(render)
    viewer.set_scene(scene)
    for i in range(60 * 3):
        viewer.draw(i / 60.)
    del viewer
