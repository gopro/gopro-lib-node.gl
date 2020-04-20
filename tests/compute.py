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

import array
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.debug import get_debug_points
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint


_PARTICULES_COMPUTE = '''
#version %(version)s

layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

layout (std430, binding = 0) buffer ipositions_buffer {
    vec3 ipositions[%(nb_particules)d];
    vec2 ivelocities[%(nb_particules)d];
};

layout (std430, binding = 1) buffer opositions_buffer {
    vec3 opositions[%(nb_particules)d];
};

uniform float time;
uniform float duration;

void main()
{
    uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationIndex;
    vec3 iposition = ipositions[i];
    vec2 ivelocity = ivelocities[i];
    vec3 position;
    position.x = iposition.x + time * ivelocity.x;
    position.y = iposition.y + 0.1 * sin(time * duration * ivelocity.y);
    position.z = 0.0;
    opositions[i] = position;
}
'''


_PARTICULES_VERT = '''
#version %(version)s
precision highp float;
in vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;

layout(std430, binding = 0) buffer positions_buffer {
    vec3 positions[];
};

void main()
{
    vec4 position = ngl_position + vec4(positions[gl_InstanceID], 0.0);
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * position;
}
'''


_PARTICULES_FRAG = '''
#version %(version)s
precision mediump float;
uniform vec4 color;
out vec4 frag_color;

void main()
{
    frag_color = color;
}
'''


@test_fingerprint(nb_keyframes=10, tolerance=1)
@scene()
def compute_particules(cfg):
    random.seed(0)
    cfg.duration = 10
    local_size = 4
    nb_particules = 128

    shader_version = '310 es' if cfg.backend == 'gles' else '430'
    shader_data = dict(
        version=shader_version,
        local_size=local_size,
        nb_particules=nb_particules,
    )
    compute_shader = _PARTICULES_COMPUTE % shader_data
    vertex_shader = _PARTICULES_VERT % shader_data
    fragment_shader = _PARTICULES_FRAG % shader_data

    positions = array.array('f')
    velocities = array.array('f')
    for i in range(nb_particules):
        positions.extend([
            random.uniform(-2.0, 1.0),
            random.uniform(-1.0, 1.0),
            0.0,
        ])
        velocities.extend([
            random.uniform(1.0, 2.0),
            random.uniform(0.5, 1.5),
        ])

    ipositions = ngl.Block(
        fields=[
            ngl.BufferVec3(data=positions),
            ngl.BufferVec2(data=velocities),
        ],
        layout='std430',
    )
    opositions = ngl.Block(fields=[ngl.BufferVec3(count=nb_particules)], layout='std430')

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1.0),
    ]
    time = ngl.AnimatedFloat(animkf)
    duration = ngl.UniformFloat(cfg.duration)

    group_size = nb_particules / local_size
    program = ngl.ComputeProgram(compute_shader)
    compute = ngl.Compute(nb_particules, 1, 1, program)
    compute.update_uniforms(time=time, duration=duration)
    compute.update_blocks(ipositions_buffer=ipositions, opositions_buffer=opositions)

    circle = ngl.Circle(radius=0.05)
    program = ngl.Program(vertex=vertex_shader, fragment=fragment_shader)
    render = ngl.Render(circle, program, nb_instances=nb_particules)
    render.update_uniforms(color=ngl.UniformVec4(value=COLORS['sgreen']))
    render.update_blocks(positions_buffer=opositions)

    group = ngl.Group()
    group.add_children(compute, render)
    return group


_COMPUTE_HISTOGRAM_CLEAR = '''
layout(local_size_x = %(local_size)d, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer histogram {
    uint histr[%(hsize)d];
    uint histg[%(hsize)d];
    uint histb[%(hsize)d];
    uvec3 max;
};

void main()
{
    uint i = gl_GlobalInvocationID.x;
    atomicAnd(histr[i], 0U);
    atomicAnd(histg[i], 0U);
    atomicAnd(histb[i], 0U);
    atomicAnd(max.r, 0U);
    atomicAnd(max.g, 0U);
    atomicAnd(max.b, 0U);
}
'''


_COMPUTE_HISTOGRAM_EXEC = '''
layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

layout(location = 0, rgba32f) readonly mediump uniform image2D source_sampler;

layout (std430, binding = 0) buffer histogram {
    uint histr[%(hsize)d];
    uint histg[%(hsize)d];
    uint histb[%(hsize)d];
    uvec3 max;
};

void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    ivec2 size = imageSize(source_sampler);
    if (x < uint(size.x) && y < uint(size.y)) {
        vec4 color = imageLoad(source_sampler, ivec2(x, y));
        uvec4 ucolor = uvec4(color * (%(hsize)d.0 - 1.0));
        uint r = atomicAdd(histr[ucolor.r], 1U);
        uint g = atomicAdd(histg[ucolor.g], 1U);
        uint b = atomicAdd(histb[ucolor.b], 1U);
        atomicMax(max.r, r);
        atomicMax(max.g, g);
        atomicMax(max.b, b);
    }
}
'''


_RENDER_HISTOGRAM_VERT = '''
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


_RENDER_HISTOGRAM_FRAG = '''
precision mediump float;
in vec2 var_uvcoord;
out vec4 frag_color;

layout(std430, binding = 0) buffer histogram {
    uint histr[%(hsize)d];
    uint histg[%(hsize)d];
    uint histb[%(hsize)d];
    uvec3 max;
};

void main()
{
    uint x = uint(var_uvcoord.x * %(size)d.0);
    uint y = uint(var_uvcoord.y * %(size)d.0);
    uint i = clamp(x + y * %(size)dU, 0U, %(hsize)dU - 1U);
    vec3 rgb = vec3(histr[i], histg[i], histb[i]) / vec3(max);
    frag_color = vec4(rgb, 1.0);
}
'''


_N = 8


def _get_compute_histogram_cuepoints():
    f = float(_N)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return dict(('%d%d' % (x, y), (c(x), c(y))) for y in range(_N) for x in range(_N))


@test_cuepoints(points=_get_compute_histogram_cuepoints(), tolerance=1)
@scene(show_dbg_points=scene.Bool())
def compute_histogram(cfg, show_dbg_points=False):
    random.seed(0)
    cfg.duration = 10
    cfg.aspect_ratio = (1, 1)
    hsize, size, local_size = _N * _N, _N, _N // 2
    data = array.array('f')
    for i in range(size * size):
        data.extend((
            random.uniform(0.0, 0.5),
            random.uniform(0.25, 0.75),
            random.uniform(0.5, 1.0),
            1.0,
        ))
    texture_buffer = ngl.BufferVec4(data=data)
    texture = ngl.Texture2D(width=size, height=size, data_src=texture_buffer)

    histogram_block = ngl.Block(layout='std430', label='histogram')
    histogram_block.add_fields(
        ngl.BufferUInt(hsize),
        ngl.BufferUInt(hsize),
        ngl.BufferUInt(hsize),
        ngl.UniformVec3(),
    )

    shader_version = '310 es' if cfg.backend == 'gles' else '430'
    shader_header = '#version %s\n' % shader_version
    if cfg.backend == 'gles' and cfg.system == 'Android':
        shader_header += '#extension GL_ANDROID_extension_pack_es31a: require\n'
    shader_params = dict(hsize=hsize, size=size, local_size=local_size)

    group_size = hsize // local_size
    clear_histogram_shader = _COMPUTE_HISTOGRAM_CLEAR % shader_params
    clear_histogram_program = ngl.ComputeProgram(shader_header + clear_histogram_shader)
    clear_histogram = ngl.Compute(
        group_size,
        1,
        1,
        clear_histogram_program,
        label='clear_histogram',
    )
    clear_histogram.update_blocks(histogram=histogram_block)

    group_size = size // local_size
    exec_histogram_shader = _COMPUTE_HISTOGRAM_EXEC % shader_params
    exec_histogram_program = ngl.ComputeProgram(shader_header + exec_histogram_shader)
    exec_histogram = ngl.Compute(
        group_size,
        group_size,
        1,
        exec_histogram_program,
        label='compute_histogram'
    )
    exec_histogram.update_blocks(histogram=histogram_block)
    exec_histogram.update_textures(source=texture)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(
        vertex=shader_header + _RENDER_HISTOGRAM_VERT,
        fragment=shader_header + _RENDER_HISTOGRAM_FRAG % shader_params,
    )
    render = ngl.Render(quad, program, label='render_histogram')
    render.update_blocks(histogram=histogram_block)

    group = ngl.Group(children=(clear_histogram, exec_histogram, render,))
    if show_dbg_points:
        cuepoints = _get_compute_histogram_cuepoints()
        group.add_children(get_debug_points(cfg, cuepoints))
    return group


_ANIMATION_COMPUTE = '''
#version %(version)s

layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

layout(std140, binding = 0) uniform input_block {
    vec3 vertices[4];
} src;

layout (std430, binding = 0) buffer output_block {
    vec3 vertices[4];
} dst;

uniform mat4 transform;

void main()
{
    uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationIndex;
    dst.vertices[i] = vec3(transform * vec4(src.vertices[i], 1.0));
}
'''


_ANIMATION_VERT = '''
#version %(version)s
precision highp float;
in vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;

void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
}
'''


_ANIMATION_FRAG = '''
#version %(version)s
precision mediump float;
uniform vec4 color;
out vec4 frag_color;

void main()
{
    frag_color = color;
}
'''


@test_fingerprint(nb_keyframes=5, tolerance=1)
@scene()
def compute_animation(cfg):
    cfg.duration = 5
    cfg.aspect_ratio = (1, 1)
    local_size = 2

    shader_version = '310 es' if cfg.backend == 'gles' else '430'
    shader_data = dict(
        version=shader_version,
        local_size=local_size,
    )
    compute_shader = _ANIMATION_COMPUTE % shader_data
    vertex_shader = _ANIMATION_VERT % shader_data
    fragment_shader = _ANIMATION_FRAG % shader_data

    vertices_data = array.array('f', [
        -0.5, -0.5, 0.0,
         0.5, -0.5, 0.0,
         0.5,  0.5, 0.0,
        -0.5,  0.5, 0.0,
    ])
    nb_vertices = 4

    input_vertices = ngl.BufferVec3(data=vertices_data)
    output_vertices = ngl.BufferVec3(data=vertices_data)
    input_block = ngl.Block(fields=[input_vertices], layout='std140')
    output_block = ngl.Block(fields=[output_vertices], layout='std430')

    rotate_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                     ngl.AnimKeyFrameFloat(cfg.duration, 360)]
    rotate = ngl.Rotate(ngl.Identity(), axis=(0, 0, 1), anim=ngl.AnimatedFloat(rotate_animkf))
    transform = ngl.UniformMat4(transform=rotate)

    program = ngl.ComputeProgram(compute_shader)
    compute = ngl.Compute(nb_vertices / (local_size ** 2), 1, 1, program)
    compute.update_uniforms(transform=transform)
    compute.update_blocks(input_block=input_block, output_block=output_block)

    quad_buffer = ngl.BufferVec3(block=output_block, block_field=0)
    geometry = ngl.Geometry(quad_buffer, topology='triangle_fan')
    program = ngl.Program(vertex=vertex_shader, fragment=fragment_shader)
    render = ngl.Render(geometry, program)
    render.update_uniforms(color=ngl.UniformVec4(value=COLORS['sgreen']))

    return ngl.Group(children=(compute, render))
