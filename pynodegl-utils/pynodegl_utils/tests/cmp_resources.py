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
import csv
import tempfile
import pynodegl as ngl

from .cmp import CompareSceneBase, get_test_decorator


_COLS = (
     'Textures memory',
     'Buffers count',
     'Buffers total',
     'Blocks count',
     'Blocks total',
     'Medias count',
     'Medias total',
     'Textures count',
     'Textures total',
     'Computes',
     'GraphicCfgs',
     'Renders',
     'RTTs',
)

class _CompareResources(CompareSceneBase):

    def __init__(self, scene_func, columns=_COLS, **kwargs):
        super().__init__(scene_func, width=320, height=240, **kwargs)

        # We can't use NamedTemporaryFile because we may not be able to open it
        # twice on some systems
        fd, self._csvfile = tempfile.mkstemp(suffix='.csv', prefix='ngl-test-resources-')
        os.close(fd)

        self._columns = columns
        self._hud = 1
        self._hud_export_filename = self._csvfile

    def get_out_data(self, debug=False, debug_func=None):
        for frame in self.render_frames():
            pass

        # filter columns
        with open(self._csvfile) as csvfile:
            reader = csv.DictReader(csvfile)
            data = [self._columns]
            for row in reader:
                data.append([row[k] for k in self._columns])

        # rely on base string diff
        ret = ''
        for row in data:
            ret += ','.join(row) + '\n'

        os.remove(self._csvfile)

        return ret


test_resources = get_test_decorator(_CompareResources)
