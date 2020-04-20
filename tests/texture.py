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
from pynodegl_utils.tests.debug import get_debug_points
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.toolbox.colors import COLORS


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


_RENDER_TO_CUBEMAP_VERT = '''
in vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;

void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
}
'''


_RENDER_TO_CUBEMAP_FRAG = '''
precision mediump float;
out vec4 frag_color[6];

void main()
{
    frag_color[0] = vec4(1.0, 0.0, 0.0, 1.0); // right
    frag_color[1] = vec4(0.0, 1.0, 0.0, 1.0); // left
    frag_color[2] = vec4(0.0, 0.0, 1.0, 1.0); // top
    frag_color[3] = vec4(1.0, 1.0, 0.0, 1.0); // bottom
    frag_color[4] = vec4(0.0, 1.0, 1.0, 1.0); // back
    frag_color[5] = vec4(1.0, 0.0, 1.0, 1.0); // front
}
'''


_RENDER_CUBEMAP_VERT = '''
in vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
out vec3 var_uvcoord;

void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_position.xyz;
}
'''


_RENDER_CUBEMAP_FRAG = '''
precision mediump float;
out vec4 frag_color;
in vec3 var_uvcoord;
uniform samplerCube tex0_sampler;

void main()
{
    frag_color = texture(tex0_sampler, vec3(var_uvcoord.xy, 0.5));
}
'''


def _get_texture_cubemap_from_mrt_scene(cfg, samples=0):
    glsl_version = '300 es' if cfg.backend == 'gles' else '330'
    glsl_header = '#version %s\n' % glsl_version

    render_to_cubemap_vert = glsl_header + _RENDER_TO_CUBEMAP_VERT
    render_to_cubemap_frag = glsl_header + _RENDER_TO_CUBEMAP_FRAG
    program = ngl.Program(vertex=render_to_cubemap_vert, fragment=render_to_cubemap_frag)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    cube = ngl.TextureCube(size=64, min_filter="linear", mag_filter="linear")
    rtt = ngl.RenderToTexture(render, [cube], samples=samples)

    render_cubemap_vert = glsl_header + _RENDER_CUBEMAP_VERT
    render_cubemap_frag = glsl_header + _RENDER_CUBEMAP_FRAG
    program = ngl.Program(vertex=render_cubemap_vert, fragment=render_cubemap_frag)
    render = ngl.Render(quad, program)
    render.update_textures(tex0=cube)

    return ngl.Group(children=(rtt, render))


@test_fingerprint()
@scene()
def texture_cubemap(cfg):
    n = 64
    p = n * n
    cb_data = array.array(
        'B',
        (255, 0, 0, 255) * p +
        (0, 255, 0, 255) * p +
        (0, 0, 255, 255) * p +
        (255, 255, 0, 255) * p +
        (0, 255, 255, 255) * p +
        (255, 0, 255, 255) * p
    )
    cb_buffer = ngl.BufferUBVec4(data=cb_data)
    cube = ngl.TextureCube(size=n, min_filter="linear", mag_filter="linear", data_src=cb_buffer)

    glsl_version = '300 es' if cfg.backend == 'gles' else '330'
    glsl_header = '#version %s\n' % glsl_version
    render_cubemap_vert = glsl_header + _RENDER_CUBEMAP_VERT
    render_cubemap_frag = glsl_header + _RENDER_CUBEMAP_FRAG
    program = ngl.Program(vertex=render_cubemap_vert, fragment=render_cubemap_frag)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    render.update_textures(tex0=cube)
    return render


@test_fingerprint()
@scene()
def texture_cubemap_from_mrt(cfg):
    return _get_texture_cubemap_from_mrt_scene(cfg)


@test_fingerprint()
@scene()
def texture_cubemap_from_mrt_msaa(cfg):
    return _get_texture_cubemap_from_mrt_scene(cfg, 4)


@test_cuepoints(width=32, height=32, points={'bottom-left': (-1, -1), 'top-right': (1, 1)}, tolerance=1)
@scene()
def texture_clear_and_scissor(cfg):
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    color = ngl.UniformVec4(COLORS['white'])
    program = ngl.Program(fragment=cfg.get_frag('color'))
    render = ngl.Render(quad, program)
    render.update_uniforms(color=color)
    graphic_config = ngl.GraphicConfig(render, scissor_test=True, scissor=(0, 0, 0, 0))

    texture = ngl.Texture2D(width=64, height=64)
    rtt = ngl.RenderToTexture(ngl.Identity(), [texture], clear_color=COLORS['orange'])

    render = ngl.Render(quad)
    render.update_textures(tex0=texture)

    return ngl.Group(children=(graphic_config, rtt, render))


@test_fingerprint(width=64, height=64)
@scene()
def texture_scissor(cfg):
    cfg.aspect_ratio = (1, 1)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    color = ngl.UniformVec4(COLORS['orange'])
    program = ngl.Program(fragment=cfg.get_frag('color'))
    render = ngl.Render(quad, program)
    render.update_uniforms(color=color)
    graphic_config = ngl.GraphicConfig(render, scissor_test=True, scissor=(32, 32, 32, 32))
    texture = ngl.Texture2D(width=64, height=64)
    rtt = ngl.RenderToTexture(graphic_config, [texture])

    render = ngl.Render(quad)
    render.update_textures(tex0=texture)

    return ngl.Group(children=(rtt, render))


_TEXTURE3D_VERT = '''
in vec4 ngl_position;
in vec2 ngl_uvcoord;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
out vec2 var_uvcoord;

void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_uvcoord;
}
'''


_TEXTURE3D_FRAG = '''
precision mediump float;
precision mediump sampler3D;
out vec4 frag_color;
in vec2 var_uvcoord;
uniform sampler3D tex0_sampler;

void main()
{
    frag_color = texture(tex0_sampler, vec3(var_uvcoord, 0.0));
    frag_color += texture(tex0_sampler, vec3(var_uvcoord, 0.5));
    frag_color += texture(tex0_sampler, vec3(var_uvcoord, 1.0));
}
'''


@test_fingerprint()
@scene()
def texture_3d(cfg):
    random.seed(0)
    width, height, depth = 9, 9, 3
    n = width * height
    data = array.array('B')
    for i in random.sample(range(n), n):
        data.extend([i * 255 // n, 0, 0, 255])
    for i in random.sample(range(n), n):
        data.extend([0, i * 255 // n, 0, 255])
    for i in random.sample(range(n), n):
        data.extend([0, 0, i * 255 // n, 255])
    texture_buffer = ngl.BufferUBVec4(data=data)
    texture = ngl.Texture3D(width=width, height=height, depth=depth, data_src=texture_buffer)

    glsl_version = '300 es' if cfg.backend == 'gles' else '330'
    glsl_header = '#version %s\n' % glsl_version
    render_cubemap_vert = glsl_header + _TEXTURE3D_VERT
    render_cubemap_frag = glsl_header + _TEXTURE3D_FRAG

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=render_cubemap_vert, fragment=render_cubemap_frag)
    render = ngl.Render(quad, program)
    render.update_textures(tex0=texture)
    return render


_RENDER_TEXTURE_LOD_VERT = '''
#version %(version)s
in vec4 ngl_position;
in vec2 ngl_uvcoord;
out vec2 var_uvcoord;

void main()
{
    gl_Position = ngl_position;
    var_uvcoord = ngl_uvcoord;
}
'''


_RENDER_TEXTURE_LOD_FRAG = '''
#version %(version)s
precision mediump float;
in vec2 var_uvcoord;
out vec4 frag_color;
uniform sampler2D tex0_sampler;

void main()
{
    frag_color = textureLod(tex0_sampler, var_uvcoord, 0.5);
}
'''


_N = 8


def _get_texture_mipmap_cuepoints():
    f = float(_N)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return dict(('%d%d' % (x, y), (c(x), c(y))) for y in range(_N) for x in range(_N))


@test_cuepoints(points=_get_texture_mipmap_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def texture_mipmap(cfg, show_dbg_points=False):
    cfg.aspect_ratio = (1, 1)
    cuepoints = _get_texture_mipmap_cuepoints()
    black = (0, 0, 0, 255)
    white = (255, 255, 255, 255)
    p = _N // 2
    cb_data = array.array(
        'B',
        ((black + white) * p + (white + black) * p) * p,
    )
    cb_buffer = ngl.BufferUBVec4(data=cb_data)

    texture = ngl.Texture2D(
        width=_N,
        height=_N,
        min_filter='nearest',
        mipmap_filter='linear',
        data_src=cb_buffer,
    )

    shader_version = '300 es' if cfg.backend == 'gles' else '330'
    program = ngl.Program(
        vertex=_RENDER_TEXTURE_LOD_VERT % dict(version=shader_version),
        fragment=_RENDER_TEXTURE_LOD_FRAG % dict(version=shader_version),
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)
    render.update_textures(tex0=texture)

    group = ngl.Group(children=(render,))
    if show_dbg_points:
        group.add_children(get_debug_points(cfg, cuepoints))

    return group
