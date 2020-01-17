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

import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints

from pynodegl_utils.tests.data import (
    LAYOUTS,
    ANIM_DURATION,
    FUNCS,
    gen_floats,
    gen_ints,
    get_data_debug_positions,
    match_fields,
    get_random_block_info,
    get_render,
)


def _get_live_spec(layout):
    livechange_f      = [[v] for v in gen_floats(4)[1:3]]
    livechange_v2     = gen_floats(2), gen_floats(2)[::-1]
    livechange_v3     = gen_floats(3), gen_floats(3)[::-1]
    livechange_v4     = gen_floats(4), gen_floats(4)[::-1]
    livechange_i      = [[v] for v in gen_ints(4)[1:3]]
    livechange_mat4   = gen_floats(4 * 4), gen_floats(4 * 4)[::-1]
    livechange_quat   = livechange_v4

    spec = [
        dict(name='f',  type='float',     category='single', livechange=livechange_f),
        dict(name='v2', type='vec2',      category='single', livechange=livechange_v2),
        dict(name='v3', type='vec3',      category='single', livechange=livechange_v3),
        dict(name='v4', type='vec4',      category='single', livechange=livechange_v4),
        dict(name='i',  type='int',       category='single', livechange=livechange_i),
        dict(name='m4', type='mat4',      category='single', livechange=livechange_mat4),
        dict(name='qm', type='quat_mat4', category='single', livechange=livechange_quat),
        dict(name='qv', type='quat_vec4', category='single', livechange=livechange_quat),
    ]

    for item in spec:
        item['func'] = FUNCS['{category}_{type}'.format(**item)]

    return spec


def _get_live_trf_spec(layout):

    t0 = ngl.Identity()
    t1 = ngl.Transform(t0)
    t2 = ngl.Translate(t1)
    t3 = ngl.Rotate(t2)
    t4 = ngl.Scale(t3)
    t5 = ngl.RotateQuat(t4)

    return [
        dict(name='m4',
             type='mat4',
             category='single',
             func=lambda data: ngl.UniformMat4(data, transform=t5),
             livechange=(
                lambda: t1.set_matrix(                      # trf_step=1
                    0.1, 0.2, 0.0, 0.0,
                    0.0, 0.3, 0.4, 0.0,
                    0.0, 0.0, 0.5, 0.0,
                    0.8, 0.7, 0.0, 0.6,
                ),
                lambda: t2.set_vector(0.2, 0.7, -0.4),      # trf_step=2
                lambda: t3.set_angle(123.4),                # trf_step=3
                lambda: t4.set_factors(0.7, 1.4, 0.2),      # trf_step=4
                lambda: t5.set_quat(0, 0, -0.474, 0.880),   # trf_step=5
                lambda: t5.set_quat(0, 0, 0, 0),
                lambda: t4.set_factors(1, 1, 1),
                lambda: t3.set_angle(0),
                lambda: t2.set_vector(0, 0, 0),
                lambda: t1.set_matrix(
                    1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0,
                )
            ),
        ),
    ]


def _live_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint):

    # duration set to 0 makes it always draw the same time, and we want that
    # FIXME: can not work with blocks because their update is not re-called
    # FIXME: can not work with transform chains because the chain transform is not re-updated
    cfg.duration = 0  #if layout == 'uniform' else ANIM_DURATION
    cfg.duration = ANIM_DURATION  # FIXME: drop

    cfg.aspect_ratio = (1, 1)
    fields_info, block_fields, color_fields, block_definition, color_definition = get_random_block_info(spec, seed, layout, color_tint=color_tint)
    fields = match_fields(fields_info, field_id)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = get_render(cfg, quad, fields, block_definition, color_definition, block_fields, color_fields, layout, debug_positions=debug_positions)
    return render


def _get_live_function(spec, field_id, layout):

    fields = match_fields(spec, field_id)
    assert len(fields) == 1
    field = fields[0]
    data_src = field['livechange']

    def keyframes_callback(t_id):
        if t_id:
            v = data_src[t_id - 1]
            field['node'].set_value(*v)

    @test_cuepoints(points=get_data_debug_positions(spec, field_id),
                    nb_keyframes=len(data_src) + 1,
                    keyframes_callback=keyframes_callback,
                    tolerance=1,
                    exercise_serialization=False,
                    debug_positions=False)
    @scene(seed=scene.Range(range=[0, 100]), debug_positions=scene.Bool(), color_tint=scene.Bool())
    def scene_func(cfg, seed=0, debug_positions=True, color_tint=False):
        return _live_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint)
    return scene_func


def _get_live_trf_function(spec, field_id, layout):

    fields = match_fields(spec, field_id)
    assert len(fields) == 1
    field = fields[0]
    livechange_funcs = field['livechange']

    def keyframes_callback(t_id):
        livechange_funcs[t_id]()

    @test_cuepoints(points=get_data_debug_positions(spec, field_id),
                    nb_keyframes=len(livechange_funcs),
                    keyframes_callback=keyframes_callback,
                    tolerance=1,
                    exercise_serialization=False,
                    debug_positions=False)
    @scene(seed=scene.Range(range=[0, 100]),
           debug_positions=scene.Bool(),
           color_tint=scene.Bool(),
           trf_step=scene.Range(range=[0, len(livechange_funcs)]))
    def scene_func(cfg, seed=0, debug_positions=True, color_tint=False, trf_step=0):
        s = _live_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint)
        for i in range(trf_step):
            keyframes_callback(i)
        return s
    return scene_func


def _bootstrap():
    for layout in LAYOUTS:
        spec = _get_live_spec(layout)
        for field_info in spec:
            field_id = '{category}_{type}'.format(**field_info)
            globals()['live_{}_{}'.format(field_id, layout)] = _get_live_function(spec, field_id, layout)

        spec = _get_live_trf_spec(layout)
        for field_info in spec:
            field_id = '{category}_{type}'.format(**field_info)
            globals()['live_trf_{}_{}'.format(field_id, layout)] = _get_live_trf_function(spec, field_id, layout)


_bootstrap()
