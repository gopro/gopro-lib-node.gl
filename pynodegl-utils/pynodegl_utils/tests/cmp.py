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

import os
import os.path as op
import difflib
import pynodegl as ngl
from pynodegl_utils.misc import get_backend
import tempfile


class CompareBase:

    @staticmethod
    def serialize(data):
        return data

    @staticmethod
    def deserialize(data):
        return data

    def get_out_data(self, debug=False, debug_func=None):
        raise NotImplementedError

    def compare_data(self, test_name, ref_data, out_data):
        ref_data = self.serialize(ref_data)
        out_data = self.serialize(out_data)

        err = []
        if ref_data != out_data:
            ref_data = ref_data.splitlines(True)
            out_data = out_data.splitlines(True)
            diff = ''.join(difflib.unified_diff(ref_data, out_data, fromfile=test_name + '-ref', tofile=test_name + '-out', n=10))
            err.append('{} fail:\n{}'.format(test_name, diff))
        return err

    @staticmethod
    def dump_image(img, dump_index, func_name=None):
        filename = op.join(get_temp_dir(), f'{func_name}_{dump_index:03}.png')
        print(f'Dumping output image to {filename}')
        img.save(filename)
        dump_index += 1


class CompareSceneBase(CompareBase):

    def __init__(self,
                 scene_func,
                 width=1280,
                 height=800,
                 nb_keyframes=1,
                 keyframes_callback=None,
                 clear_color=(0.0, 0.0, 0.0, 1.0),
                 exercise_serialization=True,
                 exercise_dot=True,
                 scene_wrap=None,
                 samples=0,
                 **scene_kwargs):
        self._width = width
        self._height = height
        self._nb_keyframes = nb_keyframes
        self._keyframes_callback = keyframes_callback
        self._clear_color = clear_color
        self._scene_func = scene_func
        self._scene_kwargs = scene_kwargs
        self._exercise_serialization = exercise_serialization
        self._exercise_dot = exercise_dot
        self._scene_wrap = scene_wrap
        self._samples = samples
        self._hud = 0
        self._hud_export_filename = None

    def render_frames(self):
        # We make sure the lists of medias is explicitely empty. If we don't a
        # jobbed make on the tests will attempt concurrent generations of a
        # default ngl-media.mp4.
        idict = dict(medias=[])

        backend = os.environ.get('BACKEND')
        if backend:
            idict['backend'] = backend

        ret = self._scene_func(idict=idict, **self._scene_kwargs)
        width, height = self._width, self._height
        duration = ret['duration']
        scene = ret['scene']

        capture_buffer = bytearray(width * height * 4)
        ctx = ngl.Context()
        assert ctx.configure(offscreen=1, width=width, height=height,
                             backend=get_backend(backend) if backend else ngl.BACKEND_AUTO,
                             samples=self._samples,
                             clear_color=self._clear_color,
                             capture_buffer=capture_buffer,
                             hud=self._hud,
                             hud_export_filename=self._hud_export_filename) == 0
        timescale = duration / float(self._nb_keyframes)

        if self._scene_wrap:
            scene = self._scene_wrap(scene)

        if self._exercise_dot:
            assert scene.dot()

        if self._exercise_serialization:
            scene_str = scene.serialize()
            ctx.set_scene_from_string(scene_str)
        else:
            ctx.set_scene(scene)

        for t_id in range(self._nb_keyframes):
            if self._keyframes_callback:
                self._keyframes_callback(t_id)
            ctx.draw(t_id * timescale)

            yield (width, height, capture_buffer)

            if not self._exercise_serialization and self._exercise_dot:
                scene.dot()


def get_test_decorator(cls):
    def test_func(*args, **kwargs):
        def test_decorator(user_func):
            # Inject a tester for ngl-test
            user_func.tester = cls(user_func, *args, **kwargs)
            return user_func
        return test_decorator
    return test_func


def get_temp_dir():
    dir = op.join(tempfile.gettempdir(), 'nodegl/tests')
    if not op.exists(dir):
        os.makedirs(dir)
    return dir
