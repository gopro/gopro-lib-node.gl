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

from .cmp import CompareSceneBase, get_test_decorator, get_temp_dir
from PIL import Image
import os

class _CompareCuePoints(CompareSceneBase):

    def __init__(self, scene_func, points, width=128, height=128, tolerance=0, **kwargs):
        super().__init__(scene_func, width=width, height=height, **kwargs)
        self._points = points
        self._tolerance = tolerance

    @staticmethod
    def serialize(data):
        ret = ''
        for color_points in data:
            color_strings = ['{}:{:08X}'.format(point_name, color) for point_name, color in sorted(color_points.items())]
            ret += ' '.join(color_strings) + '\n'
        return ret

    @staticmethod
    def deserialize(data):
        ret = []
        for line in data.splitlines():
            color_points = {}
            for color_kv in line.split():
                key, value = color_kv.split(':')
                color_points[key] = int(value, 16)
            ret.append(color_points)
        return ret

    @staticmethod
    def _pos_to_px(pos, width, height):
        x = int(round((pos[0] + 1.) / 2. * width))
        y = height - 1 - int(round((pos[1] + 1.) / 2. * height))
        x = min(max(x, 0), width - 1)
        y = min(max(y, 0), height - 1)
        return [x, y]

    def get_out_data(self, debug = False, debug_func = ''):
        cpoints = []
        debug_index = 0
        for (width, height, capture_buffer) in self.render_frames():
            if debug:
                _MODE = "RGBA"
                img = Image.frombuffer(_MODE, (width, height), capture_buffer, 'raw', _MODE, 0, 1)
                filename = os.path.normpath(get_temp_dir() + f"/{debug_func}_{debug_index}.png")
                print("writing to file: " + filename)
                img.save(filename)
                debug_index += 1
            frame_cpoints = {}
            for point_name, (x, y) in self._points.items():
                pix_x, pix_y = self._pos_to_px((x, y), width, height)
                pos = (pix_y * width + pix_x) * 4
                c = capture_buffer[pos:pos + 4]
                frame_cpoints[point_name] = c[0]<<24 | c[1]<<16 | c[2]<<8 | c[3]
            cpoints.append(frame_cpoints)
        return cpoints

    @staticmethod
    def _color_diff(c0, c1):
        r0, g0, b0, a0 = c0 >> 24, c0 >> 16 & 0xff, c0 >> 8 & 0xff, c0 & 0xff
        r1, g1, b1, a1 = c1 >> 24, c1 >> 16 & 0xff, c1 >> 8 & 0xff, c1 & 0xff
        return abs(r0 - r1), abs(g0 - g1), abs(b0 - b1), abs(a0 - a1)

    def compare_data(self, test_name, ref_data, out_data):
        err = []
        for frame, (ref_cpoints, out_cpoints) in enumerate(zip(ref_data, out_data)):
            ref_keys = sorted(ref_cpoints.keys())
            out_keys = sorted(out_cpoints.keys())
            assert ref_keys == out_keys
            for key in ref_keys:
                refc = ref_cpoints[key]
                outc = out_cpoints[key]
                diff = self._color_diff(refc, outc)
                if any(d > self._tolerance for d in diff):
                    diff_str = ','.join('%d' % d for d in diff)
                    tolr_str = ','.join('%d' % t for t in [self._tolerance] * 4)
                    err.append('{} frame #{} point {}: Diff too high ({} > {}) between ref:#{:08X} and out:#{:08X}'.format(
                               test_name, frame, key, diff_str, tolr_str, refc, outc))
        return err


test_cuepoints = get_test_decorator(_CompareCuePoints)
