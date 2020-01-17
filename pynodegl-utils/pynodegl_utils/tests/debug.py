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

import pynodegl as ngl
from pynodegl_utils.toolbox.colors import COLORS


def get_debug_points(cfg, points, radius=0.025, color=COLORS['green'], text_size=(0.1, 0.1)):
    prog = ngl.Program(fragment=cfg.get_frag('color'))
    g = ngl.Group()
    circle = ngl.Circle(radius=radius)
    circle_render = ngl.Render(circle, prog)
    circle_render.update_uniforms(color=ngl.UniformVec4(value=color))
    box_w = (text_size[0], 0, 0)
    box_h = (0, text_size[1], 0)
    for pos_name, position in points.items():
        text = ngl.Text(pos_name, box_width=box_w, box_height=box_h, bg_color=(0, 0, 0, 0), valign='top')
        text = ngl.Translate(text, (1 + radius, 1 - radius - text_size[1], 0))
        point = ngl.Group(children=(circle_render, text))
        point = ngl.Translate(point, list(position) + [0])
        g.add_children(point)
    return ngl.GraphicConfig(g, blend=True,
                             blend_src_factor='src_alpha',
                             blend_dst_factor='one_minus_src_alpha',
                             blend_src_factor_a='zero',
                             blend_dst_factor_a='one',
                             label='Debug circles')
