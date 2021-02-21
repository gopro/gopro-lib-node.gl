#
# Copyright 2021 GoPro Inc.
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
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.toolbox.shapes import equilateral_triangle_coords


@test_fingerprint(width=320, height=320, nb_keyframes=20, tolerance=1)
@scene()
def velocity_triangle_rotate(cfg):
    cfg.duration = 5.
    cfg.aspect_ratio = (1, 1)

    anim_kf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 360*3, 'circular_in_out'),
    ]
    anim = ngl.AnimatedFloat(anim_kf)
    velocity = ngl.VelocityFloat(anim)

    frag = '''
void main()
{
    float v = clamp(velocity / 3000., 0.0, 1.0);
    ngl_out_color = vec4(v, v / 2.0, 0.0, 1.0);
}
'''

    p0, p1, p2 = equilateral_triangle_coords(2.0)
    prog_t = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    triangle = ngl.Render(ngl.Triangle(p0, p1, p2), prog_t)
    triangle.update_frag_resources(color=ngl.UniformVec4(COLORS.white))
    triangle = ngl.Rotate(triangle, anim=anim)

    prog_c = ngl.Program(vertex=cfg.get_vert('color'), fragment=frag)
    circle = ngl.Render(ngl.Circle(radius=1.0, npoints=128), prog_c)
    circle.update_frag_resources(velocity=velocity)
    return ngl.Group(children=(circle, triangle))


@test_fingerprint(width=320, height=320, nb_keyframes=20, tolerance=1)
@scene()
def velocity_circle_distort_2d(cfg):
    cfg.duration = 4.
    cfg.aspect_ratio = (1, 1)

    coords = list(equilateral_triangle_coords())
    coords.append(coords[0])

    pos_kf = [ngl.AnimKeyFrameVec2(cfg.duration * i/3., pos[0:2], 'exp_in_out') for i, pos in enumerate(coords)]
    anim = ngl.AnimatedVec2(pos_kf)
    velocity = ngl.VelocityVec2(anim)

    vert = '''
void main()
{
    float distort_max = 0.1;
    float velocity_l = length(velocity);
    float direction_l = length(ngl_position);
    vec2 normed_velocity = velocity_l == 0.0 ? vec2(0.0) : -velocity / velocity_l;
    vec2 normed_direction = direction_l == 0.0 ? vec2(0.0) : ngl_position.xy / direction_l;
    float distort = clamp(dot(normed_velocity, normed_direction) / 2.0 * distort_max, 0.0, 1.0);
    vec4 pos = vec4(ngl_position, 1.0) + vec4(translate, 0.0, 0.0) + vec4(-distort * velocity, 0.0, 0.0);
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * pos;
}
'''

    geom = ngl.Circle(radius=0.2, npoints=128)
    prog = ngl.Program(vertex=vert, fragment=cfg.get_frag('color'))
    shape = ngl.Render(geom, prog)
    shape.update_frag_resources(color=ngl.UniformVec4(COLORS.white))
    shape.update_vert_resources(velocity=velocity, translate=anim)
    return shape


@test_fingerprint(width=320, height=320, nb_keyframes=20, tolerance=1)
@scene()
def velocity_circle_distort_3d(cfg):
    cfg.duration = 4.
    cfg.aspect_ratio = (1, 1)

    coords = list(equilateral_triangle_coords())
    coords.append(coords[0])
    pos_kf = [ngl.AnimKeyFrameVec3(cfg.duration * i/3., pos, 'exp_in_out') for i, pos in enumerate(coords)]
    anim = ngl.AnimatedVec3(pos_kf)
    velocity = ngl.VelocityVec3(anim)

    vert = '''
void main()
{
    float distort_max = 0.1;
    float velocity_l = length(velocity);
    float direction_l = length(ngl_position);
    vec3 normed_velocity = velocity_l == 0.0 ? vec3(0.0) : -velocity / velocity_l;
    vec3 normed_direction = direction_l == 0.0 ? vec3(0.0) : ngl_position / direction_l;
    float distort = clamp(dot(normed_velocity, normed_direction) / 2.0 * distort_max, 0.0, 1.0);
    vec4 pos = vec4(ngl_position, 1.0) + vec4(-distort * velocity, 0.0);
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * pos;
}
'''

    geom = ngl.Circle(radius=0.2, npoints=128)
    prog = ngl.Program(vertex=vert, fragment=cfg.get_frag('color'))
    shape = ngl.Render(geom, prog)
    shape.update_frag_resources(color=ngl.UniformVec4(COLORS.white))
    shape.update_vert_resources(velocity=velocity)
    return ngl.Translate(shape, anim=anim)
