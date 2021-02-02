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
    return float(ret[0]), float(ret[1])


def blend(*nodes):
    group = ngl.Group(children=nodes)
    return ngl.GraphicConfig(group, blend=True,
                             blend_src_factor='src_alpha',
                             blend_dst_factor='one_minus_src_alpha',
                             blend_src_factor_a='zero',
                             blend_dst_factor_a='one')


def overlay(*nodes):
    group = ngl.Group(children=nodes)
    return ngl.GraphicConfig(group, blend=True,
                             blend_src_factor='src_alpha',
                             blend_dst_factor='one_minus_src_alpha',
                             blend_src_factor_a='src_alpha',
                             blend_dst_factor_a='one_minus_src_alpha')


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


def gradient_quad(cfg, bl, br, tl, tr):
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
        quad_vertices.extend([x, y, 0])

    n = nb_colors
    indices = array.array('H')
    for i in range(nb_colors - 1):
        idx = [i, i + 1, i + n,
               i + n, i + n + 1, i + 1]
        indices.extend(idx)

    q = ngl.Geometry(vertices=ngl.BufferVec3(data=quad_vertices), indices=ngl.BufferUShort(data=indices))
    p = ngl.Program(vertex=cfg.get_vert('gradient'), fragment=cfg.get_frag('color'))
    p.update_vert_out_vars(color=ngl.IOVec4())
    r = ngl.Render(q, p)
    r.update_attributes(vertex_color=ngl.BufferVec4(data=colors_data))
    return r


def proxy(cfg, scene, box=(320, 320)):
    proxy_w, proxy_h = fit(box, (1, 1), cfg.aspect_ratio)
    proxy_texture = ngl.Texture2D(width=proxy_w, height=proxy_h, mag_filter='linear', min_filter='linear', label='proxy texture')
    proxy_texture_write = ngl.RenderToTexture(scene, label='proxy')
    proxy_texture_write.add_color_textures(proxy_texture)
    return proxy_w, proxy_h, proxy_texture_write, proxy_texture


_RENDER_WITH_MASK_VERTEX = '''void main() {
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
    var_tex1_coord = (tex1_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
}'''


_RENDER_WITH_MASK_POST_MULT_FRAGMENT = '''void main() {
    highp vec4 color = ngl_texvideo(tex0, var_tex0_coord);
    highp vec4 mask = ngl_texvideo(tex1, var_tex1_coord);
    highp float alpha = inverse ? (1.0 - mask.a) * color.a : color.a * mask.a;
    ngl_out_color = vec4(color.rgb / color.a, alpha);
}'''


def mask(cfg, scene, mask, inverse=False):
    mask_config = ngl.GraphicConfig(
        mask,
        blend=False,  # XXX
        blend_src_factor='one',
        blend_dst_factor='zero',
        blend_src_factor_a='one',
        blend_dst_factor_a='zero',
    )

    rtt_w, rtt_h = fit((4096, 4096), (1, 1), cfg.aspect_ratio)
    rtt_w, rtt_h = int(rtt_w), int(rtt_h)

    mask_texture = ngl.Texture2D(width=rtt_w, height=rtt_h, min_filter='linear', mag_filter='linear', mipmap_filter='linear', label='mask')
    mask_rtt = ngl.RenderToTexture(mask_config, color_textures=(mask_texture,))

    scene_texture = ngl.Texture2D(width=rtt_w, height=rtt_h, min_filter='linear', mag_filter='linear', mipmap_filter='linear', label='scene')
    scene_rtt = ngl.RenderToTexture(scene, color_textures=(scene_texture,))

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_RENDER_WITH_MASK_VERTEX, fragment=_RENDER_WITH_MASK_POST_MULT_FRAGMENT)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2(), var_tex1_coord=ngl.IOVec2())
    render = ngl.Render(quad, program, label='render with mask')
    render.update_frag_resources(tex0=scene_texture)
    render.update_frag_resources(tex1=mask_texture)
    render.update_frag_resources(inverse=ngl.UniformBool(value=inverse))

    config = ngl.GraphicConfig(
        render,
        blend=True,
        # Post mult
        blend_src_factor='src_alpha',
        blend_dst_factor='one_minus_src_alpha',
        blend_src_factor_a='one',
        blend_dst_factor_a='one_minus_src_alpha',
    )
    camera = ngl.Camera(config)
    return ngl.Group(children=(scene_rtt, mask_rtt, camera))


def mask_with_geometry(cfg, scene, geometry, inverse=False):
    program = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    render = ngl.Render(geometry, program)
    render.update_frag_resources(color=ngl.UniformVec4(value=(0, 0, 0, 1)))
    return mask(cfg, scene, render, inverse)
