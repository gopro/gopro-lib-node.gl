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
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints

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

    spec += [dict(name='f_%d'  % i, type='float',     category='single', data=one_f)    for i in range(f_count)]
    spec += [dict(name='v2_%d' % i, type='vec2',      category='single', data=one_v2)   for i in range(v2_count)]
    spec += [dict(name='v3_%d' % i, type='vec3',      category='single', data=one_v3)   for i in range(v3_count)]
    spec += [dict(name='v4_%d' % i, type='vec4',      category='single', data=one_v4)   for i in range(v4_count)]
    spec += [dict(name='i_%d'  % i, type='int',       category='single', data=one_i)    for i in range(i_count)]
    spec += [dict(name='m4_%d' % i, type='mat4',      category='single', data=one_mat4) for i in range(mat_count)]
    spec += [dict(name='qm_%d' % i, type='quat_mat4', category='single', data=one_quat) for i in range(mat_count)]
    spec += [dict(name='qv_%d' % i, type='quat_vec4', category='single', data=one_quat) for i in range(v4_count)]
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

    fields_single   = filter(lambda f: f['category'] == 'single', fields_info)
    fields_array    = filter(lambda f: f['category'] == 'array', fields_info)
    fields_animated = filter(lambda f: f['category'].startswith('animated'), fields_info)
    field_specs = (
        (fields_single,   (-1/3., -1, 2/3., 2.), 'Single fields'),
        (fields_array,    ( 1/3.,  0, 2/3., 1.), 'Arrays'),
        (fields_animated, ( 1/3., -1, 2/3., 1.), 'Animated'),
    )

    g = ngl.Group()
    block_def_text = ngl.Text(
        '{}:\n\n{}'.format(layout, block_definition),
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
        globals()['data_{}_{}'.format(field_id, layout)] = _get_data_function(field_id, layout)
