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
import os.path as op
import pynodegl as ngl
from pynodegl_utils.misc import scene, Media
from pynodegl_utils.tests.debug import get_debug_points
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.toolbox.utils import canvas, mask_with_geometry, blend


def _get_bg(cfg, m0):
    c, _ = canvas(cfg, m0)
    c.set_label('background')
    return c


def _get_colored_bg(cfg, color):
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    bg = ngl.Render(q, p, label='colored background')
    bg.update_frag_resources(color=color)
    return bg


def _get_filtered_bg(cfg, m0, color):
    frag = 'void main() { ngl_out_color = mix(ngl_texvideo(tex0, var_tex0_coord), color, color.a); }\n'
    c, _ = canvas(cfg, m0, frag, color=color)
    c.set_label('filtered background')
    return c


def _render_shape(cfg, geometry, color=COLORS.white):
    prog = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    render = ngl.Render(geometry, prog)
    render.update_frag_resources(color=ngl.UniformVec4(value=color))
    return render


@scene()
def prototype(cfg):
    m0 = Media(op.join(op.dirname(__file__), 'data', 'city2.webp'))
    cfg.aspect_ratio = (m0.width, m0.height)

    delay = 2.0  # delay before looping
    pause = 1.5
    text_effect_duration = 4.0

    in_start  = 0
    in_end    = in_start + text_effect_duration
    out_start = in_end + pause
    out_end   = out_start + text_effect_duration
    cfg.duration = out_end + delay

    colorin_animkf  = [ngl.AnimKeyFrameVec4(0, (1, 1, 1, 0)), ngl.AnimKeyFrameVec4(1, (1, 1, 1, 1))]
    colorout_animkf = [ngl.AnimKeyFrameVec4(0, (1, 1, 1, 1)), ngl.AnimKeyFrameVec4(1, (1, 1, 1, 0))]
    blurin_animkf   = [ngl.AnimKeyFrameFloat(0, 1), ngl.AnimKeyFrameFloat(1, 0)]
    blurout_animkf  = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(1, 1)]

    text_effect_settings = dict(
        target='char',
        random=True,
        overlap=ngl.UniformFloat(value=0.5),
    )

    text = ngl.Text(
        text='Prototype',
        bg_color=(0.0, 0.0, 0.0, 0.0),
        font_file=op.join(op.dirname(__file__), 'data', 'AVGARDN_2.woff'),
        aspect_ratio=cfg.aspect_ratio,
        effects=[
            ngl.TextEffect(
                start=in_start,
                end=in_end,
                random_seed=6,
                color=ngl.AnimatedVec4(colorin_animkf),
                blur=ngl.AnimatedFloat(blurin_animkf),
                **text_effect_settings),
            ngl.TextEffect(
                start=out_start,
                end=out_end,
                random_seed=50,
                color=ngl.AnimatedVec4(colorout_animkf),
                blur=ngl.AnimatedFloat(blurout_animkf),
                **text_effect_settings),
        ]
    )

    text_animkf = [
        ngl.AnimKeyFrameVec3(0.0, (0.5, 0.5, 0.5)),
        ngl.AnimKeyFrameVec3(cfg.duration, (0.9, 0.9, 0.9)),
    ]
    text = ngl.Scale(text, anim=ngl.AnimatedVec3(text_animkf))
    text_ranges = [
        ngl.TimeRangeModeCont(0),
        ngl.TimeRangeModeNoop(out_end)
    ]
    text = ngl.TimeRangeFilter(text, ranges=text_ranges)

    bg = _get_bg(cfg, m0)
    bg_animkf = [
        ngl.AnimKeyFrameVec3(0.0, (1.0, 1.0, 1.0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (1.2, 1.2, 1.2)),
    ]
    bg = ngl.Scale(bg, anim=ngl.AnimatedVec3(bg_animkf))

    return blend(bg, text)


@scene()
def japanese(cfg):
    m0 = Media(op.join(op.dirname(__file__), 'data', 'japan-gate.webp'))
    cfg.duration = 9.0
    cfg.aspect_ratio = (m0.width, m0.height)

    bgalpha_animkf = [
        ngl.AnimKeyFrameVec4(0,                (0, 0, 0, 0.0)),
        ngl.AnimKeyFrameVec4(1,                (0, 0, 0, 0.4)),
        ngl.AnimKeyFrameVec4(cfg.duration - 1, (0, 0, 0, 0.4)),
        ngl.AnimKeyFrameVec4(cfg.duration,     (0, 0, 0, 0.0)),
    ]
    bg = _get_filtered_bg(cfg, m0, ngl.AnimatedVec4(bgalpha_animkf))

    base_color = [1.0, 0.8, 0.6]
    text = ngl.Text(
        text='減る記憶、\nそれでも増える、\nパスワード',
        #font_file='/usr/share/fonts/TTF/HanaMinA.ttf',
        font_file='/usr/share/fonts/adobe-source-han-serif/SourceHanSerifJP-Regular.otf',
        bg_color=(0.0, 0.0, 0.0, 0.0),
        font_scale=1/2.,
        box_height=(0, 1.8, 0),
        writing_mode='vertical-rl',
        aspect_ratio=cfg.aspect_ratio,
        effects=[
            ngl.TextEffect(
                target='text',
                start=0.0,
                end=cfg.duration,
                color=ngl.UniformVec4(value=base_color + [1.0]),
            ),
            ngl.TextEffect(
                start=1.0,
                end=cfg.duration - 3.0,
                target='char',
                overlap=ngl.UniformFloat(value=0.7),
                color=ngl.AnimatedVec4([
                    ngl.AnimKeyFrameVec4(0, base_color + [0.0]),
                    ngl.AnimKeyFrameVec4(1, base_color + [1.0]),
                ]),
            ),
            ngl.TextEffect(
                target='text',
                start=cfg.duration - 2.0,
                end=cfg.duration - 1.0,
                color=ngl.AnimatedVec4([
                    ngl.AnimKeyFrameVec4(0, base_color + [1.0]),
                    ngl.AnimKeyFrameVec4(1, base_color + [0.0]),
                ]),
            ),
        ]
    )

    text_ranges = [
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(1),
        ngl.TimeRangeModeNoop(cfg.duration - 1.0)
    ]
    text = ngl.TimeRangeFilter(text, ranges=text_ranges)

    return blend(bg, text)


@scene(
    box_corner=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(1, 1, 1)),
    box_width=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(2, 2, 2)),
    box_height=scene.Vector(n=3, minv=(-1, -1, -1), maxv=(2, 2, 2)),
    font_scale=scene.Range(range=[0.1, 2], unit_base=100),
    scale_mode=scene.List(choices=('auto', 'fixed')),
)
def quick_brown_fox(cfg, box_corner=(-.4,-.6,0), box_width=(.7,.35,0), box_height=(0,0.9,0), font_scale=1.0, scale_mode='fixed'):
    return blend(ngl.Text(
        text='The quick brown fox\njumps over the lazy dog',
        font_file=op.join(op.dirname(__file__), 'data', 'AVGARDN_2.woff'),
        bg_color=(1.0, 0.0, 0.0, 0.3),
        box_corner=box_corner,
        box_width=box_width,
        box_height=box_height,
        aspect_ratio=cfg.aspect_ratio,
        scale_mode=scale_mode,
        font_scale=font_scale,
    ))


@scene()
def stress_test(cfg):
    return ngl.Text(
        #text='Glorious Free',
        #font_file=op.join(op.dirname(__file__), 'data', 'GloriousFree-dBR6.ttf'),
        #text='Destiny Signature',
        #font_file=op.join(op.dirname(__file__), 'data', 'DestinySignature-jEdp0.ttf'),
        #text='angelic war\nANGELIC WAR\nHLPR',
        text='H',
        font_file=op.join(op.dirname(__file__), 'data', 'AngelicWar.otf'),
        aspect_ratio=cfg.aspect_ratio,
    )


@scene()
def simple(cfg):
    return blend(ngl.Text(
        #text='ABCDEF',
        #text='Oo.j',
        text='The quick brown fox\njumps over the lazy dog',
        #text='A',
        #text='t',
        #font_file='/usr/share/fonts/adobe-source-code-pro/SourceCodePro-Black.otf',
        #font_file='/usr/share/fonts/adobe-source-han-serif/SourceHanSerifJP-Regular.otf',
        font_file=op.join(op.dirname(__file__), 'data', 'Quicksand-Medium.ttf'),
        aspect_ratio=cfg.aspect_ratio,
    ))


@scene(glow=scene.Range(range=(0, 1), unit_base=10))
def effect(cfg, glow=.5):
    return ngl.Text(
        text='a',
        aspect_ratio=cfg.aspect_ratio,
        #font_file=op.join(op.dirname(__file__), 'data', 'Quicksand-Medium.ttf'),
        effects=[
            ngl.TextEffect(
                glow=ngl.UniformFloat(glow),
                #outline=ngl.UniformFloat(0.005),
                end=cfg.duration,
            )
        ]
    )


@scene(target=scene.List(choices=('char', 'char_nospace', 'word', 'line', 'text')))
def targets(cfg, target='word'):
    s = 'Hello fifi World\nanother line'

    if 'char' in target:
        cfg.duration = len(s) * .25
    elif 'word' in target:
        cfg.duration = len(s.split()) * 0.5
    elif 'line' in target:
        cfg.duration = len(s.splitlines()) * 0.5
    else:
        cfg.duration = 1.0

    animkf = [
        ngl.AnimKeyFrameVec3(0,   (0.0, 0.0, 0.0)),
        ngl.AnimKeyFrameVec3(1/4, (0.0, 0.2, 0.0)),
        ngl.AnimKeyFrameVec3(3/4, (0.0,-0.2, 0.0)),
        ngl.AnimKeyFrameVec3(1,   (0.0, 0.0, 0.0)),
    ]
    trf = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(animkf))
    return ngl.Text(
        text=s,
        aspect_ratio=cfg.aspect_ratio,
        font_file='/usr/share/fonts/TTF/DejaVuSans.ttf',
        effects=[ngl.TextEffect(target=target, transform=trf, start=0, end=cfg.duration)]
    )


@scene(step=scene.Range(range=[0,11]))
def pwetpwet(cfg, step=10):
    cfg.duration = 5

    string = 'Pwet pwet!'
    font_file = None
    font_scale = None
    bounce_down = None
    target = None
    overlap = None
    effects = None
    random = None
    easing = None
    bg_color = (0, .2, .3, 1)

    if step > 8:
        cfg.aspect_ratio = (16, 10)

    if step > 9:
        m0 = Media(op.join(op.dirname(__file__), 'data', 'ryan-steptoe-XnOD9308hV4-unsplash.jpg'))
        cfg.aspect_ratio = (m0.width, m0.height)
        bg_color = (0, 0, 0, 0)

    if step > 7:
        easing = 'bounce_out'

    if step > 6:
        random = True

    if step > 5:
        overlap = ngl.UniformFloat(value=0.9)

    if step > 4:
        target = 'char'

    if step > 3:
        animkf = [
            ngl.AnimKeyFrameVec3(0, (0.0, 2.0, 0.0)),
            ngl.AnimKeyFrameVec3(1, (0.0, 0.0, 0.0), easing),
        ]
        bounce_down = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(animkf))
        effects = [
            ngl.TextEffect(
                start=0,
                end=cfg.duration - 2.0,
                target=target,
                overlap=overlap,
                transform=bounce_down,
                random=random,
            ),
        ]

    if step > 1:
        font_scale = 0.8

    if step > 0:
        font_file = op.join(op.dirname(__file__), 'data', 'AVGARDD_2.woff')

    if step > 10:
        string = 'كسول الزنجبيل القط'
        font_file = '/usr/share/fonts/noto/NotoKufiArabic-Regular.ttf'

    text = ngl.Text(
        text=string,
        font_file=font_file,
        bg_color=bg_color,
        aspect_ratio=cfg.aspect_ratio,
        font_scale=font_scale,
        effects=effects
    )

    if step == 3:
        animkf = [
            ngl.AnimKeyFrameVec3(0, (0.0, 2.0, 0.0)),
            ngl.AnimKeyFrameVec3(cfg.duration - 2.0, (0.0, 0.0, 0.0), easing),
        ]
        return ngl.Translate(text, anim=ngl.AnimatedVec3(animkf))


    if step >= 10:
        bg = _get_filtered_bg(cfg, m0, color=ngl.UniformVec4(value=(0, 0, 0, .2)))
        return blend(bg, text)

    return text


@scene()
def noise_cyberretro(cfg):
    m0 = Media(op.join(op.dirname(__file__), 'data', 'dark-street.webp'))
    cfg.aspect_ratio = (m0.width, m0.height)

    text = ngl.Text(
        text='CyberRetro',
        font_file=op.join(op.dirname(__file__), 'data', 'Quicksand-Medium.ttf'),
        bg_color=(0.0, 0.0, 0.0, 0.0),
        aspect_ratio=cfg.aspect_ratio,
        font_scale=.8,
        effects=[
            ngl.TextEffect(
                start=0,
                end=cfg.duration,
                target='text',
                color=ngl.UniformVec4(value=(0.2, 0.2, 0.2, 1.0)),
                glow_color=ngl.UniformVec4(value=(1, 0, 1, 1)),
            ),
            ngl.TextEffect(
                start=0,
                end=cfg.duration,
                start_pos=ngl.UniformFloat(0.49),
                target='text',
                glow=ngl.NoiseFloat(frequency=60, octaves=8, seed=1000),
            ),
            ngl.TextEffect(
                start=0,
                end=cfg.duration,
                end_pos=ngl.UniformFloat(0.49),
                target='text',
                glow=ngl.NoiseFloat(frequency=60, octaves=8, seed=2000),
            ),
        ]
    )

    bg = _get_bg(cfg, m0)
    return blend(bg, text)


@scene()
def masking(cfg):
    d = cfg.duration = 5
    easing = 'exp_out'
    ratio = 2/3

    m0 = Media(op.join(op.dirname(__file__), 'data', 'gloomy-staircase.webp'))
    cfg.aspect_ratio = (m0.width, m0.height)
    bg = _get_bg(cfg, m0)

    move_up_animkf = [
        ngl.AnimKeyFrameVec3(0, (0, -ratio, 0)),
        ngl.AnimKeyFrameVec3(1, (0, 0, 0), easing),
    ]
    move_up = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(move_up_animkf))

    move_down_animkf = [
        ngl.AnimKeyFrameVec3(0, (0, ratio, 0)),
        ngl.AnimKeyFrameVec3(1, (0, 0, 0), easing),
    ]
    move_down = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(move_down_animkf))

    text_params = dict(
        text='Stair\ncase',
        font_file=op.join(op.dirname(__file__), 'data', 'AVGARDD_2.woff'),
        bg_color=(0, 0, 0, 0),
        aspect_ratio=cfg.aspect_ratio,
        box_corner=(-.5, -ratio, 0),
        box_width=(1, 0, 0),
        box_height=(0, 2*ratio, 0),
        font_scale=0.8,
    )

    text_up   = ngl.Text(**text_params, effects=[ngl.TextEffect(start=0, end=d - 2, transform=move_up)])
    text_down = ngl.Text(**text_params, effects=[ngl.TextEffect(start=0, end=d - 2, transform=move_down)])

    mask_up_geom   = ngl.Quad(corner=(-1, 0, 0), width=(2, 0, 0), height=(0, 1, 0))
    mask_down_geom = ngl.Quad(corner=(-1,-1, 0), width=(2, 0, 0), height=(0, 1, 0))

    text_up   = mask_with_geometry(cfg, text_up, mask_down_geom, inverse=True)
    text_down = mask_with_geometry(cfg, text_down, mask_up_geom, inverse=True)

    line_height = .03
    animkf = [
        ngl.AnimKeyFrameVec3(0, (0, 1, 1)),
        ngl.AnimKeyFrameVec3(cfg.duration - 2, (1, 1, 1), easing),
    ]
    geom = ngl.Quad(corner=(-.5, -line_height/2, 0), width=(1, 0, 0), height=(0, line_height, 0))
    shape = _render_shape(cfg, geom)
    shape = ngl.Scale(shape, anim=ngl.AnimatedVec3(animkf))

    return ngl.Group(children=(bg, text_up, text_down, shape))


@scene()
def path_follow_wip(cfg):
    cfg.aspect_ratio = (1, 1)

    coords = dict(
        A=[-0.8, 0.0],
        B=[-0.2,-0.3],
        C=[ 0.2, 0.8],
        D=[ 0.8, 0.0],
    )

    points = array.array('f')
    points.extend(coords['A'] + [0])
    points.extend(coords['D'] + [0])
    points_buffer = ngl.BufferVec3(data=points)

    controls = array.array('f')
    controls.extend(coords['B'] + [0])
    controls.extend(coords['C'] + [0])
    controls_buffer = ngl.BufferVec3(data=controls)

    path = ngl.Path(points_buffer, controls_buffer, mode='bezier3')

    text = ngl.Text(
        text='Geronimo',
        font_file=op.join(op.dirname(__file__), 'data', 'AVGARDN_2.woff'),
        bg_color=(1.0, 0.0, 0.0, 0.5),
        aspect_ratio=cfg.aspect_ratio,
        box_corner=(-.5,-.5,0),
        box_width=(1,0,0),
        box_height=(0,1,0),
        font_scale=0.8,
        path=path,
    )

    return blend(
        ngl.PathDraw(path),
        get_debug_points(cfg, dict(p0=(-0.800000,0.000000), p=(-0.659694,-0.046800))),
        text,
    )


@scene()
def skew_zomgspeed(cfg):
    cfg.duration = 4

    m0 = Media(op.join(op.dirname(__file__), 'data', 'tim-foster-CoSJhdxIiik-unsplash.jpg'))
    cfg.aspect_ratio = (m0.width, m0.height)
    bg = _get_bg(cfg, m0)

    skew_amount = -45

    skew_animkf = [
        ngl.AnimKeyFrameVec3(0,   (0, skew_amount, 0)),
        ngl.AnimKeyFrameVec3(0.2, (0, skew_amount, 0), 'exp_in'),
        ngl.AnimKeyFrameVec3(1,   (0, 0, 0), 'elastic_out'),
    ]
    move_animkf = [
        ngl.AnimKeyFrameVec3(0,   (-3, 0, 0)),
        ngl.AnimKeyFrameVec3(0.2, (-1, 0, 0), 'exp_in'),
        ngl.AnimKeyFrameVec3(1.0, (0, 0, 0), 'elastic_out'),
    ]

    trf = ngl.Skew(ngl.Identity(), anim=ngl.AnimatedVec3(skew_animkf), anchor=(0, -1, 0))
    trf = ngl.Translate(trf, anim=ngl.AnimatedVec3(move_animkf))

    text = ngl.Text(
        text='ZOMG\nSPEED',
        font_file=op.join(op.dirname(__file__), 'data', 'AVGARDD_2.woff'),
        bg_color=(0.0, 0.0, 0.0, 0.0),
        aspect_ratio=cfg.aspect_ratio,
        font_scale=0.8,
        effects=[ngl.TextEffect(start=0, end=cfg.duration - 2.0, transform=trf)]
    )

    return blend(bg, text)


# derived from https://www.youtube.com/watch?v=Y2D-k-8syoE
@scene()
def motion(cfg):
    cfg.duration = 4
    cfg.aspect_ratio = (16, 9)
    bg = _get_colored_bg(cfg, ngl.UniformVec4(value=(.1, .1, .1, 1)))

    easing = 'back_out'
    vert_scale_down_animkf = [
        ngl.AnimKeyFrameVec3(0, (1, 5, 1)),
        ngl.AnimKeyFrameVec3(1, (1, 1, 1), easing),
    ]
    scale_down_animkf = [
        ngl.AnimKeyFrameVec3(0, (15, 15, 15)),
        ngl.AnimKeyFrameVec3(1, (1, 1, 1), easing),
    ]
    drop_down_animkf = [
        ngl.AnimKeyFrameVec3(0, (0, 2, 0)),
        ngl.AnimKeyFrameVec3(1, (0, 0, 0), easing),
    ]
    jump_up_animkf = [
        ngl.AnimKeyFrameVec3(0, (0,-2, 0)),
        ngl.AnimKeyFrameVec3(1, (0, 0, 0), easing),
    ]
    slide_r2l_animkf = [
        ngl.AnimKeyFrameVec3(0, (2, 0, 0)),
        ngl.AnimKeyFrameVec3(1, (0, 0, 0), easing),
    ]

    vert_scale_down = ngl.Scale(ngl.Identity(),     anim=ngl.AnimatedVec3(vert_scale_down_animkf), label='vertical scale down')
    scale_down      = ngl.Scale(ngl.Identity(),     anim=ngl.AnimatedVec3(scale_down_animkf),      label='scale down')
    drop_down       = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(drop_down_animkf),       label='drop down')
    jump_up         = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(jump_up_animkf),         label='jump up')
    scale_down2     = ngl.Scale(ngl.Identity(),     anim=ngl.AnimatedVec3(scale_down_animkf),      label='scale down')  # XXX worth perf wise?
    slide_r2l       = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(slide_r2l_animkf),       label='slide right to left')

    string = 'MOTION'
    chr_effects = [
        dict(transform=vert_scale_down),  # M
        dict(transform=scale_down),       # O
        dict(transform=drop_down),        # T
        dict(transform=jump_up),          # I
        dict(transform=scale_down2),      # O
        dict(transform=slide_r2l),        # N
    ]
    assert len(string) == len(chr_effects)
    n = len(string)

    texts = [
        ('shadow', 0.15, ngl.UniformVec4(value=(0,.6,.9, 1))),
        ('main',   0,    ngl.UniformVec4(value=(1, 1, 1, 1))),
    ]

    children = [bg]
    for label, lag, color in texts:
        text = ngl.Text(
            label=label,
            text=string,
            font_file=op.join(op.dirname(__file__), 'data', 'AVGARDD_2.woff'),
            bg_color=(0.0, 0.0, 0.0, 0.0),
            aspect_ratio=cfg.aspect_ratio,
            font_scale=0.6,
            effects=[
                ngl.TextEffect(
                    color=color,
                    start=0,
                    end=cfg.duration + lag - 2.0,
                    target='char',
                    start_pos=ngl.UniformFloat(i / n),
                    end_pos=ngl.UniformFloat((i + 1) / n),
                    overlap=ngl.UniformFloat(value=0.2),
                    **chr_effect)
                for i, chr_effect in enumerate(chr_effects)
            ]
        )
        children.append(text)

    return blend(*children)
