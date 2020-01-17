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

import difflib
import pynodegl as ngl


class CompareBase(object):

    @staticmethod
    def serialize(data):
        return data

    @staticmethod
    def deserialize(data):
        return data

    def get_out_data(self):
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

    def render_frames(self):
        # We make sure the lists of medias is explicitely empty. If we don't a
        # jobbed make on the tests will attempt concurrent generations of a
        # default ngl-media.mp4.
        ret = self._scene_func(idict=dict(medias=[]), **self._scene_kwargs)
        width, height = self._width, self._height
        duration = ret['duration']
        scene = ret['scene']

        capture_buffer = bytearray(width * height * 4)
        viewer = ngl.Viewer()
        assert viewer.configure(offscreen=1, width=width, height=height,
                                clear_color=self._clear_color,
                                capture_buffer=capture_buffer) == 0
        timescale = duration / float(self._nb_keyframes)

        if self._scene_wrap:
            scene = self._scene_wrap(scene)

        if self._exercise_dot:
            assert scene.dot()

        if self._exercise_serialization:
            scene_str = scene.serialize()
            viewer.set_scene_from_string(scene_str)
        else:
            viewer.set_scene(scene)

        for t_id in range(self._nb_keyframes):
            if self._keyframes_callback:
                self._keyframes_callback(t_id)
            viewer.draw(t_id * timescale)

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
