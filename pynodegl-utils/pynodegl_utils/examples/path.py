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
from pynodegl_utils.tests.debug import get_debug_points
from pynodegl_utils.toolbox.colors import COLORS


def _path_scene(cfg, path, points=None, controls=None, easing='linear'):
    cfg.aspect_ratio = (1, 1)

    anim_kf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1, easing),
    ]

    geom = ngl.Circle(radius=0.03, npoints=32)
    prog = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    shape = ngl.Render(geom, prog)
    shape.update_frag_resources(color=ngl.UniformVec4(value=COLORS.orange))

    moving_shape = ngl.Translate(shape, anim=ngl.AnimatedPath(anim_kf, path))

    objects = []

    if points:
        debug_points = {f'P{i}': p[:2] for i, p in enumerate(points)}
        objects.append(get_debug_points(cfg, debug_points))

    if controls:
        debug_controls = {f'C{i}': p[:2] for i, p in enumerate(controls)}
        objects.append(get_debug_points(cfg, debug_controls, color=COLORS.cyan))

    objects.append(moving_shape)

    return ngl.Group(children=objects)


@scene(easing=scene.List(choices=['linear', 'back_in_out']))
def simple_bezier(cfg, easing='linear'):
    cfg.duration = 2
    points = (
        (-0.7, 0.0, 0.0),
        ( 0.8, 0.1, 0.0),
    )
    controls = (
        (-0.2, -0.3, 0.0),
        ( 0.2,  0.8, 0.0),
    )

    keyframes = (
        ngl.PathKeyMove(to=points[0]),
        ngl.PathKeyBezier3(control1=controls[0], control2=controls[1], to=points[1]),
    )
    path = ngl.Path(keyframes);

    return _path_scene(cfg, path, points, controls, easing=easing)


@scene(tension=scene.Range(range=[0.01, 2], unit_base=100))
def catmull(cfg, tension=0.5):
    cfg.duration = 3
    points = (
        (-0.62, -0.30, 0.0),
        (-0.36,  0.40, 0.0),
        ( 0.04, -0.27, 0.0),
        ( 0.36,  0.28, 0.0),
        ( 0.65, -0.04, 0.0),
    )
    controls = (
        (-0.84, 0.07, 0.0),
        ( 0.84, 0.04, 0.0),
    )

    flat_points = (elt for iterable in points for elt in iterable)
    points_array = array.array('f', flat_points)

    path = ngl.SmoothPath(
        ngl.BufferVec3(data=points_array),
        control1=controls[0],
        control2=controls[1],
        tension=tension,
    )

    return _path_scene(cfg, path, points, controls, easing='exp_in_out')


@scene()
def complex_bezier(cfg):
    cfg.duration = 5
    points = (
        (-0.70, 0.08, 0.0),
        (-0.15, 0.06, 0.0),
        (-0.24, 0.52, 0.0),
        ( 0.23, 0.15, 0.0),
        ( 0.05,-0.25, 0.0),
    )

    controls = (
       ( 0.45,-0.59, 0.0),
       (-1.1, -0.47, 0.0),
       ( 0.25, 0.29, 0.0),
       (-0.19,-1.1,  0.0),
       (-0.25, 1.1,  0.0),
       ( 0.19,-0.75, 0.0),
       ( 0.0,  0.96, 0.0),
       ( 1.1, -0.86, 0.0),
    )

    keyframes = [
        ngl.PathKeyMove(to=points[0]),
        ngl.PathKeyBezier3(control1=controls[0], control2=controls[1], to=points[1]),
        ngl.PathKeyBezier3(control1=controls[2], control2=controls[3], to=points[2]),
        ngl.PathKeyBezier3(control1=controls[4], control2=controls[5], to=points[3]),
        ngl.PathKeyBezier3(control1=controls[6], control2=controls[7], to=points[4]),
    ]
    path = ngl.Path(keyframes)

    return _path_scene(cfg, path, points, controls)


@scene()
def composition(cfg):
    cfg.duration = 5
    points = (
        (-.6,  .2, 0),
        (-.2,  .7, 0),
        ( .5,  .3, 0),
        (-.3,  .1, 0),
        ( .1, -.2, 0),
        (-.2, -.4, 0),
        ( .3, -.6, 0),
        ( .7, -.2, 0),
        (-.4, -.1, 0),
        (-.8, -.1, 0),
        (-.6,  .2, 0),
    )

    controls = (
        (  .4,  .8, 0),
        (  .0,  .1, 0),
        ( -.1,  .2, 0),
        (  .6, -.6, 0),
        ( -.6, -.2, 0),
        ( -.8,  .4, 0),
        (-1.2,  .5, 0),
    )

    keyframes = [
        ngl.PathKeyMove(to=points[0]),
        ngl.PathKeyLine(to=points[1]),
        ngl.PathKeyBezier3(control1=controls[0], control2=controls[1], to=points[2]),
        ngl.PathKeyMove(to=points[3]),
        ngl.PathKeyBezier2(control=controls[2], to=points[4]),
        ngl.PathKeyLine(to=points[5]),
        ngl.PathKeyLine(to=points[6]),
        ngl.PathKeyBezier2(control=controls[3], to=points[7]),
        ngl.PathKeyMove(to=points[8]),
        ngl.PathKeyBezier3(control1=controls[4], control2=controls[5], to=points[9]),
        ngl.PathKeyBezier2(control=controls[6], to=points[10]),
    ]
    path = ngl.Path(keyframes)

    return _path_scene(cfg, path, points, controls)


@scene()
def lines(cfg):
    cfg.duration = 3

    points = (
        (-2/3,  1/6, 0),
        (-1/6,  2/3, 0),
        ( 0,  1/2, 0),
        ( 1/2, -1/3, 0),
        (-1/2, -2/3, 0),
    )

    keyframes = [
        ngl.PathKeyMove(to=points[0]),
        ngl.PathKeyLine(to=points[1]),
        ngl.PathKeyLine(to=points[2]),
        ngl.PathKeyMove(to=points[3]),
        ngl.PathKeyLine(to=points[4]),
    ]
    path = ngl.Path(keyframes)

    return _path_scene(cfg, path, points)


@scene()
def quadratic_arcs(cfg):
    cfg.duration = 3

    points = (
        (-2/3,  1/3, 0),
        ( 2/3,  1/3, 0),
        ( 2/3, -1/3, 0),
        (   0, -1/3, 0),
    )

    controls = (
        (  0,  5/6, 0),
        (1/3, -2/3, 0),
    )

    keyframes = [
        ngl.PathKeyMove(to=points[0]),
        ngl.PathKeyBezier2(control=controls[0], to=points[1]),
        ngl.PathKeyMove(to=points[2]),
        ngl.PathKeyBezier2(control=controls[1], to=points[3]),
    ]
    path = ngl.Path(keyframes, precision=64)

    return _path_scene(cfg, path, points, controls)


@scene()
def character_otf(cfg):
    """The 'g' glyph from Adobe Source Code Pro Black"""
    cfg.duration = 5
    keyframes = [
        ngl.PathKeyMove(to=(0.423713,0,0)),
        ngl.PathKeyBezier3(control1=(0.775735,0,0), control2=(1,0.103331,0), to=(1,0.253569,0)),
        ngl.PathKeyBezier3(control1=(1,0.381373,0), control2=(0.856618,0.434398,0), to=(0.608456,0.434398,0)),
        ngl.PathKeyLine(to=(0.448529,0.434398,0)),
        ngl.PathKeyBezier3(control1=(0.342831,0.434398,0), control2=(0.301471,0.445275,0), to=(0.301471,0.476547,0)),
        ngl.PathKeyBezier3(control1=(0.301471,0.495581,0), control2=(0.308824,0.503739,0), to=(0.331801,0.516655,0)),
        ngl.PathKeyBezier3(control1=(0.376838,0.509857,0), control2=(0.41636,0.507138,0), to=(0.450368,0.507138,0)),
        ngl.PathKeyBezier3(control1=(0.673713,0.507138,0), control2=(0.847426,0.573759,0), to=(0.847426,0.738273,0)),
        ngl.PathKeyBezier3(control1=(0.847426,0.766145,0), control2=(0.83364,0.793338,0), to=(0.820772,0.810333,0)),
        ngl.PathKeyLine(to=(0.98989,0.810333,0)),
        ngl.PathKeyLine(to=(0.98989,0.983005,0)),
        ngl.PathKeyLine(to=(0.610294,0.983005,0)),
        ngl.PathKeyBezier3(control1=(0.5625,0.993882,0), control2=(0.506434,1,0), to=(0.450368,1,0)),
        ngl.PathKeyBezier3(control1=(0.231618,1,0), control2=(0.0340074,0.919103,0), to=(0.0340074,0.746431,0)),
        ngl.PathKeyBezier3(control1=(0.0340074,0.665534,0), control2=(0.0900735,0.601632,0), to=(0.161765,0.568321,0)),
        ngl.PathKeyLine(to=(0.161765,0.562882,0)),
        ngl.PathKeyBezier3(control1=(0.0965074,0.528892,0), control2=(0.0487132,0.479266,0), to=(0.0487132,0.431679,0)),
        ngl.PathKeyBezier3(control1=(0.0487132,0.373215,0), control2=(0.0900735,0.337186,0), to=(0.143382,0.312033,0)),
        ngl.PathKeyLine(to=(0.143382,0.306594,0)),
        ngl.PathKeyBezier3(control1=(0.0487132,0.273283,0), control2=(0,0.229776,0), to=(0,0.172672,0)),
        ngl.PathKeyBezier3(control1=(0,0.0475867,0), control2=(0.190257,0,0), to=(0.423713,0,0)),
        ngl.PathKeyMove(to=(0.450368,0.648538,0)),
        ngl.PathKeyBezier3(control1=(0.387868,0.648538,0), control2=(0.337316,0.67981,0), to=(0.337316,0.746431,0)),
        ngl.PathKeyBezier3(control1=(0.337316,0.810333,0), control2=(0.387868,0.840925,0), to=(0.450368,0.840925,0)),
        ngl.PathKeyBezier3(control1=(0.511949,0.840925,0), control2=(0.565257,0.810333,0), to=(0.565257,0.746431,0)),
        ngl.PathKeyBezier3(control1=(0.565257,0.67981,0), control2=(0.511949,0.648538,0), to=(0.450368,0.648538,0)),
        ngl.PathKeyMove(to=(0.474265,0.147519,0)),
        ngl.PathKeyBezier3(control1=(0.348346,0.147519,0), control2=(0.261949,0.167233,0), to=(0.261949,0.211421,0)),
        ngl.PathKeyBezier3(control1=(0.261949,0.231135,0), control2=(0.276654,0.245411,0), to=(0.308824,0.261727,0)),
        ngl.PathKeyBezier3(control1=(0.340993,0.256288,0), control2=(0.378676,0.253569,0), to=(0.452206,0.253569,0)),
        ngl.PathKeyLine(to=(0.545956,0.253569,0)),
        ngl.PathKeyBezier3(control1=(0.639706,0.253569,0), control2=(0.693015,0.248131,0), to=(0.693015,0.211421,0)),
        ngl.PathKeyBezier3(control1=(0.693015,0.172672,0), control2=(0.600184,0.147519,0), to=(0.474265,0.147519,0)),
    ]

    path = ngl.Path(keyframes)
    return ngl.Scale(_path_scene(cfg, path), factors=(2,2,0), anchor=(1,1,0))

@scene()
def character_ttf(cfg):
    """The 'g' glyph from Noto Sans Black"""
    cfg.duration = 5
    keyframes = [
        ngl.PathKeyMove(to=(0.375912,1,0)),
        ngl.PathKeyBezier2(control=(0.483577,1,0), to=(0.550182,0.972036,0)),
        ngl.PathKeyBezier2(control=(0.616788,0.944073,0), to=(0.657847,0.902736,0)),
        ngl.PathKeyLine(to=(0.665146,0.902736,0)),
        ngl.PathKeyLine(to=(0.691606,0.987842,0)),
        ngl.PathKeyLine(to=(1,0.987842,0)),
        ngl.PathKeyLine(to=(1,0.297872,0)),
        ngl.PathKeyBezier2(control=(1,0.151976,0), to=(0.864051,0.0759878,0)),
        ngl.PathKeyBezier2(control=(0.729015,0,0), to=(0.448905,0,0)),
        ngl.PathKeyBezier2(control=(0.322993,0,0), to=(0.22719,0.00911854,0)),
        ngl.PathKeyBezier2(control=(0.132299,0.0176292,0), to=(0.044708,0.0401216,0)),
        ngl.PathKeyLine(to=(0.044708,0.238298,0)),
        ngl.PathKeyBezier2(control=(0.138686,0.212158,0), to=(0.222628,0.199392,0)),
        ngl.PathKeyBezier2(control=(0.306569,0.186018,0), to=(0.42792,0.186018,0)),
        ngl.PathKeyBezier2(control=(0.643248,0.186018,0), to=(0.643248,0.288146,0)),
        ngl.PathKeyLine(to=(0.643248,0.300304,0)),
        ngl.PathKeyBezier2(control=(0.643248,0.33617,0), to=(0.654197,0.386018,0)),
        ngl.PathKeyLine(to=(0.643248,0.386018,0)),
        ngl.PathKeyBezier2(control=(0.605839,0.345289,0), to=(0.541058,0.316109,0)),
        ngl.PathKeyBezier2(control=(0.476277,0.28693,0), to=(0.370438,0.28693,0)),
        ngl.PathKeyBezier2(control=(0.205292,0.28693,0), to=(0.10219,0.377508,0)),
        ngl.PathKeyBezier2(control=(0,0.468693,0), to=(0,0.642553,0)),
        ngl.PathKeyBezier2(control=(0,0.817021,0), to=(0.104015,0.908207,0)),
        ngl.PathKeyBezier2(control=(0.208942,1,0), to=(0.375912,1,0)),
        ngl.PathKeyMove(to=(0.510036,0.815805,0)),
        ngl.PathKeyBezier2(control=(0.362226,0.815805,0), to=(0.362226,0.638906,0)),
        ngl.PathKeyBezier2(control=(0.362226,0.549544,0), to=(0.399635,0.51003,0)),
        ngl.PathKeyBezier2(control=(0.437044,0.471125,0), to=(0.515511,0.471125,0)),
        ngl.PathKeyBezier2(control=(0.601277,0.471125,0), to=(0.635949,0.507599,0)),
        ngl.PathKeyBezier2(control=(0.67062,0.544073,0), to=(0.67062,0.617629,0)),
        ngl.PathKeyLine(to=(0.67062,0.646201,0)),
        ngl.PathKeyBezier2(control=(0.67062,0.72766,0), to=(0.637774,0.771429,0)),
        ngl.PathKeyBezier2(control=(0.605839,0.815805,0), to=(0.510036,0.815805,0)),
    ]

    path = ngl.Path(keyframes)
    return ngl.Scale(_path_scene(cfg, path), factors=(2,2,0), anchor=(1,1,0))
