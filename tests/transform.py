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
import math
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.toolbox.shapes import equilateral_triangle_coords


def _transform_shape(cfg):
    w, h = 0.75, 0.45
    geometry = ngl.Quad(corner=(-w/2., -h/2., 0), width=(w, 0, 0), height=(0, h, 0))
    prog = ngl.Program(fragment=cfg.get_frag('color'))
    render = ngl.Render(geometry, prog)
    render.update_uniforms(color=ngl.UniformVec4(value=COLORS['rose']))
    return render


@test_fingerprint()
@scene()
def transform_matrix(cfg):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    mat = [
        0.5, 0.5, 0.0, 0.0,
       -1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
       -0.2, 0.4, 0.0, 1.0,
    ]
    return ngl.Transform(shape, matrix=mat)


@test_fingerprint(nb_keyframes=8, tolerance=1)
@scene()
def transform_animated_camera(cfg):
    cfg.duration = 5.
    g = ngl.Group()

    elems = (
        ('red',     None),
        ('yellow',  (-0.6,  0.8, -1)),
        ('green',   ( 0.6,  0.8, -1)),
        ('cyan',    (-0.6, -0.5, -1)),
        ('magenta', ( 0.6, -0.5, -1)),
    )

    quad = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    prog = ngl.Program(fragment=cfg.get_frag('color'))
    for color, vector in elems:
        node = ngl.Render(quad, prog)
        node.update_uniforms(color=ngl.UniformVec4(value=COLORS[color]))
        if vector:
            node = ngl.Translate(node, vector=vector)
        g.add_children(node)

    g = ngl.GraphicConfig(g, depth_test=True)
    camera = ngl.Camera(g)
    camera.set_eye(0, 0, 2)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(0.1, 10.0)

    tr_animkf = [ngl.AnimKeyFrameVec3(0,  (0.0, 0.0, 0.0)),
                 ngl.AnimKeyFrameVec3(cfg.duration, (0.0, 0.0, 3.0))]
    eye_transform = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(tr_animkf))

    rot_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                  ngl.AnimKeyFrameFloat(cfg.duration, 360)]
    eye_transform = ngl.Rotate(eye_transform, axis=(0, 1, 0), anim=ngl.AnimatedFloat(rot_animkf))

    camera.set_eye_transform(eye_transform)

    fov_animkf = [ngl.AnimKeyFrameFloat(0.5, 60.0),
                  ngl.AnimKeyFrameFloat(cfg.duration, 45.0)]
    camera.set_fov_anim(ngl.AnimatedFloat(fov_animkf))

    return camera


@test_fingerprint()
@scene(vector=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_translate(cfg, vector=(0.2, 0.7, -0.4)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.Translate(shape, vector)


@test_fingerprint()
@scene()
def transform_translate_animated(cfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3.
    p0, p1, p2 = equilateral_triangle_coords()
    anim = [
        ngl.AnimKeyFrameVec3(0, p0),
        ngl.AnimKeyFrameVec3(1 * cfg.duration / 3., p1),
        ngl.AnimKeyFrameVec3(2 * cfg.duration / 3., p2),
        ngl.AnimKeyFrameVec3(cfg.duration, p0),
    ]
    shape = _transform_shape(cfg)
    return ngl.Translate(shape, anim=ngl.AnimatedVec3(anim))


@test_fingerprint()
@scene(factors=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_scale(cfg, factors=(0.7, 1.4, 0)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.Scale(shape, factors)


@test_fingerprint()
@scene(factors=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
       anchor=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_scale_anchor(cfg, factors=(0.7, 1.4, 0), anchor=(-0.4, 0.5, 0.7)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.Scale(shape, factors, anchor=anchor)


@test_fingerprint()
@scene(factors=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_scale_animated(cfg, factors=(0.7, 1.4, 0)):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 2.0
    shape = _transform_shape(cfg)
    anim = [
        ngl.AnimKeyFrameVec3(0, (0, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2., factors),
        ngl.AnimKeyFrameVec3(cfg.duration, (0, 0, 0)),
    ]
    return ngl.Scale(shape, anim=ngl.AnimatedVec3(anim))


@test_fingerprint()
@scene(factors=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
       anchor=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_scale_anchor_animated(cfg, factors=(0.7, 1.4, 0), anchor=(-0.4, 0.5, 0.7)):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 2.0
    shape = _transform_shape(cfg)
    anim = [
        ngl.AnimKeyFrameVec3(0, (0, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2., factors),
        ngl.AnimKeyFrameVec3(cfg.duration, (0, 0, 0)),
    ]
    return ngl.Scale(shape, anim=ngl.AnimatedVec3(anim), anchor=anchor)


@test_fingerprint()
@scene(angle=scene.Range(range=[0, 360], unit_base=10))
def transform_rotate(cfg, angle=123.4):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.Rotate(shape, angle)


@test_fingerprint()
@scene(angle=scene.Range(range=[0, 360], unit_base=10),
       anchor=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_rotate_anchor(cfg, angle=123.4, anchor=(0.15, 0.35, 0.7)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.Rotate(shape, angle, anchor=anchor)


@test_fingerprint()
@scene(quat=scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)))
def transform_rotate_quat(cfg, quat=(0, 0, -0.474, 0.880)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.RotateQuat(shape, quat)


@test_fingerprint()
@scene(quat=scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)),
       anchor=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)))
def transform_rotate_quat_anchor(cfg, quat=(0, 0, -0.474, 0.880), anchor=(0.15, 0.35, 0.7)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    return ngl.RotateQuat(shape, quat, anchor=anchor)


@test_fingerprint(nb_keyframes=8)
@scene(quat0=scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)),
       quat1=scene.Vector(n=4, minv=(-1, -1, -1, -1), maxv=(1, 1, 1, 1)))
def transform_rotate_quat_animated(cfg, quat0=(0, 0, -0.474, 0.880), quat1=(0, 0, 0, 0)):
    cfg.aspect_ratio = (1, 1)
    shape = _transform_shape(cfg)
    anim = [
        ngl.AnimKeyFrameQuat(0, quat0),
        ngl.AnimKeyFrameQuat(cfg.duration / 2., quat1),
        ngl.AnimKeyFrameQuat(cfg.duration, quat0),
    ]
    return ngl.RotateQuat(shape, anim=ngl.AnimatedQuat(anim))
