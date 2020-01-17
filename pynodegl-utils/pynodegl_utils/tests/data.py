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

import colorsys
import random
import pynodegl as ngl
from pynodegl_utils.tests.debug import get_debug_points


_FIELDS_VERT = '''
in vec4 ngl_position;
in vec2 ngl_uvcoord;

uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
uniform mat3 ngl_normal_matrix;

out vec2 var_uvcoord;

void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_uvcoord;
}
'''

_FIELDS_FRAG = '''
precision highp float;

in vec2 var_uvcoord;
out vec4 frag_color;

uniform int nb_fields;

%(block_definition)s
%(color_definition)s

float in_rect(vec4 rect, vec2 pos)
{
    return (1.0 - step(pos.x, rect.x)) * step(pos.x, rect.x + rect.z) *
           (1.0 - step(pos.y, rect.y)) * step(pos.y, rect.y + rect.w);
}

%(func_definitions)s

void main()
{
    float w = 1.0;
    float h = 1.0 / float(nb_fields);
    vec3 res = %(func_calls)s;
    frag_color = vec4(res, 1.0);
}
'''

_ARRAY_TPL = '''
vec3 get_color_%(field_name)s(float w, float h, float x, float y)
{
    float amount = 0.0;
    int len = %(fields_prefix)s%(field_name)s.length();
    for (int i = 0; i < len; i++) {
        %(amount_code)s
    }
    return %(colors_prefix)s%(field_name)s * amount;
}
'''

_SINGLE_TPL = '''
vec3 get_color_%(field_name)s(float w, float h, float x, float y)
{
    float amount = 0.0;
    %(amount_code)s
    return %(colors_prefix)s%(field_name)s * amount;
}
'''

_COMMON_INT_TPL = 'amount += float(%(fields_prefix)s%(field_name)s%(vec_field)s) / 255. * in_rect(rect_%(comp_id)d, var_uvcoord);'
_COMMON_FLT_TPL = 'amount += %(fields_prefix)s%(field_name)s%(vec_field)s * in_rect(rect_%(comp_id)d, var_uvcoord);'

_RECT_ARRAY_TPL = 'vec4 rect_%(comp_id)d = vec4(x + %(row)f * w / (%(nb_rows)f * float(len)) + float(i) * w / float(len), y + %(col)f * h / %(nb_cols)f, w / %(nb_rows)f / float(len), h / %(nb_cols)f);'
_RECT_SINGLE_TPL = 'vec4 rect_%(comp_id)d = vec4(x + %(col)f * w / %(nb_cols)f, y + %(row)f * h / %(nb_rows)f, w / %(nb_cols)f, h / %(nb_rows)f);'

ANIM_DURATION = 5.0
LAYOUTS = ('std140', 'std430', 'uniform')

# row, col, is_int
_TYPE_SPEC = dict(
    float=    (1, 1, False),
    vec2=     (1, 2, False),
    vec3=     (1, 3, False),
    vec4=     (1, 4, False),
    mat4=     (4, 4, False),
    int=      (1, 1, True),
    ivec2=    (1, 2, True),
    ivec3=    (1, 3, True),
    ivec4=    (1, 4, True),
    quat_mat4=(4, 4, False),
    quat_vec4=(1, 4, False),
)


def _get_glsl_fields_definition(binding, layout, name, definition):
    data_tpl = dict(name=name, definition=definition, binding=binding)
    if layout == 'std430':
        tpl = 'layout(std430, binding=%(binding)d) buffer %(name)s_block {\n%(definition)s\n} %(name)s;'
    elif layout == 'std140':
        tpl = 'layout(std140) uniform %(name)s_block {\n%(definition)s\n} %(name)s;'
    else:
        tpl = '%(definition)s'
    return tpl % data_tpl


def _get_display_glsl_func(layout, field_name, field_type, is_array=False):
    rows, cols, is_int = _TYPE_SPEC[field_type]
    nb_comp = rows * cols

    tpl = _ARRAY_TPL if is_array else _SINGLE_TPL
    rect_tpl = _RECT_ARRAY_TPL if is_array else _RECT_SINGLE_TPL
    amount_tpl = _COMMON_INT_TPL if is_int else _COMMON_FLT_TPL

    tpl_data = dict(
        colors_prefix='color_' if layout == 'uniform' else 'colors.',
        fields_prefix='field_' if layout == 'uniform' else 'fields.',
        field_name=field_name,
        nb_comp=nb_comp,
    )

    rect_lines = []
    amount_lines = []
    for row in range(rows):
        for col in range(cols):
            comp_id = row * cols + col

            tpl_data['col'] = col
            tpl_data['row'] = row
            tpl_data['nb_cols'] = cols
            tpl_data['nb_rows'] = rows

            if nb_comp == 16:
                tpl_data['vec_field'] = '[%d][%d]' % (col, row)
            elif nb_comp >= 1 and nb_comp <= 4:
                tpl_data['vec_field'] = '.' + "xyzw"[comp_id] if nb_comp != 1 else ''
            else:
                assert False

            if is_array:
                tpl_data['vec_field'] = '[i]' + tpl_data['vec_field']

            tpl_data['comp_id'] = comp_id
            rect_lines.append(rect_tpl % tpl_data)
            amount_lines.append(amount_tpl % tpl_data)

    tpl_data['amount_code'] = ('\n' + (1 + is_array) * 4 * ' ').join(rect_lines + amount_lines)

    return tpl % tpl_data


def _get_debug_point(rect):
    xpos = rect[0] + rect[2]/2.
    ypos = rect[1] + rect[3]/2.
    xpos = xpos * 2. - 1
    ypos = ypos * 2. - 1
    return xpos, ypos


def _get_debug_positions_from_fields(fields):
    debug_points = {}
    for i, field in enumerate(fields):
        array_len = field.get('len')
        nb_rows, nb_cols, _ = _TYPE_SPEC[field['type']]

        name = field['name']
        comp_id = 0

        w = 1.0
        h = 1.0 / float(len(fields))
        x = 0.0
        y = float(i) * h

        if array_len is None:
            for row in range(nb_rows):
                for col in range(nb_cols):
                    rect = (x + col * w / float(nb_cols),
                            y + row * h / float(nb_rows),
                            w / float(nb_cols),
                            h / float(nb_rows))
                    debug_points['{}_{}'.format(name, comp_id)] = _get_debug_point(rect)
                    comp_id += 1

        else:
            for i in range(array_len):
                for row in range(nb_rows):
                    for col in range(nb_cols):
                        rect = (x + row * w / (nb_rows * array_len) + i * w / float(array_len),
                                y + col * h / float(nb_cols),
                                w / float(nb_rows) / float(array_len),
                                h / float(nb_cols))
                        debug_points['{}_{}'.format(name, comp_id)] = _get_debug_point(rect)
                        comp_id += 1

    return debug_points


def get_data_debug_positions(spec, field_id):
    fields_info, block_fields, color_fields, block_definition, color_definition = get_random_block_info(spec) #seed, layout)
    fields = match_fields(spec, field_id)
    return _get_debug_positions_from_fields(fields)


def get_render(cfg, quad, fields, block_definition, color_definition, block_fields, color_fields, layout, debug_positions=False):

    func_calls = []
    func_definitions = []
    for i, field in enumerate(fields):
        is_array = 'len' in field
        func_calls.append('get_color_%s(w, h, 0.0, %f * h)' % (field['name'], i))
        func_definitions.append(_get_display_glsl_func(layout, field['name'], field['type'], is_array=is_array))

    frag_data = dict(
        block_definition=_get_glsl_fields_definition(1, layout, 'fields', block_definition),
        color_definition=_get_glsl_fields_definition(2, layout, 'colors', color_definition),
        layout=layout,
        func_definitions='\n'.join(func_definitions),
        func_calls=' + '.join(func_calls),
    )

    if layout == 'std430':
        shader_version = '310 es' if cfg.backend == 'gles' else '430'
    else:
        shader_version = '300 es' if cfg.backend == 'gles' else '330'

    header = '#version %s\n' % shader_version

    fragment = header + _FIELDS_FRAG % frag_data

    program = ngl.Program(vertex=header + _FIELDS_VERT, fragment=fragment)
    render = ngl.Render(quad, program)

    if isinstance(color_fields, dict):
        assert isinstance(block_fields, dict)
        field_names = set(f['name'] for f in fields)
        d = {}
        d.update(('color_' + n, u) for (n, u) in color_fields.items() if n in field_names)
        d.update(('field_' + n, u) for (n, u) in block_fields.items() if n in field_names)
        render.update_uniforms(**d)
    else:
        render.update_blocks(fields_block=block_fields, colors_block=color_fields)

    render.update_uniforms(nb_fields=ngl.UniformInt(len(fields)))

    if debug_positions:
        debug_points = _get_debug_positions_from_fields(fields)
        dbg_circles = get_debug_points(cfg, debug_points, text_size=(.2, .1))
        g = ngl.Group(children=(render, dbg_circles))
        return g

    return render


def match_fields(fields, target_field_id):
    ret = []
    for field_info in fields:
        field_id = '{category}_{type}'.format(**field_info)
        if field_id == target_field_id:
            ret.append(field_info)
    return ret


def gen_floats(n):
    return [(i + .5) / float(n) for i in range(n)]


def gen_ints(n):
    return [int(i * 256) for i in gen_floats(n)]


def _get_anim_kf(key_cls, data):
    t0, t1, t2 = 0, ANIM_DURATION / 2., ANIM_DURATION
    if data is None:
        _, v1, v2, _ = gen_floats(4)
        return [key_cls(t0, v1), key_cls(t1, v2), key_cls(t2, v1)]
    return [
        key_cls(t0, data),
        key_cls(t1, data[::-1]),
        key_cls(t2, data),
    ]


FUNCS = dict(
    animated_float=       lambda data: ngl.AnimatedFloat(keyframes=_get_anim_kf(ngl.AnimKeyFrameFloat, data)),
    animated_vec2=        lambda data: ngl.AnimatedVec2(keyframes=_get_anim_kf(ngl.AnimKeyFrameVec2, data)),
    animated_vec3=        lambda data: ngl.AnimatedVec3(keyframes=_get_anim_kf(ngl.AnimKeyFrameVec3, data)),
    animated_vec4=        lambda data: ngl.AnimatedVec4(keyframes=_get_anim_kf(ngl.AnimKeyFrameVec4, data)),
    animated_buffer_float=lambda data: ngl.AnimatedBufferFloat(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_buffer_vec2= lambda data: ngl.AnimatedBufferVec2(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_buffer_vec3= lambda data: ngl.AnimatedBufferVec3(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_buffer_vec4= lambda data: ngl.AnimatedBufferVec4(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_quat_mat4=   lambda data: ngl.AnimatedQuat(keyframes=_get_anim_kf(ngl.AnimKeyFrameQuat, data), as_mat4=True),
    animated_quat_vec4=   lambda data: ngl.AnimatedQuat(keyframes=_get_anim_kf(ngl.AnimKeyFrameQuat, data), as_mat4=False),
    array_float=          lambda data: ngl.BufferFloat(data=data),
    array_int=            lambda data: ngl.BufferInt(data=data),
    array_ivec2=          lambda data: ngl.BufferIVec2(data=data),
    array_ivec3=          lambda data: ngl.BufferIVec3(data=data),
    array_ivec4=          lambda data: ngl.BufferIVec4(data=data),
    array_mat4=           lambda data: ngl.BufferMat4(data=data),
    array_vec2=           lambda data: ngl.BufferVec2(data=data),
    array_vec3=           lambda data: ngl.BufferVec3(data=data),
    array_vec4=           lambda data: ngl.BufferVec4(data=data),
    single_float=         lambda data: ngl.UniformFloat(data),
    single_int=           lambda data: ngl.UniformInt(data),
    single_mat4=          lambda data: ngl.UniformMat4(data),
    single_quat_mat4=     lambda data: ngl.UniformQuat(data, as_mat4=True),
    single_quat_vec4=     lambda data: ngl.UniformQuat(data, as_mat4=False),
    single_vec2=          lambda data: ngl.UniformVec2(data),
    single_vec3=          lambda data: ngl.UniformVec3(data),
    single_vec4=          lambda data: ngl.UniformVec4(data),
)


def _get_field_decl(layout, f):
    t = f['type']
    t = t.split('_')[1] if t.startswith('quat') else t
    return '%s%-5s %s%s%s' % (
        'uniform ' if layout == 'uniform' else '',
        t,
        'field_' if layout == 'uniform' else '',
        f['name'],
        '[%d]' % f['len'] if 'len' in f else ''
    )


def get_random_block_info(spec, seed=0, layout=LAYOUTS[0], color_tint=True):

    # Seed only defines the random for the position of the fields
    random.seed(seed)
    fields_pos = range(len(spec))
    random.shuffle(fields_pos)

    # Always the same colors whatever the user seed
    random.seed(0)

    fields_info = []
    max_id_len = 0
    for i, field_info in enumerate(spec):
        node = field_info['func'](field_info.get('data'))
        node.set_label(field_info['name'])
        field_info['node'] = node
        field_info['decl'] = _get_field_decl(layout, field_info)
        field_info['pos'] = fields_pos.index(i)
        max_id_len = max(len(field_info['decl']), max_id_len)
        if color_tint:
            hue = random.uniform(0, 1)
            field_info['color'] = colorsys.hls_to_rgb(hue, 0.6, 1.0)
        else:
            field_info['color'] = (1, 1, 1)
        fields_info.append(field_info)

    shuf_fields = [fields_info[pos] for pos in fields_pos]
    color_fields = [(f['name'], ngl.UniformVec3(f['color'])) for f in fields_info]
    block_fields = [(f['name'], f['node']) for f in shuf_fields]
    if layout == 'uniform':
        color_fields = dict(color_fields)
        block_fields = dict(block_fields)
    else:
        color_fields = ngl.Block(fields=[f for n, f in color_fields], layout=layout, label='colors_block')
        block_fields = ngl.Block(fields=[f for n, f in block_fields], layout=layout, label='fields_block')

    pad = lambda s: (max_id_len - len(s)) * ' '
    block_definition = '\n'.join('%s;%s // #%02d' % (f['decl'], pad(f['decl']), i) for i, f in enumerate(shuf_fields))

    color_prefix = 'color_' if layout == 'uniform' else ''
    color_u = 'uniform ' if layout == 'uniform' else ''
    color_definition = '\n'.join(color_u + 'vec3 %s;' % (color_prefix + f['name']) for f in fields_info)

    return fields_info, block_fields, color_fields, block_definition, color_definition
