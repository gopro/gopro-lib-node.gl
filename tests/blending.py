#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

import sys
import os
import os.path as op
import array
import math
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene

from pynodegl_utils.tests.debug import get_debug_points
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.toolbox.grid import AutoGrid, autogrid_simple, autogrid_queue


_CIRCLE_RADIUS = 0.5


def _equilateral_triangle_coords(sz=0.5, balanced=True):
    b = sz * math.sqrt(3) / 2.
    c = sz / 2.
    yoff = (c - sz) / 2. if balanced else 0.
    return (-b, yoff - c, 0.), (b, yoff - c, 0.), (0., yoff + sz, 0.)


def _mid_pos(*points):
    return [sum(x) / len(x) for x in zip(*points)]


def _color_positions(sz=0.5, balanced=True):
    pA, pB, pC = [(x, y) for (x, y, _) in _equilateral_triangle_coords(sz, balanced)]
    pD, pE, pF = [_mid_pos(p0, p1) for (p0, p1) in ((pA, pC), (pA, pB), (pB, pC))]
    pO = _mid_pos(pA, pB, pC)
    yoff = -sz / 4. if balanced else 0.
    return dict(A=pA, B=pB, C=pC, D=pD, E=pE, F=pF, O=pO, Z=(-1., yoff + 1.))


def _get_dbg_positions(n=1):
    if n == 1:
        return _color_positions(_CIRCLE_RADIUS)
    ret = {}
    ag = AutoGrid(range(n))
    for _, i, col, row in ag:
        dbg_positions = _color_positions(_CIRCLE_RADIUS)
        dbg_positions = dict(('%s%d' % (name, i), ag.transform_coords(p, (col, row))) for name, p in dbg_positions.items())
        ret.update(dbg_positions)
    return ret


_CIRCLES_COLORS = [
    (186/255., 159/255.,   8/255., 1.0),
    (165/255.,   3/255.,  81/255., 1.0),
    (  6/255., 163/255., 194/255., 1.0),
]


def _get_blending_base_objects(cfg):
    circle = ngl.Circle(radius=_CIRCLE_RADIUS, npoints=100)
    prog = ngl.Program(fragment=cfg.get_frag('color'))
    positions = _equilateral_triangle_coords(_CIRCLE_RADIUS * 2.0 / 3.0)
    colored_circles = ngl.Group(label='colored circles')
    for position, color in zip(positions, _CIRCLES_COLORS):
        render = ngl.Render(circle, prog)
        render.update_uniforms(color=ngl.UniformVec4(value=color))
        render = ngl.Translate(render, position)
        colored_circles.add_children(render)
    return colored_circles, circle, prog, positions


def _get_background_circles(circle, prog, positions, bcolor):
    blend_bg = ngl.Group()
    render = ngl.Render(circle, prog)
    render.update_uniforms(color=ngl.UniformVec4(value=bcolor))
    for position in positions:
        trender = ngl.Translate(render, position)
        blend_bg.add_children(trender)
    return blend_bg


def _get_blending_scene_with_args(cfg, colored_circles, circle, prog, positions, bname, bcolor, **bparams):
    g = ngl.Group(label=bname)
    if bcolor is not None:
        blend_bg = _get_background_circles(circle, prog, positions, bcolor)
        blend_bg.set_label('background for {}'.format(bname))
        g.add_children(blend_bg)
    blended_circles = ngl.GraphicConfig(colored_circles, blend=True, **bparams)
    g.add_children(blended_circles)
    return g


def _get_blending_scene(cfg, bname, bcolor, **bparams):
    cfg.aspect_ratio = (1, 1)
    colored_circles, circle, prog, positions = _get_blending_base_objects(cfg)
    return _get_blending_scene_with_args(cfg, colored_circles, circle, prog, positions, bname, bcolor, **bparams)


_BLENDING_CFGS = (
    ("none", None, dict()),
    ("multiply", COLORS["white"], dict(
        blend_src_factor='dst_color',
        blend_dst_factor='zero',
    )),
    ("screen", COLORS["black"], dict(
        blend_src_factor="one",
        blend_dst_factor="one_minus_src_color",
    )),
    ("darken", COLORS["white"], dict(
        blend_op='min',
    )),
    ("lighten", COLORS["black"], dict(
        blend_op='max',
    )),
)
_BLENDINGS = list(b[0] for b in _BLENDING_CFGS)
_NB_BLENDINGS = len(_BLENDINGS)


def _get_blending_scenes(cfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = len(_BLENDING_CFGS)

    # WARNING: it is important to keep the creation of these base objects
    # outside of the loop below to test the diamond tree infrastructure
    # (multiple pipelines for one render).
    colored_circles, circle, prog, positions = _get_blending_base_objects(cfg)

    scenes = []
    for bname, bcolor, bparams in _BLENDING_CFGS:
        scenes.append(_get_blending_scene_with_args(cfg, colored_circles, circle, prog, positions, bname, bcolor, **bparams))
    return scenes


def _debug_overlay(cfg, scene, grid_names, show_dbg_points=False, show_labels=False):
    if not show_dbg_points and not show_labels:
        return scene

    assert grid_names is not None

    text_height = 0.25

    overlay = ngl.Group()

    if show_labels:
        text_group = ngl.Group()
        ag = AutoGrid(grid_names)
        for grid_name, i, col, row in ag:
            text = ngl.Text(grid_name,
                            fg_color=COLORS["white"], bg_color=COLORS["black"],
                            valign='top',
                            box_width=(2.0, 0, 0),
                            box_height=(0, text_height, 0),
                            box_corner=(-1, 1.0 - text_height, 0))
            text = ag.place_node(text, (col, row))
            text_group.add_children(text)

        scene = ngl.Translate(scene, (0, -text_height / 2.0 * ag.scale, 0), label='text offsetting')
        overlay.add_children(scene, text_group)
    else:
        overlay.add_children(scene)

    if show_dbg_points:
        nb = len(grid_names)
        dbg_positions = _get_dbg_positions(nb)
        if show_labels:
            dbg_positions = dict((name, (p[0], p[1] - text_height / 2.0 * ag.scale)) for name, p in dbg_positions.items())
        dbg_points = get_debug_points(cfg, dbg_positions, radius=0.01, text_size=(0.08, 0.08))

        overlay.add_children(dbg_points)

    return overlay


_TEST_SETTINGS = dict(show_dbg_points=False, show_labels=False, clear_color=COLORS['azure'])


@test_cuepoints(points=_get_dbg_positions(_NB_BLENDINGS), **_TEST_SETTINGS)
@scene(show_dbg_points=scene.Bool(), show_labels=scene.Bool())
def blending_all_diamond(cfg, show_dbg_points=True, show_labels=True):
    scenes = _get_blending_scenes(cfg)
    scene = autogrid_simple(scenes)
    return _debug_overlay(cfg, scene, _BLENDINGS, show_dbg_points, show_labels)


@test_cuepoints(points=_get_dbg_positions(_NB_BLENDINGS), nb_keyframes=_NB_BLENDINGS + 1, **_TEST_SETTINGS)
@scene(show_dbg_points=scene.Bool(), show_labels=scene.Bool())
def blending_all_timed_diamond(cfg, show_dbg_points=True, show_labels=True):
    scenes = _get_blending_scenes(cfg)
    scene = autogrid_queue(scenes, duration=cfg.duration, overlap_time=1.5)
    return _debug_overlay(cfg, scene, _BLENDINGS, show_dbg_points, show_labels)


def _get_blending_function(bname, bcolor, **bparams):
    @test_cuepoints(points=_get_dbg_positions(), **_TEST_SETTINGS)
    @scene(show_dbg_points=scene.Bool(), show_labels=scene.Bool())
    def scene_func(cfg, show_dbg_points=True, show_labels=True):
        scene = _get_blending_scene(cfg, bname, bcolor, **bparams)
        return _debug_overlay(cfg, scene, [bname], show_dbg_points, show_labels)
    return scene_func


for bname, bcolor, bparams in _BLENDING_CFGS:
    globals()["blending_" + bname] = _get_blending_function(bname, bcolor, **bparams)
