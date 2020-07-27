import array
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import get_random_color_buffer


@scene(color=scene.Color(),
       rotate=scene.Bool(),
       scale=scene.Bool(),
       translate=scene.Bool())
def animated_square(cfg, color=(1, 0.66, 0, 1), rotate=True, scale=True, translate=True):
    '''Animated Translate/Scale/Rotate on a square'''
    cfg.duration = 5.0
    cfg.aspect_ratio = (1, 1)

    sz = 1/3.
    q = ngl.Quad((-sz/2, -sz/2, 0), (sz, 0, 0), (0, sz, 0))
    p = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    node = ngl.Render(q, p)
    ucolor = ngl.UniformVec4(value=color)
    node.update_frag_resources(color=ucolor)

    coords = [(-1, 1), (1, 1), (1, -1), (-1, -1), (-1, 1)]

    if rotate:
        animkf = (ngl.AnimKeyFrameFloat(0,            0),
                  ngl.AnimKeyFrameFloat(cfg.duration, 360))
        node = ngl.Rotate(node, anim=ngl.AnimatedFloat(animkf))

    if scale:
        animkf = (ngl.AnimKeyFrameVec3(0,              (1, 1, 1)),
                  ngl.AnimKeyFrameVec3(cfg.duration/2, (2, 2, 2)),
                  ngl.AnimKeyFrameVec3(cfg.duration,   (1, 1, 1)))
        node = ngl.Scale(node, anim=ngl.AnimatedVec3(animkf))

    if translate:
        animkf = []
        tscale = 1. / float(len(coords) - 1) * cfg.duration
        for i, xy in enumerate(coords):
            pos = (xy[0] * .5, xy[1] * .5, 0)
            t = i * tscale
            animkf.append(ngl.AnimKeyFrameVec3(t, pos))
        node = ngl.Translate(node, anim=ngl.AnimatedVec3(animkf))

    return node


@scene()
def animated_uniform(cfg):
    '''Uniform mat4 animated with a transform chain'''
    m0 = cfg.medias[0]
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = ngl.Media(m0.filename)
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('matrix-transform'))
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    ts = ngl.Render(q, p)
    ts.update_frag_resources(tex0=t)

    scale_animkf = [ngl.AnimKeyFrameVec3(0, (1, 1, 1)),
                    ngl.AnimKeyFrameVec3(cfg.duration, (0.1, 0.1, 0.1), 'quartic_out')]
    s = ngl.Scale(ngl.Identity(), anim=ngl.AnimatedVec3(scale_animkf))

    rotate_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                     ngl.AnimKeyFrameFloat(cfg.duration, 360, 'exp_out')]
    r = ngl.Rotate(s, axis=(0, 0, 1), anim=ngl.AnimatedFloat(rotate_animkf))

    u = ngl.UniformMat4(transform=r)
    ts.update_frag_resources(matrix=u)

    return ts


@scene(rotate=scene.Bool())
def animated_camera(cfg, rotate=True):
    '''Animated camera around a scene'''
    g = ngl.Group()

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = ngl.Media(cfg.medias[0].filename)
    t = ngl.Texture2D(data_src=m)
    program = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    node = ngl.Render(q, program)
    node.update_frag_resources(tex0=t)
    g.add_children(node)

    translate = ngl.Translate(node, vector=(-0.6, 0.8, -1))
    g.add_children(translate)

    translate = ngl.Translate(node, vector=(0.6, 0.8, -1))
    g.add_children(translate)

    translate = ngl.Translate(node, vector=(-0.6, -0.5, -1))
    g.add_children(translate)

    translate = ngl.Translate(node, vector=(0.6, -0.5, -1))
    g.add_children(translate)

    g = ngl.GraphicConfig(g, depth_test=True)
    camera = ngl.Camera(g)
    camera.set_eye(0, 0, 2)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(0.1, 10.0)

    tr_animkf = [ngl.AnimKeyFrameVec3(0,  (0.0, 0.0, 0.0)),
                 ngl.AnimKeyFrameVec3(10, (0.0, 0.0, 3.0), 'exp_out')]
    node = ngl.Translate(ngl.Identity(), anim=ngl.AnimatedVec3(tr_animkf))

    if rotate:
        rot_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                      ngl.AnimKeyFrameFloat(cfg.duration, 360, 'exp_out')]
        node = ngl.Rotate(node, axis=(0, 1, 0), anim=ngl.AnimatedFloat(rot_animkf))

    camera.set_eye_transform(node)

    fov_animkf = [ngl.AnimKeyFrameFloat(0.5, 60.0),
                  ngl.AnimKeyFrameFloat(cfg.duration, 45.0, 'exp_out')]
    camera.set_fov_anim(ngl.AnimatedFloat(fov_animkf))

    return camera


@scene(dim=scene.Range(range=[1, 100]))
def animated_buffer(cfg, dim=50):
    '''Transform a random buffer content using animations'''
    cfg.duration = 5.

    random.seed(0)
    nb_kf = int(cfg.duration)
    buffers = [get_random_color_buffer(dim) for i in range(nb_kf)]
    random_animkf = []
    time_scale = cfg.duration / float(nb_kf)
    for i, buf in enumerate(buffers + [buffers[0]]):
        random_animkf.append(ngl.AnimKeyFrameBuffer(i*time_scale, buf))
    random_buffer = ngl.AnimatedBufferVec3(keyframes=random_animkf)
    random_tex = ngl.Texture2D(data_src=random_buffer, width=dim, height=dim)

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=cfg.get_vert('texture'), fragment=cfg.get_frag('texture'))
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(tex0=random_tex)
    return render


@scene()
def animated_circles(cfg):
    '''Simple cyclic circles animation'''
    group = ngl.Group()

    cfg.duration = 5.
    cfg.aspect_ratio = (1, 1)
    radius = 0.2
    n = 10
    step = 360. / n

    shape = ngl.Circle(radius=radius, npoints=128)
    prog = ngl.Program(vertex=cfg.get_vert('color'), fragment=cfg.get_frag('color'))
    render = ngl.Render(shape, prog)
    render.update_frag_resources(color=ngl.UniformVec4([1.0] * 4))

    for i in range(n):
        mid_time = cfg.duration / 2.0
        start_time = mid_time / (i + 2)
        end_time = cfg.duration - start_time

        scale_animkf = [
                ngl.AnimKeyFrameVec3(start_time, (0, 0, 0)),
                ngl.AnimKeyFrameVec3(mid_time, (1.0, 1.0, 1.0), 'exp_out'),
                ngl.AnimKeyFrameVec3(end_time, (0, 0, 0), 'exp_in'),
        ]

        angle = i * step
        rotate_animkf = [
                ngl.AnimKeyFrameFloat(start_time, 0),
                ngl.AnimKeyFrameFloat(mid_time, angle, 'exp_out'),
                ngl.AnimKeyFrameFloat(end_time, 0, 'exp_in'),
        ]

        tnode = render
        tnode = ngl.Scale(tnode, anim=ngl.AnimatedVec3(scale_animkf))
        tnode = ngl.Translate(tnode, vector=(1 - radius, 0, 0))
        tnode = ngl.Rotate(tnode, anim=ngl.AnimatedFloat(rotate_animkf))

        group.add_children(tnode)

    return group
