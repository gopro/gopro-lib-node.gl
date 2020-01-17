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

import itertools
import random
import pynodegl as ngl
from pynodegl_utils.tests.cmp_floats import test_floats


def _easing_split(easing):
    name_split = easing.split(':')
    easing_name = name_split[0]
    args = [float(x) for x in name_split[1:]] if len(name_split) > 1 else None
    return easing_name, args


def _easing_join(easing, args):
    return easing if not args else easing + ':' + ':'.join('%g' % x for x in args)


_easing_specs = (
    ('linear',    0),
    ('quadratic', 3),
    ('cubic',     3),
    ('quartic',   3),
    ('quintic',   3),
    ('power:7.3', 3),
    ('sinus',     3),
    ('exp',       3),
    ('circular',  3),
    ('bounce',    1),
    ('elastic',   1),
    ('back',      3),
)

def _get_easing_list():
    easings = []
    for col, (easing, flags) in enumerate(_easing_specs):
        versions = []
        if flags & 1:
            versions += ['_in', '_out']
        if flags & 2:
            versions += ['_in_out', '_out_in']
        if not flags:
            versions = ['']

        for version in versions:
            base_name, args = _easing_split(easing)
            easing_name = _easing_join(base_name + version, args)
            easings.append(easing_name)
    return easings

_offsets = (None, (0.0, 0.7), (0.3, 1.0), (0.3, 0.7))
_easing_list = _get_easing_list()


@test_floats()
def anim_forward_api(nb_points=7):
    scale = 1. / float(nb_points)
    ret = []
    times = [i * scale for i in range(nb_points + 1)]
    for easing in _easing_list:
        easing_name, easing_args = _easing_split(easing)
        for offsets in _offsets:
            values = [ngl.easing_evaluate(easing_name, t, easing_args, offsets) for t in times]
            ret.append([easing_name] + values)
    return ret


@test_floats()
def anim_resolution_api(nb_points=7):
    scale = 1. / float(nb_points)
    ret = []
    times = [i * scale for i in range(nb_points + 1)]
    for easing in _easing_list:
        easing_name, easing_args = _easing_split(easing)
        for offsets in _offsets:
            try:
                values = [ngl.easing_solve(easing_name, t, easing_args, offsets) for t in times]
            except Exception as e:
                pass
            else:
                ret.append([easing_name] + values)
    return ret


def _get_anim_func(size, animated_type, kf_func):

    @test_floats()
    def test_func():
        offsets = ((None, None), (None, 0.7), (0.3, None), (0.3, 0.7))
        nb_kf = len(_easing_specs) + 1
        nb_queries = nb_kf - 1
        scale = 1. / float(nb_kf)
        random.seed(0)
        kfvalues = [[random.uniform(0, 1) for r in range(size)] for i in range(nb_kf + 1)]

        ret = []
        for i, (easing_start_offset, easing_end_offset) in enumerate(offsets):
            anim_kf = [kf_func(0, kfvalues[0])]
            for j in range(nb_kf):
                t = (j + 1) * scale
                v = kfvalues[j + 1]
                easing_name, easing_args = _easing_split(_easing_list[j])
                anim_kf.append(kf_func(t, v,
                                       easing=easing_name,
                                       easing_args=easing_args,
                                       easing_start_offset=easing_start_offset,
                                       easing_end_offset=easing_end_offset))
            anim = animated_type(anim_kf)

            # Query between times
            values = [anim.evaluate((t_id + 1) * scale) for t_id in range(nb_queries)]

            # Query boundaries and out of them (to trigger a copy instead of a mix)
            values += [anim.evaluate(0)]
            values += [anim.evaluate(1)]
            values += [anim.evaluate(5)]

            if hasattr(values[0], '__iter__'):
                values = list(itertools.chain(*values))
            ret.append(['off%d' % i] + values)

        return ret

    return test_func


_float_kf_func = lambda t, v, **kw: ngl.AnimKeyFrameFloat(t, v[0], **kw)
_vec2_kf_func  = lambda t, v, **kw: ngl.AnimKeyFrameVec2(t, v, **kw)
_vec3_kf_func  = lambda t, v, **kw: ngl.AnimKeyFrameVec3(t, v, **kw)
_vec4_kf_func  = lambda t, v, **kw: ngl.AnimKeyFrameVec4(t, v, **kw)

anim_forward_float = _get_anim_func(1, ngl.AnimatedFloat, _float_kf_func)
anim_forward_vec2  = _get_anim_func(2, ngl.AnimatedVec2,  _vec2_kf_func)
anim_forward_vec3  = _get_anim_func(3, ngl.AnimatedVec3,  _vec3_kf_func)
anim_forward_vec4  = _get_anim_func(4, ngl.AnimatedVec4,  _vec4_kf_func)
