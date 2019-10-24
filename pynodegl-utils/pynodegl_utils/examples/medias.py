import pynodegl as ngl
from pynodegl_utils.misc import scene


@scene(uv_corner=scene.Vector(n=2),
       uv_width=scene.Vector(n=2),
       uv_height=scene.Vector(n=2),
       progress_bar=scene.Bool())
def centered_media(cfg, uv_corner=(0, 0), uv_width=(1, 0), uv_height=(0, 1), progress_bar=True):
    '''A simple centered media with an optional progress bar in the shader'''
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0), uv_corner, uv_width, uv_height)
    m = ngl.Media(m0.filename)
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(q, p)
    render.update_frag_resources(tex0=t)

    if progress_bar:
        p.set_fragment(cfg.get_frag('progress-bar'))

        media_duration = ngl.UniformFloat(m0.duration)
        ar = ngl.UniformFloat(cfg.aspect_ratio_float)
        render.update_frag_resources(media_duration=media_duration, ar=ar)
    return render


@scene(speed=scene.Range(range=[0.01, 2], unit_base=1000))
def playback_speed(cfg, speed=1.0):
    '''Adjust media playback speed using animation keyframes'''
    m0 = cfg.medias[0]
    media_duration = m0.duration
    initial_seek = 5
    rush_duration = media_duration - initial_seek
    cfg.duration = rush_duration / speed
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    time_animkf = [ngl.AnimKeyFrameFloat(0, initial_seek),
                   ngl.AnimKeyFrameFloat(cfg.duration, media_duration)]
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(time_animkf))
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    render = ngl.Render(q, p)
    render.update_frag_resources(tex0=t)
    return render


@scene()
def time_remapping(cfg):
    '''
    Time remapping in the following order:
    - nothing displayed for a while (but media prefetch happening in background)
    - first frame displayed for a while
    - normal playback
    - last frame displayed for a while (even though the media is closed)
    - nothing again until the end
    '''
    m0 = cfg.medias[0]

    media_seek = 10
    noop_duration = 2
    prefetch_duration = 2
    freeze_duration = 3
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start  = range_start   + freeze_duration
    play_stop   = play_start    + playback_duration
    range_stop  = play_stop     + freeze_duration
    duration    = range_stop    + noop_duration

    cfg.duration = duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop,  media_seek + playback_duration),
    ]

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    m.set_sxplayer_min_level('verbose')
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    p.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_uvcoord=ngl.IOVec2())
    r = ngl.Render(q, p)
    r.update_frag_resources(tex0=t)

    time_ranges = [
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(range_start),
        ngl.TimeRangeModeNoop(range_stop),
    ]
    rf = ngl.TimeRangeFilter(r, ranges=time_ranges, prefetch_time=prefetch_duration)

    base_string = 'media time: %2g to %2g\nscene time: %2g to %2g\ntime range: %2g to %2g' % (
                  media_seek, media_seek + playback_duration, play_start, play_stop, range_start, range_stop)
    text = ngl.Text(base_string,
                    box_height=(0, 0.3, 0),
                    box_corner=(-1, 1 - 0.3, 0),
                    aspect_ratio=cfg.aspect_ratio,
                    halign='left')

    group = ngl.Group()
    group.add_children(rf, text)

    steps = (
        ('default color, nothing yet', 0, noop_duration),
        ('default color, media prefetched', noop_duration, range_start),
        ('first frame', range_start, play_start),
        ('normal playback', play_start, play_stop),
        ('last frame', play_stop, range_stop),
        ('default color, media released', range_stop, duration),
    )

    for i, (description, start_time, end_time) in enumerate(steps):
        text = ngl.Text('%g to %g: %s' % (start_time, end_time, description),
                        aspect_ratio=cfg.aspect_ratio,
                        box_height=(0, 0.2, 0))
        text_tr = (
            ngl.TimeRangeModeNoop(0),
            ngl.TimeRangeModeCont(start_time),
            ngl.TimeRangeModeNoop(end_time),
        )
        text_rf = ngl.TimeRangeFilter(text, ranges=text_tr, label='text-step-%d' % i)
        group.add_children(text_rf)

    return ngl.GraphicConfig(group, blend=True,
                             blend_src_factor='src_alpha',
                             blend_dst_factor='one_minus_src_alpha',
                             blend_src_factor_a='zero',
                             blend_dst_factor_a='one')
