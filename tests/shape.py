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
import itertools
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.toolbox.shapes import equilateral_triangle_coords
from pynodegl_utils.toolbox.grid import autogrid_simple


def _render_shape(cfg, geometry, color):
    prog = ngl.Program(fragment=cfg.get_frag('color'))
    render = ngl.Render(geometry, prog)
    render.update_uniforms(color=ngl.UniformVec4(value=color))
    return render


@test_fingerprint()
@scene(sz=scene.Range(range=[0.1, 2], unit_base=100),
       color=scene.Color())
def shape_triangle(cfg, sz=1, color=COLORS['orange']):
    cfg.aspect_ratio = (1, 1)
    p0, p1, p2 = equilateral_triangle_coords(sz)
    geometry = ngl.Triangle(p0, p1, p2)
    return _render_shape(cfg, geometry, color)


@test_fingerprint()
@scene(corner=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
       width=scene.Vector(n=3, minv=(0, 0, 0), maxv=(2, 2, 2)),
       height=scene.Vector(n=3, minv=(0, 0, 0), maxv=(2, 2, 2)),
       color=scene.Color())
def shape_quad(cfg, corner=(-.5, -.8, 0), width=(0.9, 0.2, 0), height=(0.1, 1.3, 0), color=COLORS['sgreen']):
    cfg.aspect_ratio = (1, 1)
    geometry = ngl.Quad(corner, width, height)
    return _render_shape(cfg, geometry, color)


@test_fingerprint()
@scene(radius=scene.Range(range=[0.1, 2], unit_base=100),
       color=scene.Color())
def shape_circle(cfg, radius=0.5, color=COLORS['azure']):
    cfg.aspect_ratio = (1, 1)
    geometry = ngl.Circle(radius, npoints=64)
    return _render_shape(cfg, geometry, color)


def _shape_geometry(cfg, set_normals=False, set_indices=False):
    # Fake cube (3 faces only) obtained from:
    # echo 'cube();'>x.scad; openscad x.scad -o x.stl
    vertices = array.array('f', [x - 0.5 for x in [
        1,1,0, 0,1,1, 1,1,1,
        0,1,1, 1,1,0, 0,1,0,
        0,0,0, 0,1,1, 0,1,0,
        0,1,1, 0,0,0, 0,0,1,
        0,1,1, 1,0,1, 1,1,1,
        1,0,1, 0,1,1, 0,0,1,
    ]])

    normals = array.array('f', [
        0,1,0,  0,1,0,  0,1,0,
        0,1,0,  0,1,0,  0,1,0,
       -1,0,0, -1,0,0, -1,0,0,
       -1,0,0, -1,0,0, -1,0,0,
        0,0,1,  0,0,1,  0,0,1,
        0,0,1,  0,0,1,  0,0,1,
    ])

    vertices_buffer = ngl.BufferVec3(data=vertices)
    normals_buffer = ngl.BufferVec3(data=normals)

    cfg.aspect_ratio = (1, 1)
    geometry = ngl.Geometry(vertices=vertices_buffer)

    if set_normals:
        geometry.set_normals(normals_buffer)
        prog = ngl.Program(fragment=cfg.get_frag('colored-normals'))
        render = ngl.Render(geometry, prog)
    else:
        render = _render_shape(cfg, geometry, COLORS['magenta'])

    if set_indices:
        indices = array.array('H', list(range(3 * 6)))
        indices_buffer = ngl.BufferUShort(data=indices)
        geometry.set_indices(indices_buffer)

    return ngl.Rotate(render, 45, axis=(1, 1, 1))


@test_fingerprint()
@scene()
def shape_geometry(cfg):
    return _shape_geometry(cfg, set_normals=False, set_indices=False)


@test_fingerprint()
@scene()
def shape_geometry_normals(cfg):
    return _shape_geometry(cfg, set_normals=True, set_indices=False)


@test_fingerprint()
@scene()
def shape_geometry_indices(cfg):
    return _shape_geometry(cfg, set_normals=False, set_indices=True)


@test_fingerprint()
@scene()
def shape_geometry_normals_indices(cfg):
    return _shape_geometry(cfg, set_normals=True, set_indices=True)


@test_fingerprint()
@scene()
def shape_diamond_colormask(cfg):
    color_write_masks = ('r+g+b+a', 'r+g+a', 'g+b+a', 'r+b+a')
    geometry = ngl.Circle(npoints=5)
    prog = ngl.Program(fragment=cfg.get_frag('color'))
    render = ngl.Render(geometry, prog)
    render.update_uniforms(color=ngl.UniformVec4(value=COLORS['white']))
    scenes = [ngl.GraphicConfig(render, color_write_mask=cwm) for cwm in color_write_masks]
    return autogrid_simple(scenes)


def _shape_geometry_rtt(cfg, depth=False, samples=0):
    w, h = 640, 480

    scene = _shape_geometry(cfg, set_normals=True)

    if depth:
        scene = ngl.GraphicConfig(scene, depth_test=True)

    texture = ngl.Texture2D()
    texture.set_width(w)
    texture.set_height(h)

    rtt = ngl.RenderToTexture(scene)
    rtt.add_color_textures(texture)

    if depth:
        texture = ngl.Texture2D()
        texture.set_format('d16_unorm')
        texture.set_width(w)
        texture.set_height(h)
        rtt.set_depth_texture(texture)
    else:
        rtt.set_clear_color(*COLORS['cgreen'])

    if samples:
        rtt.set_samples(samples)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad)
    render.update_textures(tex0=texture)

    return ngl.Group(children=(rtt, render))


@test_fingerprint()
@scene()
def shape_geometry_rtt(cfg):
    return _shape_geometry_rtt(cfg)


@test_fingerprint()
@scene()
def shape_geometry_rtt_depth(cfg):
    return _shape_geometry_rtt(cfg, depth=True)


@test_fingerprint()
@scene()
def shape_geometry_rtt_samples(cfg):
    return _shape_geometry_rtt(cfg, samples=4)


def _get_morphing_coordinates(n, x_off, y_off):
    coords = [(random.uniform(0, 1) + x_off,
               random.uniform(0, 1) + y_off, 0) for i in range(n - 1)]
    coords.append(coords[0])  # smooth loop
    return coords


@test_fingerprint(nb_keyframes=8, tolerance=1)
@scene(n=scene.Range(range=[2, 50]))
def shape_morphing(cfg, n=6):
    cfg.duration = 5.0
    random.seed(0)
    vertices_tl = _get_morphing_coordinates(n, -1, 0)
    vertices_tr = _get_morphing_coordinates(n,  0, 0)
    vertices_bl = _get_morphing_coordinates(n, -1,-1)
    vertices_br = _get_morphing_coordinates(n,  0,-1)

    vertices_animkf = []
    for i, coords in enumerate(zip(vertices_tl, vertices_tr, vertices_bl, vertices_br)):
        flat_coords = list(itertools.chain(*coords))
        coords_array = array.array('f', flat_coords)
        vertices_animkf.append(ngl.AnimKeyFrameBuffer(i * cfg.duration / (n - 1), coords_array))
    vertices = ngl.AnimatedBufferVec3(vertices_animkf)

    geom = ngl.Geometry(vertices)
    geom.set_topology('triangle_fan')
    p = ngl.Program(fragment=cfg.get_frag('color'))
    render = ngl.Render(geom, p)
    render.update_uniforms(color=ngl.UniformVec4(COLORS['cyan']))
    return render


def _get_cropboard_function(set_indices=False):

    @test_fingerprint(nb_keyframes=10, tolerance=1)
    @scene(dim_clr=scene.Range(range=[1, 50]),
           dim_cut=scene.Range(range=[1, 50]))
    def cropboard(cfg, dim_clr=3, dim_cut=9):
        cfg.duration = 5. + 1.

        random.seed(0)

        get_rand = lambda: array.array('f', [random.random() for i in range(dim_clr ** 2 * 3)])
        nb_kf = 2
        buffers = [get_rand() for i in range(nb_kf)]
        random_animkf = []
        time_scale = cfg.duration / float(nb_kf)
        for i, buf in enumerate(buffers + [buffers[0]]):
            random_animkf.append(ngl.AnimKeyFrameBuffer(i*time_scale, buf))
        random_buffer = ngl.AnimatedBufferVec3(keyframes=random_animkf)
        random_tex = ngl.Texture2D(data_src=random_buffer, width=dim_clr, height=dim_clr)

        kw = kh = 1. / dim_cut
        qw = qh = 2. / dim_cut

        p = ngl.Program(vertex=cfg.get_vert('cropboard'))

        uv_offset_buffer = array.array('f')
        translate_a_buffer = array.array('f')
        translate_b_buffer = array.array('f')

        if set_indices:
            indices = array.array('H', [0, 2, 1, 3])
            indices_buffer = ngl.BufferUShort(data=indices)

            vertices = array.array('f', [
                 0, 0,  0,
                qw, qh, 0,
                qw, 0,  0,
                 0, qh, 0,
            ])

            uvcoords = array.array('f', [
                 0, 1.0,
                kw, 1.0 - kh,
                kw, 1.0,
                 0, 1.0 - kh,
            ])

            vertices_buffer = ngl.BufferVec3(data=vertices)
            uvcoords_buffer = ngl.BufferVec2(data=uvcoords)

            q = ngl.Geometry(topology='triangle_fan',
                             vertices=vertices_buffer,
                             uvcoords=uvcoords_buffer,
                             indices=indices_buffer)
        else:
            q = ngl.Quad(corner=(0, 0, 0), width=(qw, 0, 0), height=(0, qh, 0),
                         uv_corner=(0, 0), uv_width=(kw, 0), uv_height=(0, kh))

        for y in range(dim_cut):
            for x in range(dim_cut):
                uv_offset = [x*kw, (y+1.)*kh - 1.]
                src = [random.uniform(-2, 2), random.uniform(-2, 2)]
                dst = [x*qw - 1., 1. - (y+1.)*qh]

                uv_offset_buffer.extend(uv_offset)
                translate_a_buffer.extend(src)
                translate_b_buffer.extend(dst)

        utime_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                        ngl.AnimKeyFrameFloat(cfg.duration - 1., 1)]
        utime = ngl.AnimatedFloat(utime_animkf)

        render = ngl.Render(q, p, nb_instances=dim_cut**2)
        render.update_textures(tex0=random_tex)
        render.update_uniforms(time=utime)
        render.update_instance_attributes(
            uv_offset=ngl.BufferVec2(data=uv_offset_buffer),
            translate_a=ngl.BufferVec2(data=translate_a_buffer),
            translate_b=ngl.BufferVec2(data=translate_b_buffer),
        )

        return render

    return cropboard


shape_cropboard = _get_cropboard_function(set_indices=False)
shape_cropboard_indices = _get_cropboard_function(set_indices=True)
