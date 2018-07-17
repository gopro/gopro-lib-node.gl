import array
import os.path as op
import platform
import math
import inspect
import json
import subprocess
import pynodegl as ngl


def scene(**widgets_specs):
    def real_decorator(scene_func):
        def func_wrapper(idict=None, **extra_args):
            if idict is None:
                idict = {}
            scene_cfg = SceneCfg(**idict)
            scene = scene_func(scene_cfg, **extra_args)
            odict = scene_cfg.as_dict()
            odict['scene'] = scene
            return odict

        final_specs = []

        # Construct a arg -> default dict
        func_specs = inspect.getargspec(scene_func)
        if func_specs.defaults:
            nb_optionnals = len(func_specs.defaults)
            for i, key in enumerate(func_specs.args[-nb_optionnals:]):
                # Create a copy of the widgets specifications with the defaults value
                if key in widgets_specs:
                    specs = widgets_specs[key].copy()
                    specs['name'] = key
                    specs['default'] = func_specs.defaults[i]
                    final_specs.append(specs)

        # Transfers the widgets specifications to the UI.
        # We could use the return value but it's better if the user can still
        # call its decorated scene function transparently inside his own code
        # without getting garbage along the return value.
        func_wrapper.widgets_specs = final_specs

        # Flag the scene as a scene function so it's registered in the UI.
        func_wrapper.iam_a_ngl_scene_func = True

        return func_wrapper

    return real_decorator


def get_vk_shader(name, stype, shader_path=None):
    if shader_path is None:
        shader_path = op.join(op.dirname(__file__), 'examples', 'shaders')
    src_path = op.join(shader_path, '%s.%s' % (name, stype))
    dst_path = op.join(shader_path, '%s-%s.spv' % (name, stype))
    print 'SPV: %s -> %s' % (src_path, dst_path)
    ret = subprocess.call(['glslangValidator', '-V', '-o', dst_path, src_path])
    assert ret == 0
    return array.array('b', open(dst_path).read())


def get_vk_frag(name, shader_path=None):
    return get_vk_shader(name, 'frag', shader_path)


def get_vk_vert(name, shader_path=None):
    return get_vk_shader(name, 'vert', shader_path)


def get_shader(filename, shader_path=None):
    if shader_path is None:
        shader_path = op.join(op.dirname(__file__), 'examples', 'shaders')
    return open(op.join(shader_path, filename)).read()


def get_frag(name, shader_path=None):
    return get_shader(name + '.frag', shader_path)


def get_vert(name, shader_path=None):
    return get_shader(name + '.vert', shader_path)


def get_comp(name, shader_path=None):
    return get_shader(name + '.comp', shader_path)


class Media:

    def __init__(self, filename):
        self._filename = filename
        self._set_media_dimensions()

    def _set_media_dimensions(self):
        data = subprocess.check_output(['ffprobe', '-v', '0',
                                        '-select_streams', 'v:0',
                                        '-of', 'json',
                                        '-show_streams', '-show_format',
                                        self._filename])
        data = json.loads(data)
        st = data['streams'][0]
        self._dimensions = (st['width'], st['height'])
        self._duration = float(data['format']['duration'])
        self._framerate = tuple(int(x) for x in st['avg_frame_rate'].split('/'))

    @property
    def filename(self):
        return self._filename

    @property
    def width(self):
        return self._dimensions[0]

    @property
    def height(self):
        return self._dimensions[1]

    @property
    def dimensions(self):
        return self._dimensions

    @property
    def duration(self):
        return self._duration

    @property
    def framerate(self):
        return self._framerate

    @property
    def framerate_float(self):
        return self._framerate[0] / float(self._framerate[1])


class SceneCfg:

    _DEFAULT_MEDIA_FILE = '/tmp/ngl-media.mp4'
    _DEFAULT_FIELDS = {
        'aspect_ratio': (16, 9),
        'duration': 30.0,
        'framerate': (60, 1),
        'backend': 'gl',
        'samples': 0,
        'system': platform.system(),
        'files': [],
        'medias': None,
        'clear_color': (0.0, 0.0, 0.0, 1.0),
    }

    def __init__(self, **kwargs):
        for field, def_val in self._DEFAULT_FIELDS.iteritems():
            val = kwargs.get(field, def_val)
            setattr(self, field, val)

        if self.medias is None:
            media_file = self._DEFAULT_MEDIA_FILE
            if not op.exists(self._DEFAULT_MEDIA_FILE):
                ret = subprocess.call(['ffmpeg', '-nostdin', '-nostats', '-f', 'lavfi', '-i',
                                       'testsrc2=d=%d:r=%d/%d' % (int(math.ceil(self.duration)),
                                                                  self.framerate[0], self.framerate[1]),
                                       media_file])
                if ret:
                    raise Exception("Unable to create a media file using ffmpeg (ret=%d)" % ret)
            self.medias = [Media(media_file)]

    @property
    def aspect_ratio_float(self):
        return self.aspect_ratio[0] / float(self.aspect_ratio[1])

    def as_dict(self):
        odict = {}
        for field in self._DEFAULT_FIELDS.keys():
            odict[field] = getattr(self, field)
        return odict


def get_viewport(width, height, aspect_ratio):
    view_width = width
    view_height = width * aspect_ratio[1] / aspect_ratio[0]
    if view_height > height:
        view_height = height
        view_width = height * aspect_ratio[0] / aspect_ratio[1]
    view_x = (width - view_width) // 2
    view_y = (height - view_height) // 2
    return (view_x, view_y, view_width, view_height)


def get_backend(backend):
    backend_map = {
        'gl': ngl.BACKEND_OPENGL,
        'gles': ngl.BACKEND_OPENGLES,
        'vk': ngl.BACKEND_VULKAN,
    }
    return backend_map[backend]
