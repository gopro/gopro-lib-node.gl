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
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.toolbox.colors import COLORS


def _get_cube():
    # front
    p0 = (-1, -1, 1)
    p1 = ( 1, -1, 1)
    p2 = ( 1,  1, 1)
    p3 = (-1,  1, 1)

    # back
    p4 = (-1, -1, -1)
    p5 = ( 1, -1, -1)
    p6 = ( 1,  1, -1)
    p7 = (-1,  1, -1)

    cube_vertices_data = array.array(
        'f',
        p0 + p1 + p2 + p2 + p3 + p0 +  # front
        p1 + p5 + p6 + p6 + p2 + p1 +  # right
        p7 + p6 + p5 + p5 + p4 + p7 +  # back
        p4 + p0 + p3 + p3 + p7 + p4 +  # left
        p4 + p5 + p1 + p1 + p0 + p4 +  # bottom
        p3 + p2 + p6 + p6 + p7 + p3    # top
    )

    uvs = (0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1)
    cube_uvs_data = array.array('f', uvs * 6)

    up = (0, 1, 0)
    down = (0, -1, 0)
    front = (0, 0, 1)
    back = (0, 0, -1)
    left = (-1, 0, 0)
    right = (1, 0, 0)

    cube_normals_data = array.array(
        'f',
        front * 6 +
        right * 6 +
        back * 6 +
        left * 6 +
        down * 6 +
        up * 6
    )

    return ngl.Geometry(
        vertices=ngl.BufferVec3(data=cube_vertices_data),
        uvcoords=ngl.BufferVec2(data=cube_uvs_data),
        normals=ngl.BufferVec3(data=cube_normals_data),
    )


_RENDER_CUBE_VERT = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_normal = ngl_normal_matrix * ngl_normal;
}
'''


_RENDER_CUBE_FRAG = '''
void main()
{
    ngl_out_color = vec4((var_normal + 1.0) / 2.0, 1.0);
}
'''


def _get_rtt_scene(cfg, features='depth', texture_ds_format=None, samples=0, mipmap_filter='none'):
    cfg.duration = 10
    cfg.aspect_ratio = (1, 1)
    cube = _get_cube()
    program = ngl.Program(vertex=_RENDER_CUBE_VERT, fragment=_RENDER_CUBE_FRAG)
    program.update_vert_out_vars(var_normal=ngl.IOVec3())
    render = ngl.Render(cube, program)
    render = ngl.Scale(render, (0.5, 0.5, 0.5))

    for i in range(3):
        rot_animkf = ngl.AnimatedFloat([
            ngl.AnimKeyFrameFloat(0,            0),
            ngl.AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))
        ])
        axis = [int(i == x) for x in range(3)]
        render = ngl.Rotate(render, axis=axis, anim=rot_animkf)

    config = ngl.GraphicConfig(render, depth_test=True)

    camera = ngl.Camera(
        config,
        eye=(0.0, 0.0, 3.0),
        center=(0.0, 0.0, 0.0),
        up=(0.0, 1.0, 0.0),
        perspective=(45.0, cfg.aspect_ratio_float),
        clipping=(1.0, 10.0),
    )

    size = 1024
    texture_depth = None
    if texture_ds_format:
        texture_depth = ngl.Texture2D(width=size, height=size, format=texture_ds_format)

    texture = ngl.Texture2D(
        width=size,
        height=size,
        min_filter='linear',
        mipmap_filter=mipmap_filter,
    )
    rtt = ngl.RenderToTexture(
        camera,
        [texture],
        features=features,
        depth_texture=texture_depth,
        samples=samples,
        clear_color=(0, 0, 0, 1),
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=texture)
    return ngl.Group(children=(rtt, render))


def _get_rtt_function(**kwargs):
    @test_fingerprint(width=512, height=512, nb_keyframes=10)
    @scene()
    def rtt_function(cfg):
        return _get_rtt_scene(cfg, **kwargs)
    return rtt_function


_rtt_tests = dict(
    feature_depth=dict(features='depth'),
    feature_depth_stencil=dict(features='depth+stencil'),
    feature_depth_msaa=dict(features='depth', samples=4),
    feature_depth_stencil_msaa=dict(features='depth+stencil', samples=4),
    mipmap=dict(features='depth', mipmap_filter='linear'),
    texture_depth=dict(texture_ds_format='auto_depth'),
    texture_depth_stencil=dict(texture_ds_format='auto_depth_stencil'),
    texture_depth_msaa=dict(texture_ds_format='auto_depth', samples=4),
    texture_depth_stencil_msaa=dict(texture_ds_format='auto_depth_stencil', samples=4),
)


for name, params in _rtt_tests.items():
    globals()['rtt_' + name] = _get_rtt_function(**params)


@test_cuepoints(width=32, height=32, points={'bottom-left': (-0.5, -0.5), 'top-right': (0.5, 0.5)}, tolerance=1)
@scene()
def rtt_load_attachment(cfg):
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    background = ngl.Render(quad, program)
    background.update_frag_resources(color=ngl.UniformVec4(value=COLORS['white']))

    program = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(color=ngl.UniformVec4(value=COLORS['orange']))

    texture = ngl.Texture2D(width=16, height=16)
    rtt = ngl.RenderToTexture(render, [texture])

    quad = ngl.Quad((0, 0, 0), (1, 0, 0), (0, 1, 0))
    program = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    program.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    foreground = ngl.Render(quad, program)
    foreground.update_frag_resources(tex0=texture)

    return ngl.Group(children=(background, rtt, foreground))
