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


def fit(dst_zone, disp_ar, src_ar):
    '''
    find the largest box of display aspect ratio `src_ar` that can fit into
    `dst_zone` with a global display AR `disp_ar`
    '''
    zw, zh = dst_zone
    ar = (src_ar[0] * disp_ar[1]) / (src_ar[1] * disp_ar[0])
    ret = (zh * ar, zh) if zw / zh > ar else (zw, zw / ar)
    return int(ret[0]), int(ret[1])


def blend(*nodes):
    group = ngl.Group(children=nodes)
    return ngl.GraphicConfig(group, blend=True,
                             blend_src_factor='src_alpha',
                             blend_dst_factor='one_minus_src_alpha',
                             blend_src_factor_a='zero',
                             blend_dst_factor_a='one')


def canvas(cfg, media, frag=None, **frag_args):
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(media.filename)
    t = ngl.Texture2D(data_src=m, mag_filter='linear', min_filter='linear')
    if frag is None:
        frag = cfg.get_frag('texture')
    p = ngl.Program(vertex=cfg.get_vert('texture'), fragment=frag)
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    r = ngl.Render(q, p)
    r.update_frag_resources(tex0=t, **frag_args)
    return r, t


def gradient(cfg, bl, br, tl, tr):
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=cfg.get_vert('gradient'), fragment=cfg.get_frag('color'))
    p.update_vert_out_vars(color=ngl.IOVec4())
    r = ngl.Render(q, p)
    r.update_attributes(vertex_color=ngl.BufferVec4(data=array.array('f', bl + br + tl + tr)))
    return r


def gradient_line(cfg, *colors):
    colors_data = array.array('f')
    for color in colors:  # top
        colors_data.extend(color)
    for color in colors:  # bottom
        colors_data.extend(color)

    quad_vertices = array.array('f')
    nb_colors = len(colors)
    for i in range(nb_colors * 2):
        x = (i % nb_colors) / (nb_colors - 1) * 2 - 1
        y = 1 if i < nb_colors else -1
        #print(f'{i:2d}: {x}, {y}')
        quad_vertices.extend([x, y, 0])

    n = nb_colors
    indices = array.array('H')
    for i in range(nb_colors - 1):
        idx = [i, i + 1, i + n,
               i + n, i + n + 1, i + 1]
        #print(idx)
        indices.extend(idx)

    q = ngl.Geometry(vertices=ngl.BufferVec3(data=quad_vertices), indices=ngl.BufferUShort(data=indices))
    p = ngl.Program(vertex=cfg.get_vert('gradient'), fragment=cfg.get_frag('color'))
    p.update_vert_out_vars(color=ngl.IOVec4())
    r = ngl.Render(q, p)
    r.update_attributes(vertex_color=ngl.BufferVec4(data=colors_data))
    return r


def proxy(cfg, scene, box=(320, 320)):
    proxy_w, proxy_h = fit(box, (1, 1), cfg.aspect_ratio)
    proxy_texture = ngl.Texture2D(width=proxy_w, height=proxy_h, label='proxy texture')
    proxy_texture_write = ngl.RenderToTexture(scene, label='proxy')
    proxy_texture_write.add_color_textures(proxy_texture)
    return proxy_w, proxy_h, proxy_texture_write, proxy_texture
