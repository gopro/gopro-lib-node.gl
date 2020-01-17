import array
import colorsys
import math
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.grid import AutoGrid


def _block(w, h, program, corner=None, **uniforms):
    block_width = (w, 0, 0)
    block_height = (0, h, 0)
    block_corner = (-w / 2., -h / 2., 0) if corner is None else corner
    block_quad = ngl.Quad(corner=block_corner, width=block_width, height=block_height)
    block_render = ngl.Render(block_quad, program)
    block_render.update_uniforms(**uniforms)
    return block_render


def _easing_split(easing):
    name_split = easing.split(':')
    easing_name = name_split[0]
    args = [float(x) for x in name_split[1:]] if len(name_split) > 1 else None
    return easing_name, args


def _easing_join(easing, args):
    return easing if not args else easing + ':' + ':'.join('%g' % x for x in args)


def _get_easing_node(cfg, easing, curve_zoom, color_program, nb_points=128):
    text_vratio = 1 / 8.
    graph_hpad_ratio = 1 / 16.

    area_size = 2.0
    width, height = area_size, area_size
    text_height = text_vratio * height
    pad_height = graph_hpad_ratio * height

    # Colors
    hue = random.uniform(0, 0.6)
    color = list(colorsys.hls_to_rgb(hue, 0.6, 1.0)) + [1]
    ucolor = ngl.UniformVec4(value=color)
    graph_bg_ucolor = ngl.UniformVec4(value=(.15, .15, .15, 1))
    normed_graph_bg_ucolor = ngl.UniformVec4(value=(0, 0, 0, 1))
    line_ucolor = ngl.UniformVec4(value=(1, 1, 1, .4))

    # Text legend
    text = ngl.Text(text=easing,
                    fg_color=color,
                    padding=3,
                    bg_color=(0, 0, 0, 1),
                    box_corner=(-width / 2., height / 2. - text_height, 0),
                    box_width=(width, 0, 0),
                    box_height=(0, text_height, 0),
                    label='%s legend' % easing)

    # Graph drawing area (where the curve may overflow)
    graph_size = area_size - text_height - pad_height * 2
    graph_block = _block(graph_size, graph_size, color_program,
                         corner=(-graph_size/2, -(graph_size + text_height)/2, 0),
                         color=graph_bg_ucolor)

    # Normed area of the graph
    normed_graph_size = graph_size * curve_zoom
    normed_graph_block = _block(normed_graph_size, normed_graph_size, color_program,
                                corner=(-normed_graph_size/2, -(normed_graph_size + text_height)/2, 0),
                                color=normed_graph_bg_ucolor)

    # Curve
    easing_name, easing_args = _easing_split(easing)
    curve_scale_factor = graph_size / area_size * curve_zoom
    vertices_data = array.array('f')
    for i in range(nb_points + 1):
        t = i / float(nb_points)
        v = ngl.easing_evaluate(easing_name, t, easing_args)
        x = curve_scale_factor * (t * width - width / 2.)
        y = curve_scale_factor * (v * height - height / 2.)
        y -= text_height / 2.
        vertices_data.extend([x, y, 0])
    vertices = ngl.BufferVec3(data=vertices_data)
    geometry = ngl.Geometry(vertices, topology='line_strip')
    curve = ngl.Render(geometry, color_program, label='%s curve' % easing)
    curve.update_uniforms(color=ucolor)

    # Value cursor
    y = 2 / 3. * pad_height
    x = y * math.sqrt(3)
    cursor_geometry = ngl.Triangle((-x, y, 0), (0, 0, 0), (-x, -y, 0))
    cursor = ngl.Render(cursor_geometry, color_program, label='%s cursor' % easing)
    cursor.update_uniforms(color=ucolor)

    # Horizontal value line
    hline_data = array.array('f', (0, 0, 0, graph_size, 0, 0))
    hline_vertices = ngl.BufferVec3(data=hline_data)
    hline_geometry = ngl.Geometry(hline_vertices, topology='line_strip')
    hline = ngl.Render(hline_geometry, color_program, label='%s value line' % easing)
    hline.update_uniforms(color=line_ucolor)

    # Value animation (cursor + h-line)
    value_x = -graph_size / 2.
    value_y = (-text_height - normed_graph_size) / 2.
    value_animkf = (
        ngl.AnimKeyFrameVec3(0, (value_x, value_y, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (value_x, value_y + normed_graph_size, 0), easing_name, easing_args),
    )
    value_anim = ngl.Group(children=(hline, cursor))
    value_anim = ngl.Translate(value_anim, anim=ngl.AnimatedVec3(value_animkf), label='%s value anim' % easing)

    # Vertical time line
    vline_data = array.array('f', (0, 0, 0, 0, graph_size, 0))
    vline_vertices = ngl.BufferVec3(data=vline_data)
    vline_geometry = ngl.Geometry(vline_vertices, topology='line_strip')
    vline = ngl.Render(vline_geometry, color_program, label='%s time line' % easing)
    vline.update_uniforms(color=line_ucolor)

    # Time animation (v-line only)
    time_x = -normed_graph_size / 2.
    time_y = (-text_height - graph_size) / 2.
    time_animkf = (
        ngl.AnimKeyFrameVec3(0, (time_x, time_y, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (time_x + normed_graph_size, time_y, 0))
    )
    time_anim = ngl.Translate(vline, anim=ngl.AnimatedVec3(time_animkf), label='%s time anim' % easing)

    group = ngl.Group(label='%s block' % easing)
    group.add_children(text, graph_block, normed_graph_block, curve, value_anim, time_anim)
    return group


_easing_specs = (
    ('linear',    0, 1.),
    ('quadratic', 3, 1.),
    ('cubic',     3, 1.),
    ('quartic',   3, 1.),
    ('quintic',   3, 1.),
    ('power:7.3', 3, 1.),
    ('sinus',     3, 1.),
    ('exp',       3, 1.),
    ('circular',  3, 1.),
    ('bounce',    1, 1.),
    ('elastic',   1, 0.5),
    ('back',      3, 0.7),
)


def _get_easing_list():
    easings = []
    for col, (easing, flags, zoom) in enumerate(_easing_specs):
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
            easings.append((easing_name, zoom))
    return easings


_easing_list = _get_easing_list()
_easing_names = [e[0] for e in _easing_list]


def _get_easing_nodes(cfg, color_program):
    ag = AutoGrid(_easing_list)
    cfg.aspect_ratio = (ag.nb_cols, ag.nb_rows)
    easing_h = 1. / ag.nb_rows
    easing_w = 1. / ag.nb_cols
    for easing, easing_id, col, row in ag:
        easing_name, zoom = easing
        easing_node = _get_easing_node(cfg, easing_name, zoom, color_program)
        easing_node = ngl.Scale(easing_node, factors=[easing_w, easing_h, 0])
        x = easing_w * (-ag.nb_cols + 1 + 2 * col)
        y = easing_h * (ag.nb_rows - 1 - 2 * row)
        easing_node = ngl.Translate(easing_node, vector=(x, y, 0))
        yield easing_node


@scene(easing_id=scene.List(choices=['*'] + _easing_names))
def easings(cfg, easing_id='*'):
    '''Display all the easings (primitive for animation / motion design) at once'''
    random.seed(0)

    cfg.duration = 2.

    frag_data = cfg.get_frag('color')
    color_program = ngl.Program(fragment=frag_data, label='color')
    full_block = _block(2, 2, color_program, color=ngl.UniformVec4(value=(.3, .3, .3, 1)))

    group = ngl.Group()
    group.add_children(full_block)
    if easing_id == '*':
        for easing_node in _get_easing_nodes(cfg, color_program):
            group.add_children(easing_node)
    else:
        cfg.aspect_ratio = (1, 1)
        easing_index = _easing_names.index(easing_id)
        random.seed(easing_index)
        easing, zoom = _easing_list[easing_index]
        easing_node = _get_easing_node(cfg, easing, zoom, color_program)
        group.add_children(easing_node)

    return ngl.GraphicConfig(group,
                             blend=True,
                             blend_src_factor='src_alpha',
                             blend_dst_factor='one_minus_src_alpha',
                             blend_src_factor_a='zero',
                             blend_dst_factor_a='one')
