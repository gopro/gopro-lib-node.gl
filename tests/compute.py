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
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.debug import get_debug_points
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint


_PARTICULES_COMPUTE = '''
layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

void main()
{
    uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationIndex;
    vec3 iposition = idata.positions[i];
    vec2 ivelocity = idata.velocities[i];
    vec3 position;
    position.x = iposition.x + time * ivelocity.x;
    position.y = iposition.y + 0.1 * sin(time * duration * ivelocity.y);
    position.z = 0.0;
    odata.positions[i] = position;
}
'''


_PARTICULES_VERT = '''
void main()
{
    vec4 position = ngl_position + vec4(data.positions[ngl_instance_index], 0.0);
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * position;
}
'''


@test_fingerprint(nb_keyframes=10, tolerance=1)
@scene()
def compute_particules(cfg):
    random.seed(0)
    cfg.duration = 10
    local_size = 4
    nb_particules = 128

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
            ngl.BufferVec3(data=positions, label='positions'),
            ngl.BufferVec2(data=velocities, label='velocities'),
        ],
        layout='std430',
    )
    opositions = ngl.Block(fields=[ngl.BufferVec3(count=nb_particules, label='positions')], layout='std430')

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1.0),
    ]
    time = ngl.AnimatedFloat(animkf)
    duration = ngl.UniformFloat(cfg.duration)

    group_size = nb_particules / local_size
    program = ngl.ComputeProgram(_PARTICULES_COMPUTE % dict(local_size=local_size))
    compute = ngl.Compute(nb_particules, 1, 1, program)
    compute.update_resources(time=time, duration=duration, idata=ipositions, odata=opositions)

    circle = ngl.Circle(radius=0.05)
    program = ngl.Program(vertex=_PARTICULES_VERT, fragment=cfg.get_frag('color'))
    render = ngl.Render(circle, program, nb_instances=nb_particules)
    render.update_frag_resources(color=ngl.UniformVec4(value=COLORS['sgreen']))
    render.update_vert_resources(data=opositions)

    group = ngl.Group()
    group.add_children(compute, render)
    return group


_COMPUTE_HISTOGRAM_CLEAR = '''
layout(local_size_x = %(local_size)d, local_size_y = 1, local_size_z = 1) in;

void main()
{
    uint i = gl_GlobalInvocationID.x;
    atomicAnd(hist.r[i], 0U);
    atomicAnd(hist.g[i], 0U);
    atomicAnd(hist.b[i], 0U);
    atomicAnd(hist.max.r, 0U);
    atomicAnd(hist.max.g, 0U);
    atomicAnd(hist.max.b, 0U);
}
'''


_COMPUTE_HISTOGRAM_EXEC = '''
layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    ivec2 size = imageSize(source);
    if (x < uint(size.x) && y < uint(size.y)) {
        vec4 color = imageLoad(source, ivec2(x, y));
        uvec4 ucolor = uvec4(color * (%(hsize)d.0 - 1.0));
        uint r = atomicAdd(hist.r[ucolor.r], 1U);
        uint g = atomicAdd(hist.g[ucolor.g], 1U);
        uint b = atomicAdd(hist.b[ucolor.b], 1U);
        atomicMax(hist.max.r, r);
        atomicMax(hist.max.g, g);
        atomicMax(hist.max.b, b);
    }
}
'''


_RENDER_HISTOGRAM_VERT = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_uvcoord;
}
'''


_RENDER_HISTOGRAM_FRAG = '''
void main()
{
    uint x = uint(var_uvcoord.x * %(size)d.0);
    uint y = uint(var_uvcoord.y * %(size)d.0);
    uint i = clamp(x + y * %(size)dU, 0U, %(hsize)dU - 1U);
    vec3 rgb = vec3(hist.r[i], hist.g[i], hist.b[i]) / vec3(hist.max);
    ngl_out_color = vec4(rgb, 1.0);
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
    texture.set_format('r32g32b32a32_sfloat')

    histogram_block = ngl.Block(layout='std430', label='histogram')
    histogram_block.add_fields(
        ngl.BufferUInt(hsize, label='r'),
        ngl.BufferUInt(hsize, label='g'),
        ngl.BufferUInt(hsize, label='b'),
        ngl.UniformUIVec3(label='max'),
    )

    shader_params = dict(hsize=hsize, size=size, local_size=local_size)

    group_size = hsize // local_size
    clear_histogram_shader = _COMPUTE_HISTOGRAM_CLEAR % shader_params
    clear_histogram_program = ngl.ComputeProgram(clear_histogram_shader)
    clear_histogram = ngl.Compute(
        group_size,
        1,
        1,
        clear_histogram_program,
        label='clear_histogram',
    )
    clear_histogram.update_resources(hist=histogram_block)

    group_size = size // local_size
    exec_histogram_shader = _COMPUTE_HISTOGRAM_EXEC % shader_params
    exec_histogram_program = ngl.ComputeProgram(exec_histogram_shader)
    exec_histogram = ngl.Compute(
        group_size,
        group_size,
        1,
        exec_histogram_program,
        label='compute_histogram'
    )
    exec_histogram.update_resources(hist=histogram_block, source=texture)
    exec_histogram_program.update_properties(source=ngl.ResourceProps(as_image=True))

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(
        vertex=_RENDER_HISTOGRAM_VERT,
        fragment=_RENDER_HISTOGRAM_FRAG % shader_params,
    )
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program, label='render_histogram')
    render.update_frag_resources(hist=histogram_block)

    group = ngl.Group(children=(clear_histogram, exec_histogram, render,))
    if show_dbg_points:
        cuepoints = _get_compute_histogram_cuepoints()
        group.add_children(get_debug_points(cfg, cuepoints))
    return group


_ANIMATION_COMPUTE = '''
layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

void main()
{
    uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationIndex;
    dst.vertices[i] = vec3(transform * vec4(src.vertices[i], 1.0));
}
'''


@test_fingerprint(nb_keyframes=5, tolerance=1)
@scene()
def compute_animation(cfg):
    cfg.duration = 5
    cfg.aspect_ratio = (1, 1)
    local_size = 2

    compute_shader = _ANIMATION_COMPUTE % dict(local_size=local_size)

    vertices_data = array.array('f', [
        -0.5, -0.5, 0.0,
         0.5, -0.5, 0.0,
        -0.5,  0.5, 0.0,
         0.5,  0.5, 0.0,
    ])
    nb_vertices = 4

    input_vertices = ngl.BufferVec3(data=vertices_data, label='vertices')
    output_vertices = ngl.BufferVec3(data=vertices_data, label='vertices')
    input_block = ngl.Block(fields=[input_vertices], layout='std140')
    output_block = ngl.Block(fields=[output_vertices], layout='std430')

    rotate_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                     ngl.AnimKeyFrameFloat(cfg.duration, 360)]
    rotate = ngl.Rotate(ngl.Identity(), axis=(0, 0, 1), anim=ngl.AnimatedFloat(rotate_animkf))
    transform = ngl.UniformMat4(transform=rotate)

    program = ngl.ComputeProgram(compute_shader)
    compute = ngl.Compute(nb_vertices / (local_size ** 2), 1, 1, program)
    compute.update_resources(transform=transform, src=input_block, dst=output_block)

    quad_buffer = ngl.BufferVec3(block=output_block, block_field=0)
    geometry = ngl.Geometry(quad_buffer, topology='triangle_strip')
    program = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    render = ngl.Render(geometry, program)
    render.update_frag_resources(color=ngl.UniformVec4(value=COLORS['sgreen']))

    return ngl.Group(children=(compute, render))
