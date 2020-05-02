import os.path as op
import array
import pynodegl as ngl
from pynodegl_utils.misc import scene


def _load_model(fp):
    index = 0

    vertices = []
    uvs = []
    normals = []

    vertex_indices = []
    uv_indices = []
    normal_indices = []

    indexed_vertices = array.array('f')
    indexed_uvs = array.array('f')
    indexed_normals = array.array('f')

    while True:
        line = fp.readline()
        if line == '':
            break
        line = line.strip()
        fragment = line.split(' ')
        if len(fragment) > 0:
            if fragment[0] == 'v':
                vertices.append((
                    float(fragment[1]),
                    float(fragment[2]),
                    float(fragment[3])
                ))
            elif fragment[0] == 'vt':
                uvs.append((
                    float(fragment[1]),
                    float(fragment[2])
                ))
            elif fragment[0] == 'vn':
                normals.append((
                    float(fragment[1]),
                    float(fragment[2]),
                    float(fragment[3])
                ))
            elif fragment[0] == 'f':
                for i in range(1, 4):
                    indices = fragment[i].split('/')
                    vertex_indices.append(int(indices[0]))
                    uv_indices.append(int(indices[1]))
                    normal_indices.append(int(indices[2]))

    for index in zip(vertex_indices, uv_indices, normal_indices):
        vertex_index, uv_index, normal_index = index

        vertex = vertices[vertex_index - 1]
        uv = uvs[uv_index - 1]
        normal = normals[normal_index - 1]

        indexed_vertices.extend(vertex)
        indexed_uvs.extend(uv)
        indexed_normals.extend(normal)

    return indexed_vertices, indexed_uvs, indexed_normals


@scene(model=scene.File(filter='Object files (*.obj)'))
def obj(cfg, n=0.5, model=None):
    '''Load and display a cube object (generated with Blender)'''

    if model is None:
        model = op.join(op.dirname(__file__), 'data', 'model.obj')

    with open(model) as fp:
        vertices_data, uvs_data, normals_data = _load_model(fp)

    vertices = ngl.BufferVec3(data=vertices_data)
    texcoords = ngl.BufferVec2(data=uvs_data)
    normals = ngl.BufferVec3(data=normals_data)

    q = ngl.Geometry(vertices, texcoords, normals)
    m = ngl.Media(cfg.medias[0].filename)
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program(vertex=cfg.get_vert('tex-tint-normals'), fragment=cfg.get_frag('tex-tint-normals'))
    render = ngl.Render(q, p)
    render.update_frag_resources(tex0=t)
    render = ngl.GraphicConfig(render, depth_test=True)

    animkf = [ngl.AnimKeyFrameFloat(0, 0),
              ngl.AnimKeyFrameFloat(cfg.duration, 360*2)]
    rot = ngl.Rotate(render, label='roty', axis=(0, 1, 0), anim=ngl.AnimatedFloat(animkf))

    camera = ngl.Camera(rot)
    camera.set_eye(2.0, 2.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)

    return camera


@scene(stl=scene.File(filter='STL files (*.stl)'),
       scale=scene.Range(range=[0.01, 10], unit_base=100))
def stl(cfg, stl=None, scale=.8):
    '''Load and display a sphere generated with OpenSCAD'''

    if stl is None:
        # generated with: echo 'sphere($fn=15);'>sphere.scad; openscad sphere.scad -o sphere.stl
        stl = op.join(op.dirname(__file__), 'data', 'sphere.stl')

    normals_data  = array.array('f')
    vertices_data = array.array('f')
    solid_label = None
    normal = None

    with open(stl) as fp:
        for line in fp.readlines():
            line = line.strip()
            if line.startswith('solid'):
                solid_label = line.split(None, 1)[1]
            elif line.startswith('facet normal'):
                _, _, normal = line.split(None, 2)
                normal = [float(f) for f in normal.split()]
            elif normal and line.startswith('vertex'):
                _, vertex = line.split(None, 1)
                vertex = [float(f) for f in vertex.split()]
                normals_data.extend(normal)
                vertices_data.extend(vertex)

    vertices = ngl.BufferVec3(data=vertices_data)
    normals  = ngl.BufferVec3(data=normals_data)

    g = ngl.Geometry(vertices=vertices, normals=normals)
    p = ngl.Program(vertex=cfg.get_vert('colored-normals'), fragment=cfg.get_frag('colored-normals'))
    solid = ngl.Render(g, p, label=solid_label)
    solid = ngl.GraphicConfig(solid, depth_test=True)

    solid = ngl.Scale(solid, [scale] * 3)

    for i in range(3):
        rot_animkf = ngl.AnimatedFloat([ngl.AnimKeyFrameFloat(0, 0),
                                        ngl.AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))])
        axis = [int(i == x) for x in range(3)]
        solid = ngl.Rotate(solid, axis=axis, anim=rot_animkf)

    camera = ngl.Camera(solid)
    camera.set_eye(2.0, 2.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)

    return camera
