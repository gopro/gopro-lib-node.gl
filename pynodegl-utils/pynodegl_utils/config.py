#!/usr/bin/env python
#
# Copyright 2018 GoPro Inc.
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
import tempfile
import json
from PySide2 import QtCore


class Config(QtCore.QObject):

    CHOICES = {
        'aspect_ratio': [
            (16, 9),
            (16, 10),
            (4, 3),
            (1, 1),
            (9, 16),
        ],
        'samples': [0, 2, 4, 8],
        'framerate': [
            (8, 1), (12, 1), (15, 1),
            (24000, 1001), (24, 1),
            (25, 1),
            (30000, 1001), (30, 1),
            (50, 1),
            (60000, 1001), (60, 1),
        ],
        'log_level': [
            'verbose',
            'debug',
            'info',
            'warning',
            'error',
            'quiet',
        ],
        'backend': [
            'opengl',
            'opengles',
        ],
    }

    def __init__(self, module_pkgname):
        super().__init__()

        self._cfg = {
            'aspect_ratio': (16, 9),
            'samples': 0,
            'framerate': (60, 1),
            'log_level': 'info',
            'clear_color': (0.0, 0.0, 0.0, 1.0),
            'backend': 'opengl',

            # Export
            'export_width': 1280,
            'export_height': 720,
            'export_filename': op.join(tempfile.gettempdir(), 'ngl-export.mp4'),
            'export_extra_enc_args': '',

            # Medias
            'medias_list': [],
            'medias_last_dir': QtCore.QDir.currentPath(),
        }

        self._module_pkgname = module_pkgname
        self._needs_saving = False

        config_filepath = self._get_config_filepath()
        if op.exists(config_filepath):
            with open(config_filepath) as f:
                self._cfg.update(self._sanitized_config(json.load(f)))
        else:
            self._needs_saving = True

        self._config_timer = QtCore.QTimer()
        self._config_timer.setInterval(1000)  # every second
        self._config_timer.timeout.connect(self._check_config)
        self._config_timer.start()

    def _sanitized_config(self, cfg):
        out_cfg = {}
        for key, value in cfg.items():
            allowed_values = self.CHOICES.get(key)
            if isinstance(value, list):
                value = tuple(value)
            if allowed_values is None or value in allowed_values:
                out_cfg[key] = value
            else:
                print(f'warning: {value} not allowed for {key}')
        return out_cfg

    def _get_config_filepath(self):
        config_basedir = os.environ.get('XDG_DATA_HOME', op.expanduser('~/.local/share'))
        config_dir = op.join(config_basedir, 'node.gl')
        return op.join(config_dir, 'controller.json')

    @QtCore.Slot()
    def _check_config(self):
        if not self._needs_saving:
            return

        config_filepath = self._get_config_filepath()
        config_dir = op.dirname(config_filepath)
        if not op.exists(config_dir):
            os.makedirs(config_dir)

        config_file = open(config_filepath, 'w')
        json.dump(self._cfg, config_file, indent=4)
        config_file.close()

        self._needs_saving = False

    def get(self, key):
        ret = self._cfg.get(key)
        return tuple(ret) if isinstance(ret, list) else ret

    def _set_cfg(self, key, value):
        self._cfg[key] = value
        self._needs_saving = True

    @QtCore.Slot(list)
    def set_medias_list(self, medias_list):
        self._set_cfg('medias_list', medias_list)

    @QtCore.Slot(str)
    def set_medias_last_dir(self, last_dir):
        self._set_cfg('medias_last_dir', last_dir)

    @QtCore.Slot(int)
    def set_export_width(self, export_width):
        self._set_cfg('export_width', export_width)

    @QtCore.Slot(int)
    def set_export_height(self, export_height):
        self._set_cfg('export_height', export_height)

    @QtCore.Slot(str)
    def set_export_filename(self, filename):
        self._set_cfg('export_filename', filename)

    @QtCore.Slot(str)
    def set_export_extra_enc_args(self, extra_enc_args):
        self._set_cfg('export_extra_enc_args', extra_enc_args)

    @QtCore.Slot(tuple)
    def set_aspect_ratio(self, ar):
        self._set_cfg('aspect_ratio', ar)

    @QtCore.Slot(tuple)
    def set_frame_rate(self, fr):
        self._set_cfg('framerate', fr)

    @QtCore.Slot(int)
    def set_samples(self, samples):
        self._set_cfg('samples', samples)

    @QtCore.Slot(tuple)
    def set_clear_color(self, color):
        self._set_cfg('clear_color', color)

    @QtCore.Slot(str)
    def set_log_level(self, level):
        self._set_cfg('log_level', level)

    @QtCore.Slot(str)
    def set_backend(self, backend):
        self._set_cfg('backend', backend)

    def geometry_changed(self, geometry):
        self._set_cfg('geometry', geometry)

    @QtCore.Slot(str, str)
    def scene_changed(self, module_name, scene_name):
        self._set_cfg('pkg', self._module_pkgname)
        self._set_cfg('module', module_name)
        self._set_cfg('scene',  scene_name)
