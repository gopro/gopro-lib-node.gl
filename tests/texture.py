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
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint


def _render_buffer(w, h):
    n = w * h
    data = array.array('B', [i * 255 // n for i in range(n)])
    buf = ngl.BufferUByte(data=data)
    texture = ngl.Texture2D(width=w, height=h, data_src=buf)
    render = ngl.Render(ngl.Quad())
    render.update_textures(tex0=texture)
    return render


@test_fingerprint()
@scene(w=scene.Range(range=[1, 128]),
       h=scene.Range(range=[1, 128]))
def texture_data(cfg, w=4, h=5):
    cfg.aspect_ratio = (1, 1)
    return _render_buffer(w, h)


@test_fingerprint()
@scene(dim=scene.Range(range=[1, 100]))
def texture_data_animated(cfg, dim=8):
    cfg.duration = 3.0
    random.seed(0)
    get_rand = lambda: array.array('f', [random.random() for i in range(dim ** 2 * 3)])
    nb_kf = int(cfg.duration)
    buffers = [get_rand() for i in range(nb_kf)]
    random_animkf = []
    time_scale = cfg.duration / float(nb_kf)
    for i, buf in enumerate(buffers + [buffers[0]]):
        random_animkf.append(ngl.AnimKeyFrameBuffer(i*time_scale, buf))
    random_buffer = ngl.AnimatedBufferVec3(keyframes=random_animkf)
    random_tex = ngl.Texture2D(data_src=random_buffer, width=dim, height=dim)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    prog = ngl.Program()
    render = ngl.Render(quad, prog)
    render.update_textures(tex0=random_tex)
    return render


@test_fingerprint()
@scene(h=scene.Range(range=[1, 32]))
def texture_data_unaligned_row(cfg, h=32):
    '''Tests upload of buffers with rows that are not 4-byte aligned'''
    cfg.aspect_ratio = (1, 1)
    return _render_buffer(1, h)
