import os.path as op
import array
import math
import random


from pynodegl import (
        AnimKeyFrameFloat,
        AnimKeyFrameVec3,
        AnimKeyFrameVec4,
        AnimKeyFrameQuat,
        AnimatedFloat,
        AnimatedVec3,
        AnimatedVec4,
        AnimatedQuat,
        BufferFloat,
        BufferUInt,
        BufferUBVec3,
        BufferUBVec4,
        BufferUIVec4,
        BufferVec2,
        BufferVec3,
        Camera,
        Circle,
        Compute,
        ComputeProgram,
        GraphicConfig,
        Geometry,
        Group,
        Media,
        Program,
        Quad,
        Render,
        RenderToTexture,
        Rotate,
        Scale,
        Texture2D,
        Texture3D,
        Translate,
        Triangle,
        UniformInt,
        UniformFloat,
        UniformVec3,
        UniformVec4,
        UniformQuat,
)

from pynodegl_utils.misc import scene, get_frag, get_vert, get_comp
from pynodegl_utils.misc import get_vk_frag, get_vk_vert


@scene(xsplit={'type': 'range', 'range': [0, 1], 'unit_base': 100},
       trilinear={'type': 'bool'})
def lut3d(cfg, xsplit=.3, trilinear=True):
    level = 6
    level2 = level**2

    # Generated with `ffmpeg -f lavfi -i haldclutsrc=6,curves=vintage -f
    # rawvideo -frames:v 1 lut3d.raw`
    lut3d_filename = op.join(op.dirname(__file__), 'data', 'lut3d.raw')
    cfg.files.append(lut3d_filename)
    lut3d_buf = BufferUBVec3(filename=lut3d_filename)
    lut3d_tex = Texture3D(data_src=lut3d_buf,
                          width=level2, height=level2, depth=level2)
    if trilinear:
        lut3d_tex.set_min_filter('linear')
        lut3d_tex.set_mag_filter('linear')

    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    video = Media(m0.filename)
    video_tex = Texture2D(data_src=video)

    shader_version = '300 es' if cfg.backend == 'gles' else '330'
    shader_header = '#version %s\n' % shader_version
    prog = Program(fragment=shader_header + get_frag('lut3d'),
                   vertex=shader_header + get_vert('lut3d'))

    quad = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = Render(quad, prog)
    render.update_textures(tex0=video_tex)
    render.update_textures(lut3d=lut3d_tex)
    render.update_uniforms(xsplit=UniformFloat(value=xsplit))

    return render


@scene(bgcolor1={'type': 'color'},
       bgcolor2={'type': 'color'},
       bilinear_filtering={'type': 'bool'})
def buffer_dove(cfg,
                bgcolor1=(.6, 0, 0, 1), bgcolor2=(.8, .8, 0, 1),
                bilinear_filtering=True):
    cfg.duration = 3.

    # Credits: https://icons8.com/icon/40514/dove
    icon_filename = op.join(op.dirname(__file__), 'data', 'icons8-dove.raw')
    cfg.files.append(icon_filename)
    w, h = (96, 96)
    cfg.aspect_ratio = (w, h)

    img_buf = BufferUBVec4(filename=icon_filename, name='icon raw buffer')

    img_tex = Texture2D(data_src=img_buf, width=w, height=h)
    if bilinear_filtering:
        img_tex.set_mag_filter('linear')
    quad = Quad((-.5, -.5, 0.1), (1, 0, 0), (0, 1, 0))
    prog = Program()
    render = Render(quad, prog, name='dove')
    render.update_textures(tex0=img_tex)
    render = GraphicConfig(render,
                           blend=True,
                           blend_src_factor='src_alpha',
                           blend_dst_factor='one_minus_src_alpha',
                           blend_src_factor_a='zero',
                           blend_dst_factor_a='one')

    prog_bg = Program(fragment=get_frag('color'))
    shape_bg = Circle(radius=.6, npoints=256)
    render_bg = Render(shape_bg, prog_bg, name='background')
    color_animkf = [AnimKeyFrameVec4(0,                bgcolor1),
                    AnimKeyFrameVec4(cfg.duration/2.0, bgcolor2),
                    AnimKeyFrameVec4(cfg.duration,     bgcolor1)]
    ucolor = UniformVec4(anim=AnimatedVec4(color_animkf))
    render_bg.update_uniforms(color=ucolor)

    return Group(children=(render_bg, render))


@scene(size={'type': 'range', 'range': [0, 1.5], 'unit_base': 1000})
def triangle(cfg, size=0.5):
    b = size * math.sqrt(3) / 2.0
    c = size * 1/2.
    cfg.duration = 3.
    cfg.aspect_ratio = (1, 1)

    triangle = Triangle((-b, -c, 0), (b, -c, 0), (0, size, 0))
    p = Program(fragment=get_frag('triangle'))
    node = Render(triangle, p)
    animkf = [AnimKeyFrameFloat(0, 0),
              AnimKeyFrameFloat(  cfg.duration/3.,   -360/3., 'exp_in_out'),
              AnimKeyFrameFloat(2*cfg.duration/3., -2*360/3., 'exp_in_out'),
              AnimKeyFrameFloat(  cfg.duration,      -360,    'exp_in_out')]
    node = Rotate(node, anim=AnimatedFloat(animkf))
    return node


@scene(n={'type': 'range', 'range': [2, 10]})
def fibo(cfg, n=8):
    cfg.duration = 5.0
    cfg.aspect_ratio = (1, 1)

    p = Program(fragment=get_frag('color'))

    fib = [0, 1, 1]
    for i in range(2, n):
        fib.append(fib[i] + fib[i-1])
    fib = fib[::-1]

    shift = 1/3.  # XXX: what's the exact math here?
    shape_scale = 1. / ((2.-shift) * sum(fib))

    orig = (-shift, -shift, 0)
    g = None
    root = None
    for i, x in enumerate(fib[:-1]):
        w = x * shape_scale
        gray = 1. - i/float(n)
        color = [gray, gray, gray, 1]
        q = Quad(orig, (w, 0, 0), (0, w, 0))
        render = Render(q, p)
        render.update_uniforms(color=UniformVec4(value=color))

        new_g = Group()
        animkf = [AnimKeyFrameFloat(0,               90),
                  AnimKeyFrameFloat(cfg.duration/2, -90, "exp_in_out"),
                  AnimKeyFrameFloat(cfg.duration,    90, "exp_in_out")]
        rot = Rotate(new_g, anchor=orig, anim=AnimatedFloat(animkf))
        if g:
            g.add_children(rot)
        else:
            root = rot
        g = new_g
        new_g.add_children(render)
        orig = (orig[0] + w, orig[1] + w, 0)

    return root


@scene(dim={'type': 'range', 'range': [1, 50]})
def cropboard(cfg, dim=15):
    m0 = cfg.medias[0]
    random.seed(0)
    cfg.duration = 10
    cfg.aspect_ratio = (m0.width, m0.height)

    kw = kh = 1. / dim
    qw = qh = 2. / dim
    tqs = []

    p = Program()
    m = Media(m0.filename)
    t = Texture2D(data_src=m)

    for y in range(dim):
        for x in range(dim):
            corner = (-1. + x*qw, 1. - (y+1.)*qh, 0)
            q = Quad(corner, (qw, 0, 0), (0, qh, 0))

            q.set_uv_corner(x*kw, 1. - (y+1.)*kh)
            q.set_uv_width(kw, 0)
            q.set_uv_height(0, kh)

            render = Render(q, p)
            render.update_textures(tex0=t)

            startx = random.uniform(-2, 2)
            starty = random.uniform(-2, 2)
            trn_animkf = [AnimKeyFrameVec3(0, (startx, starty, 0)),
                          AnimKeyFrameVec3(cfg.duration*2/3., (0, 0, 0), "exp_out")]
            trn = Translate(render, anim=AnimatedVec3(trn_animkf))
            tqs.append(trn)

    return Group(children=tqs)


@scene(freq_precision={'type': 'range', 'range': [1, 10]},
       overlay={'type': 'range', 'unit_base': 100})
def audiotex(cfg, freq_precision=7, overlay=0.6):
    media = cfg.medias[0]
    cfg.duration = media.duration
    cfg.aspect_ratio = (media.width, media.height)

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    audio_m = Media(media.filename, audio_tex=1)
    audio_tex = Texture2D(data_src=audio_m)

    video_m = Media(media.filename)
    video_tex = Texture2D(data_src=video_m)

    p = Program(vertex=get_vert('dual-tex'),
                fragment=get_frag('audiotex'))
    render = Render(q, p)
    render.update_textures(tex0=audio_tex, tex1=video_tex)
    render.update_uniforms(overlay=UniformFloat(overlay))
    render.update_uniforms(freq_precision=UniformInt(freq_precision))
    return render


@scene(particules={'type': 'range', 'range': [1, 1023]})
def particules(cfg, particules=32):
    random.seed(0)

    compute_data_version = "310 es" if cfg.backend == "gles" else "430"
    compute_data = '#version %s\n' % compute_data_version
    compute_data += get_comp('particules')

    cfg.duration = 6

    x = 64
    p = x * particules

    positions = array.array('f')
    velocities = array.array('f')

    for i in range(p):
        positions.extend([
            random.uniform(-1.0, 1.0),
            random.uniform(0.0, 1.0),
            0.0,
            0.0,
        ])

        velocities.extend([
            random.uniform(-0.01, 0.01),
            random.uniform(-0.05, 0.05),
        ])

    ipositions = BufferVec3()
    ipositions.set_data(positions)
    ipositions.set_stride(4 * 4)
    ivelocities = BufferVec2()
    ivelocities.set_data(velocities)

    opositions = BufferVec3(p)
    opositions.set_stride(4 * 4)

    animkf = [AnimKeyFrameFloat(0, 0),
              AnimKeyFrameFloat(cfg.duration, 1)]
    utime = UniformFloat(anim=AnimatedFloat(animkf))
    uduration = UniformFloat(cfg.duration)

    cp = ComputeProgram(compute_data)

    c = Compute(x, particules, 1, cp)
    c.update_uniforms(
        time=utime,
        duration=uduration,
    )
    c.update_buffers(
        ipositions_buffer=ipositions,
        ivelocities_buffer=ivelocities,
        opositions_buffer=opositions,
    )

    gm = Geometry(opositions)
    gm.set_draw_mode('points')

    p = Program(fragment=get_frag('color'))
    r = Render(gm, p)
    r.update_uniforms(color=UniformVec4(value=(0, .6, .8, 1)))

    g = Group()
    g.add_children(c, r)

    return Camera(g)


@scene()
def blending_and_stencil(cfg):
    cfg.duration = 5
    random.seed(0)
    fragment = get_frag('color')

    program = Program(fragment=fragment)
    circle = Circle(npoints=256)
    cloud_color = UniformVec4(value=(1, 1, 1, 0.4))

    main_group = Group()

    quad = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = Render(quad, program, name='sky')
    render.update_uniforms(color=UniformVec4(value=(0.2, 0.6, 1, 1)))
    config = GraphicConfig(render,
                           stencil_test=True,
                           stencil_write_mask=0xFF,
                           stencil_func='always',
                           stencil_ref=1,
                           stencil_read_mask=0xFF,
                           stencil_fail='replace',
                           stencil_depth_fail='replace',
                           stencil_depth_pass='replace')
    main_group.add_children(config)

    render = Render(circle, program, name='sun')
    render.update_uniforms(color=UniformVec4(value=(1, 0.8, 0, 1)))

    scale = Scale(render, (0.15, 0.15, 0.0))
    translate = Translate(scale, (0.4, 0.3, 0))
    main_group.add_children(translate)

    cloud_group = Group(name='clouds')

    centers = [
        (-1.0, 0.85, 0.4),
        (-0.5, 2.0,  1.0),
        (   0, 0.85, 0.4),
        ( 1.0, 1.55, 0.8),
        ( 0.6, 0.65, 0.075),
        ( 0.5, 1.80, 1.25),
    ]

    for center in centers:
        render = Render(circle, program)
        render.update_uniforms(color=cloud_color)

        factor = random.random() * 0.4 + center[2]
        keyframe = cfg.duration * (random.random() * 0.4 + 0.2)
        animkf = (AnimKeyFrameVec3(0,            (factor,       factor,       0)),
                  AnimKeyFrameVec3(keyframe,     (factor + 0.1, factor + 0.1, 0)),
                  AnimKeyFrameVec3(cfg.duration, (factor,       factor,       0)))
        scale = Scale(render, anim=AnimatedVec3(animkf))

        translate = Translate(scale, vector=(center[0], center[1], 0))
        cloud_group.add_children(translate)

    config = GraphicConfig(cloud_group,
                           blend=True,
                           blend_src_factor='src_alpha',
                           blend_dst_factor='one_minus_src_alpha',
                           blend_src_factor_a='zero',
                           blend_dst_factor_a='one',
                           stencil_test=True,
                           stencil_write_mask=0x0,
                           stencil_func='equal',
                           stencil_ref=1,
                           stencil_read_mask=0xFF,
                           stencil_fail='keep',
                           stencil_depth_fail='keep',
                           stencil_depth_pass='keep')
    main_group.add_children(config)

    camera = Camera(main_group)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float, 1.0, 10.0)

    return camera


def _get_cube_quads():
            # corner             width        height      color
    return (((-0.5, -0.5,  0.5), ( 1, 0,  0), (0, 1,  0), (1, 1, 0)),  # front
            (( 0.5, -0.5, -0.5), (-1, 0,  0), (0, 1,  0), (0, 0, 1)),  # back
            ((-0.5, -0.5, -0.5), ( 0, 0,  1), (0, 1,  0), (0, 1, 0)),  # left
            (( 0.5, -0.5,  0.5), ( 0, 0, -1), (0, 1,  0), (0, 1, 1)),  # right
            ((-0.5, -0.5, -0.5), ( 1, 0,  0), (0, 0,  1), (1, 0, 0)),  # bottom
            ((-0.5,  0.5,  0.5), ( 1, 0,  0), (0, 0, -1), (1, 0, 1)))  # top


def _get_cube_side(texture, program, corner, width, height, color):
    render = Render(Quad(corner, width, height), program)
    render.update_textures(tex0=texture)
    render.update_uniforms(blend_color=UniformVec3(value=color))
    render.update_uniforms(mix_factor=UniformFloat(value=0.2))
    return render


@scene(display_depth_buffer={'type': 'bool'})
def cube(cfg, display_depth_buffer=False):
    cube = Group(name="cube")

    frag_data = get_frag('tex-tint')
    program = Program(fragment=frag_data)

    texture = Texture2D(data_src=Media(cfg.medias[0].filename))
    children = [_get_cube_side(texture, program, qi[0], qi[1], qi[2], qi[3]) for qi in _get_cube_quads()]
    cube.add_children(*children)

    for i in range(3):
        rot_animkf = AnimatedFloat([AnimKeyFrameFloat(0,            0),
                                    AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))])
        axis = [int(i == x) for x in range(3)]
        cube = Rotate(cube, axis=axis, anim=rot_animkf)

    config = GraphicConfig(cube, depth_test=True)

    camera = Camera(config)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float, 1.0, 10.0)

    if not display_depth_buffer:
        return camera
    else:
        group = Group()

        depth_texture = Texture2D()
        depth_texture.set_format("depth_component")
        depth_texture.set_type("unsigned_short")
        depth_texture.set_width(640)
        depth_texture.set_height(480)

        texture = Texture2D()
        texture.set_width(640)
        texture.set_height(480)
        rtt = RenderToTexture(camera, texture)
        rtt.set_depth_texture(depth_texture)

        quad = Quad((-1.0, -1.0, 0), (1, 0, 0), (0, 1, 0))
        program = Program()
        render = Render(quad, program)
        render.update_textures(tex0=texture)
        group.add_children(rtt, render)

        quad = Quad((0.0, 0.0, 0), (1, 0, 0), (0, 1, 0))
        program = Program()
        render = Render(quad, program)
        render.update_textures(tex0=depth_texture)
        group.add_children(rtt, render)

        return group


@scene()
def histogram(cfg):
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)
    g = Group()

    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    h = BufferUIVec4(256 + 1)

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    r = Render(q)
    r.update_textures(tex0=t)
    proxy_size = 128
    proxy = Texture2D(width=proxy_size, height=proxy_size, immutable=1)
    rtt = RenderToTexture(r, proxy)
    g.add_children(rtt)

    shader_version = '310 es' if cfg.backend == 'gles' else '430'
    shader_header = '#version %s\n' % shader_version
    if cfg.backend == 'gles' and cfg.system == 'Android':
        shader_header += '#extension GL_ANDROID_extension_pack_es31a: require\n'

    compute_program = ComputeProgram(shader_header + get_comp('histogram-clear'))
    compute = Compute(256, 1, 1, compute_program, name='histogram-clear')
    compute.update_buffers(histogram_buffer=h)
    g.add_children(compute)

    local_size = 8
    group_size = proxy_size / local_size
    compute_shader = get_comp('histogram-exec') % {'local_size': local_size}
    compute_program = ComputeProgram(shader_header + compute_shader)
    compute = Compute(group_size, group_size, 1, compute_program, name='histogram-exec')
    compute.update_buffers(histogram=h)
    compute.update_textures(source=proxy)
    g.add_children(compute)

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = Program(vertex=shader_header + get_vert('histogram-display'),
                fragment=shader_header + get_frag('histogram-display'))
    render = Render(q, p)
    render.update_textures(tex0=t)
    render.update_buffers(histogram_buffer=h)
    g.add_children(render)

    return g


@scene()
def quaternion(cfg):
    cfg.duration = 10.
    step = cfg.duration / 5.
    x = math.sqrt(0.5)
    quat_animkf = [
        AnimKeyFrameQuat(0 * step, (0, 0, 0, 1)),
        AnimKeyFrameQuat(1 * step, (0, 0,-x, x)),
        AnimKeyFrameQuat(2 * step, (0, 1, 0, 0)),
        AnimKeyFrameQuat(3 * step, (1, 0, 0, 0)),
        AnimKeyFrameQuat(4 * step, (x, 0, 0, x)),
        AnimKeyFrameQuat(5 * step, (0, 0, 0, 1)),
    ]
    quat = UniformQuat(anim=AnimatedQuat(quat_animkf))

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program(vertex=get_vert('uniform-mat4'))
    render = Render(q, p)
    render.update_textures(tex0=t)
    render.update_uniforms(transformation_matrix=quat)

    camera = Camera(render)
    camera.set_eye(0.0, 0.0, 4.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float, 1.0, 10.0)

    return camera


@scene(ndim={'type': 'range', 'range': [1,8]},
       nb_layers={'type': 'range', 'range': [1,10]},
       ref_color={'type': 'color'},
       nb_mountains={'type': 'range', 'range': [3, 15]})
def mountain(cfg, ndim=3, nb_layers=7,
             ref_color=(0.5, .75, .75, 1.0), nb_mountains=6):
    random.seed(0)
    random_dim = 1 << ndim
    cfg.aspect_ratio = (16, 9)
    cfg.duration = nb_mountains ** 2

    def get_rand():
        return array.array('f', [random.uniform(0, 1) for x in range(random_dim)])

    black, white = (0, 0, 0, 1), (1, 1, 1, 1)
    quad = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))

    prog = Program(fragment=get_frag('mountain'))
    hscale = 1/2.
    mountains = []
    for i in range(nb_mountains):
        yoffset = (nb_mountains-i-1)/float(nb_mountains-1) * (1.0 - hscale)

        if i < nb_mountains/2:
            c0, c1 = ref_color, white
            x = (i + 1) / float(nb_mountains/2 + 1)
        else:
            c0, c1 = black, ref_color
            x = (i - nb_mountains/2) / float((nb_mountains-1)/2)
        mcolor = [x*a + (1.0-x)*b for a, b in zip(c0, c1)]

        random_buf = BufferFloat(data=get_rand())
        random_tex = Texture2D(data_src=random_buf, width=random_dim, height=1)

        utime_animkf = [AnimKeyFrameFloat(0, 0),
                        AnimKeyFrameFloat(cfg.duration, i+1)]
        utime = UniformFloat(anim=AnimatedFloat(utime_animkf))

        uyoffset_animkf = [AnimKeyFrameFloat(0, yoffset/2.),
                           AnimKeyFrameFloat(cfg.duration/2.0, yoffset),
                           AnimKeyFrameFloat(cfg.duration, yoffset/2.)]
        uyoffset = UniformFloat(anim=AnimatedFloat(uyoffset_animkf))

        render = Render(quad, prog)
        render.update_textures(tex0=random_tex)
        render.update_uniforms(dim=UniformInt(random_dim))
        render.update_uniforms(nb_layers=UniformInt(nb_layers))
        render.update_uniforms(time=utime)
        render.update_uniforms(lacunarity=UniformFloat(2.0))
        render.update_uniforms(gain=UniformFloat(0.5))
        render.update_uniforms(mcolor=UniformVec4(mcolor))
        render.update_uniforms(yoffset=uyoffset)
        render.update_uniforms(hscale=UniformFloat(hscale))

        mountains.append(render)

    prog = Program(fragment=get_frag('color'))
    sky = Render(quad, prog)
    sky.update_uniforms(color=UniformVec4(white))

    group = Group(children=[sky] + mountains)
    blend = GraphicConfig(group,
                          blend=True,
                          blend_src_factor='src_alpha',
                          blend_dst_factor='one_minus_src_alpha',
                          blend_src_factor_a='zero',
                          blend_dst_factor_a='one')
    return blend

@scene()
def vktest(cfg):
    cfg.duration = 4
    colors_data = array.array('f', [1.0, 0.0, 0.0,
                                    0.0, 1.0, 0.0,
                                    0.0, 0.0, 1.0,
                                    1.0, 1.0, 1.0])
    vertices_data = array.array('f', [-0.5, -0.5, 0.0,
                                       0.5, -0.5, 0.0,
                                       0.5,  0.5, 0.0,
                                      -0.5,  0.5, 0.0])

    indices_data = array.array('i', [0, 1, 2, 2, 3, 0])

    colors_buffer = BufferVec3(data=colors_data)
    vertices_buffer = BufferVec3(data=vertices_data)
    indices_buffer = BufferUInt(data=indices_data)

    geometry = Geometry(vertices_buffer, indices=indices_buffer)
    program = Program(fragment=get_vk_frag('vktest'),
                      vertex=get_vk_vert('vktest'))
    render = Render(geometry, program)
    render.update_attributes(color=colors_buffer)

    animkf = [AnimKeyFrameFloat(0, 0),
              AnimKeyFrameFloat(1*cfg.duration/4., -1*360/4., 'exp_in_out'),
              AnimKeyFrameFloat(2*cfg.duration/4., -2*360/4., 'exp_in_out'),
              AnimKeyFrameFloat(3*cfg.duration/4., -3*360/4., 'exp_in_out'),
              AnimKeyFrameFloat(4*cfg.duration/4., -4*360/4., 'exp_in_out')]
    rotate = Rotate(render, anim=AnimatedFloat(animkf))

    gc = GraphicConfig(rotate,
                       color_write_mask='r+g+a')

    camera = Camera(gc)
    camera.set_eye(0.0, 0.0, 4.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float, 1.0, 10.0)

    return camera
