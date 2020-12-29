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
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.tests.debug import get_debug_points

from pynodegl_utils.tests.data import (
    ANIM_DURATION,
    FUNCS,
    LAYOUTS,
    gen_floats,
    gen_ints,
    get_data_debug_positions,
    match_fields,
    get_random_block_info,
    get_render,
)


def _get_data_spec(layout, i_count=6, f_count=7, v2_count=5, v3_count=9, v4_count=2, mat_count=3):
    f_list     = gen_floats(f_count)
    v2_list    = gen_floats(v2_count * 2)
    v3_list    = gen_floats(v3_count * 3)
    v4_list    = gen_floats(v4_count * 4)
    i_list     = gen_ints(i_count)
    iv2_list   = [int(x * 256) for x in v2_list]
    iv3_list   = [int(x * 256) for x in v3_list]
    iv4_list   = [int(x * 256) for x in v4_list]
    mat4_list  = gen_floats(mat_count * 4 * 4)
    one_f      = gen_floats(1)[0]
    one_v2     = gen_floats(2)
    one_v3     = gen_floats(3)
    one_v4     = gen_floats(4)
    one_i      = gen_ints(1)[0]
    one_b      = True
    one_iv2    = gen_ints(2)
    one_iv3    = gen_ints(3)
    one_iv4    = gen_ints(4)
    one_u      = gen_ints(1)[0]
    one_uv2    = gen_ints(2)
    one_uv3    = gen_ints(3)
    one_uv4    = gen_ints(4)
    one_mat4   = gen_floats(4 * 4)
    one_quat   = one_v4

    f_array    = array.array('f', f_list)
    v2_array   = array.array('f', v2_list)
    v3_array   = array.array('f', v3_list)
    v4_array   = array.array('f', v4_list)
    i_array    = array.array('i', i_list)
    iv2_array  = array.array('i', iv2_list)
    iv3_array  = array.array('i', iv3_list)
    iv4_array  = array.array('i', iv4_list)
    mat4_array = array.array('f', mat4_list)

    spec = []

    spec += [dict(name=f'b_{i}',    type='bool',      category='single', data=one_b)    for i in range(i_count)]
    spec += [dict(name=f'f_{i}',    type='float',     category='single', data=one_f)    for i in range(f_count)]
    spec += [dict(name=f'v2_{i}',   type='vec2',      category='single', data=one_v2)   for i in range(v2_count)]
    spec += [dict(name=f'v3_{i}',   type='vec3',      category='single', data=one_v3)   for i in range(v3_count)]
    spec += [dict(name=f'v4_{i}',   type='vec4',      category='single', data=one_v4)   for i in range(v4_count)]
    spec += [dict(name=f'i_{i}',    type='int',       category='single', data=one_i)    for i in range(i_count)]
    spec += [dict(name=f'iv2_{i}',  type='ivec2',     category='single', data=one_iv2)  for i in range(v2_count)]
    spec += [dict(name=f'iv3_{i}',  type='ivec3',     category='single', data=one_iv3)  for i in range(v3_count)]
    spec += [dict(name=f'iv4_{i}',  type='ivec4',     category='single', data=one_iv4)  for i in range(v4_count)]
    spec += [dict(name=f'u_{i}',    type='uint',      category='single', data=one_u)    for i in range(i_count)]
    spec += [dict(name=f'uiv2_{i}', type='uvec2',     category='single', data=one_uv2)  for i in range(v2_count)]
    spec += [dict(name=f'uiv3_{i}', type='uvec3',     category='single', data=one_uv3)  for i in range(v3_count)]
    spec += [dict(name=f'uiv4_{i}', type='uvec4',     category='single', data=one_uv4)  for i in range(v4_count)]
    spec += [dict(name=f'm4_{i}',   type='mat4',      category='single', data=one_mat4) for i in range(mat_count)]
    spec += [dict(name=f'qm_{i}',   type='quat_mat4', category='single', data=one_quat) for i in range(mat_count)]
    spec += [dict(name=f'qv_{i}',   type='quat_vec4', category='single', data=one_quat) for i in range(v4_count)]
    spec += [
        dict(name='t_f',   type='float',     category='array',    data=f_array,    len=f_count),
        dict(name='t_v2',  type='vec2',      category='array',    data=v2_array,   len=v2_count),
        dict(name='t_v3',  type='vec3',      category='array',    data=v3_array,   len=v3_count),
        dict(name='t_v4',  type='vec4',      category='array',    data=v4_array,   len=v4_count),
        dict(name='a_qm4', type='quat_mat4', category='animated', data=one_quat),
        dict(name='a_qv4', type='quat_vec4', category='animated', data=one_quat),
        dict(name='a_f',   type='float',     category='animated', data=None),
        dict(name='a_v2',  type='vec2',      category='animated', data=one_v2),
        dict(name='a_v3',  type='vec3',      category='animated', data=one_v3),
        dict(name='a_v4',  type='vec4',      category='animated', data=one_v4),
    ]

    if layout != 'uniform':
        spec += [
            dict(name='t_i',    type='int',        category='array',           data=i_array,    len=i_count),
            dict(name='t_iv2',  type='ivec2',      category='array',           data=iv2_array,  len=v2_count),
            dict(name='t_iv3',  type='ivec3',      category='array',           data=iv3_array,  len=v3_count),
            dict(name='t_iv4',  type='ivec4',      category='array',           data=iv4_array,  len=v4_count),
            dict(name='t_mat4', type='mat4',       category='array',           data=mat4_array, len=mat_count),
            dict(name='ab_f',   type='float',      category='animated_buffer', data=f_array,    len=f_count),
            dict(name='ab_v2',  type='vec2',       category='animated_buffer', data=v2_array,   len=v2_count),
            dict(name='ab_v3',  type='vec3',       category='animated_buffer', data=v3_array,   len=v3_count),
            dict(name='ab_v4',  type='vec4',       category='animated_buffer', data=v4_array,   len=v4_count),
        ]

    for item in spec:
        item['func'] = FUNCS['{category}_{type}'.format(**item)]

    return spec


def _get_render_with_legend(cfg, fields, area, title, block_definition, color_definition, block_fields, color_fields, layout):
    title_h = 1 / 10.

    ax, ay, aw, ah = area
    title_node = ngl.Text(
        title,
        box_corner=(ax, ay + ah - title_h, 0),
        box_width=(aw, 0, 0),
        box_height=(0, title_h, 0),
        fg_color=(0, 0, 0, 1),
        bg_color=(1, 1, 1, 1),
        aspect_ratio=cfg.aspect_ratio,
    )

    text_group = ngl.Group()
    nb_fields = len(fields)
    field_h = (ah - title_h) / float(nb_fields)
    for i, field in enumerate(fields):
        field_hpos = nb_fields - i - 1
        text_node = ngl.Text('#%02d %s' % (field['pos'], field['name']),
                             box_corner=(ax, ay + field_hpos * field_h, 0),
                             box_width=(aw / 2., 0, 0),
                             box_height=(0, field_h, 0),
                             fg_color=list(field['color']) + [1],
                             halign='left',
                             aspect_ratio=cfg.aspect_ratio)
        text_group.add_children(text_node)

    quad = ngl.Quad((ax + aw / 2., ay, 0), (aw / 2., 0, 0), (0, ah - title_h, 0))
    render = get_render(cfg, quad, fields, block_definition, color_definition, block_fields, color_fields, layout)

    return ngl.Group(children=(title_node, text_group, render))


@scene(seed=scene.Range(range=[0, 100]),
       layout=scene.List(choices=LAYOUTS),
       color_tint=scene.Bool())
def debug_block(cfg, seed=0, layout=LAYOUTS[0], color_tint=True):
    cfg.duration = ANIM_DURATION
    cfg.aspect_ratio = (1, 1)

    spec = _get_data_spec(layout)

    fields_info, block_fields, color_fields, block_definition, color_definition = get_random_block_info(spec, seed, layout, color_tint=color_tint)

    fields_single   = [f for f in fields_info if f['category'] == 'single']
    fields_array    = [f for f in fields_info if f['category'] == 'array']
    fields_animated = [f for f in fields_info if f['category'].startswith('animated')]
    field_specs = (
        (fields_single,   (-1/3., -1, 2/3., 2.), 'Single fields'),
        (fields_array,    ( 1/3.,  0, 2/3., 1.), 'Arrays'),
        (fields_animated, ( 1/3., -1, 2/3., 1.), 'Animated'),
    )

    g = ngl.Group()
    block_def_text = ngl.Text(
        f'{layout}:\n\n{block_definition}',
        valign='top',
        box_corner=(-1, -1, 0),
        box_width=(2/3., 0, 0),
        box_height=(0, 2, 0),
        aspect_ratio=cfg.aspect_ratio,
    )
    g.add_children(block_def_text)

    for cat_fields, area, title in field_specs:
        visual_fields = _get_render_with_legend(cfg, cat_fields, area, title, block_definition, color_definition, block_fields, color_fields, layout)
        g.add_children(visual_fields)
    return g


def _data_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint):
    cfg.duration = ANIM_DURATION
    cfg.aspect_ratio = (1, 1)

    fields_info, block_fields, color_fields, block_definition, color_definition = get_random_block_info(spec, seed, layout, color_tint=color_tint)
    fields = match_fields(fields_info, field_id)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = get_render(cfg, quad, fields, block_definition, color_definition, block_fields, color_fields, layout, debug_positions=debug_positions)
    return render


def _get_data_function(field_id, layout):
    nb_keyframes = 5 if 'animated' in field_id else 1
    spec = _get_data_spec(layout)

    @test_cuepoints(points=get_data_debug_positions(spec, field_id),
                    nb_keyframes=nb_keyframes,
                    tolerance=1,
                    debug_positions=False)
    @scene(seed=scene.Range(range=[0, 100]), debug_positions=scene.Bool(), color_tint=scene.Bool())
    def scene_func(cfg, seed=0, debug_positions=True, color_tint=False):
        return _data_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint)
    return scene_func


for layout in {'std140', 'std430', 'uniform'}:
    spec = _get_data_spec(layout, i_count=1, f_count=1, v2_count=1, v3_count=1, v4_count=1, mat_count=1)
    for field_info in spec:
        field_id = '{category}_{type}'.format(**field_info)
        globals()[f'data_{field_id}_{layout}'] = _get_data_function(field_id, layout)


_RENDER_STREAMEDBUFFER_VERT = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
'''


_RENDER_STREAMEDBUFFER_FRAG = '''
void main()
{
    uint x = uint(var_uvcoord.x * %(size)d.0);
    uint y = uint(var_uvcoord.y * %(size)d.0);
    uint i = clamp(x + y * %(size)dU, 0U, %(data_size)dU - 1U);
    ngl_out_color = vec4(streamed.data[i]);
}
'''


def _get_data_streamed_buffer_cuepoints(size):
    f = float(size)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return {f'{x}{y}': (c(x), c(y)) for y in range(size) for x in range(size)}


def _get_data_streamed_buffer_vec4_scene(cfg, size, nb_keyframes, scale, single, show_dbg_points):
    cfg.duration = nb_keyframes * scale
    cfg.aspect_ratio = (1, 1)
    data_size = size * size
    assert not single or size == 2

    time_anim = None
    if scale != 1:
        kfs = [
            ngl.AnimKeyFrameFloat(0, 0),
            ngl.AnimKeyFrameFloat(cfg.duration, nb_keyframes),
        ]
        time_anim = ngl.AnimatedTime(kfs)

    pts_data = array.array('q')
    assert pts_data.itemsize == 8

    for i in range(nb_keyframes):
        offset = 10000 if i == 0 else 0
        pts_data.extend([i * 1000000 + offset])

    vec4_data = array.array('f')
    for i in range(nb_keyframes):
        for j in range(data_size):
            v = i / float(nb_keyframes) + j / float(data_size*nb_keyframes)
            if single:
                vec4_data.extend([v])
            else:
                vec4_data.extend([v, v, v, v])

    pts_buffer = ngl.BufferInt64(data=pts_data)
    vec4_buffer = ngl.BufferVec4(data=vec4_data)

    if single:
        streamed_buffer = ngl.StreamedVec4(pts_buffer, vec4_buffer, time_anim=time_anim, label='data')
    else:
        streamed_buffer = ngl.StreamedBufferVec4(data_size, pts_buffer, vec4_buffer, time_anim=time_anim, label='data')
    streamed_block = ngl.Block(layout='std140', label='streamed_block', fields=(streamed_buffer,))

    shader_params = dict(data_size=data_size, size=size)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(
        vertex=_RENDER_STREAMEDBUFFER_VERT,
        fragment=_RENDER_STREAMEDBUFFER_FRAG % shader_params,
    )
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(streamed=streamed_block)

    group = ngl.Group(children=(render,))
    if show_dbg_points:
        cuepoints = _get_data_streamed_buffer_cuepoints(size)
        group.add_children(get_debug_points(cfg, cuepoints))
    return group


def _get_data_streamed_buffer_function(scale, single):
    size = 2 if single else 4
    nb_keyframes = 4
    @test_cuepoints(points=_get_data_streamed_buffer_cuepoints(size), nb_keyframes=nb_keyframes, tolerance=1)
    @scene(show_dbg_points=scene.Bool())
    def scene_func(cfg, show_dbg_points=False):
        return _get_data_streamed_buffer_vec4_scene(cfg, size, nb_keyframes, scale, single, show_dbg_points)
    return scene_func


data_streamed_vec4 = _get_data_streamed_buffer_function(1, True)
data_streamed_vec4_time_anim = _get_data_streamed_buffer_function(2, True)
data_streamed_buffer_vec4 = _get_data_streamed_buffer_function(1, False)
data_streamed_buffer_vec4_time_anim = _get_data_streamed_buffer_function(2, False)


@test_cuepoints(points={'c': (0, 0)}, tolerance=1)
@scene()
def data_integer_iovars(cfg):
    cfg.aspect_ratio = (1, 1)
    vert = '''void main() {
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_color_u32 = color_u32;
}
'''
    frag = '''void main() {
    ngl_out_color = vec4(var_color_u32) / 255.;
}'''
    program = ngl.Program(vertex=vert, fragment=frag)
    program.update_vert_out_vars(var_color_u32=ngl.IOIVec4())
    geometry = ngl.Quad(corner=(-1, -1, 0), width=(2, 0, 0), height=(0, 2, 0))
    render = ngl.Render(geometry, program)
    render.update_vert_resources(color_u32=ngl.UniformIVec4(value=(0x50, 0x80, 0xa0, 0xff)))
    return render


@test_fingerprint(nb_keyframes=10, tolerance=1)
@scene()
def data_noise_time(cfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 2
    vert = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    ngl_out_pos += vec4(t - 1., signal, 0.0, 0.0);
}
'''
    frag = '''
void main()
{
    ngl_out_color = color;
}
'''

    geometry = ngl.Circle(radius=0.25, npoints=6)
    program = ngl.Program(vertex=vert, fragment=frag)
    render = ngl.Render(geometry, program)
    render.update_vert_resources(t=ngl.Time(), signal=ngl.NoiseFloat(octaves=8))
    render.update_frag_resources(color=ngl.UniformVec4(value=COLORS['white']))
    return render


@test_fingerprint(nb_keyframes=30, tolerance=1)
@scene()
def data_noise_wiggle(cfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3
    vert = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    ngl_out_pos += vec4(wiggle, 0.0, 0.0);
}
'''
    frag = '''
void main()
{
    ngl_out_color = color;
}
'''

    geometry = ngl.Circle(radius=0.25, npoints=6)
    program = ngl.Program(vertex=vert, fragment=frag)
    render = ngl.Render(geometry, program)
    render.update_vert_resources(wiggle=ngl.NoiseVec2(octaves=8))
    render.update_frag_resources(color=ngl.UniformVec4(value=COLORS['white']))
    return render
