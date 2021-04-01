#
# Copyright 2017 GoPro Inc.
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
import subprocess

import pynodegl as ngl
from PySide6 import QtGui, QtCore

from .com import query_inplace
from .misc import get_backend, get_viewport, get_nodegl_tempdir


class Exporter(QtCore.QThread):

    progressed = QtCore.Signal(int)
    failed = QtCore.Signal()
    export_finished = QtCore.Signal()

    def __init__(self, get_scene_func, filename, w, h, extra_enc_args=None, time=None):
        super().__init__()
        self._get_scene_func = get_scene_func
        self._filename = filename
        self._width = w
        self._height = h
        self._extra_enc_args = extra_enc_args if extra_enc_args is not None else []
        self._time = time
        self._cancelled = False

    def run(self):
        filename, width, height = self._filename, self._width, self._height

        if filename.endswith('gif'):
            palette_filename = op.join(get_nodegl_tempdir(), 'palette.png')
            pass1_args = ['-vf', 'palettegen']
            pass2_args = self._extra_enc_args + ['-i', palette_filename, '-lavfi', 'paletteuse']
            ok = self._export(palette_filename, width, height, pass1_args)
            if not ok:
                return
            ok = self._export(filename, width, height, pass2_args)
        else:
            ok = self._export(filename, width, height, self._extra_enc_args)
        if ok:
            self.export_finished.emit()

    def _export(self, filename, width, height, extra_enc_args=None):
        fd_r, fd_w = os.pipe()

        cfg = self._get_scene_func()
        if not cfg:
            self.failed.emit()
            return False

        fps = cfg['framerate']
        duration = cfg['duration']
        samples = cfg['samples']

        cmd = ['ffmpeg', '-r', '%d/%d' % fps,
               '-nostats', '-nostdin',
               '-f', 'rawvideo',
               '-video_size', '%dx%d' % (width, height),
               '-pixel_format', 'rgba',
               '-i', 'pipe:%d' % fd_r]
        if extra_enc_args:
            cmd += extra_enc_args
        cmd += ['-y', filename]

        reader = subprocess.Popen(cmd, pass_fds=(fd_r,))
        os.close(fd_r)

        capture_buffer = bytearray(width * height * 4)

        # node.gl context
        ctx = ngl.Context()
        ctx.configure(
            platform=ngl.PLATFORM_AUTO,
            backend=get_backend(cfg['backend']),
            offscreen=1,
            width=width,
            height=height,
            viewport=get_viewport(width, height, cfg['aspect_ratio']),
            samples=samples,
            clear_color=cfg['clear_color'],
            capture_buffer=capture_buffer,
        )
        ctx.set_scene_from_string(cfg['scene'])

        if self._time is not None:
            ctx.draw(self._time)
            os.write(fd_w, capture_buffer)
            self.progressed.emit(100)
        else:
            # Draw every frame
            nb_frame = int(duration * fps[0] / fps[1])
            for i in range(nb_frame):
                if self._cancelled:
                    break
                time = i * fps[1] / float(fps[0])
                ctx.draw(time)
                os.write(fd_w, capture_buffer)
                self.progressed.emit(i*100 / nb_frame)
            self.progressed.emit(100)

        os.close(fd_w)
        reader.wait()
        return True

    def cancel(self):
        self._cancelled = True


def test_export():
    import sys

    def _get_scene(**cfg_overrides):
        cfg = {
            'pkg': 'pynodegl_utils.examples',
            'scene': ('misc', 'triangle'),
            'duration': 5,
        }
        cfg.update(cfg_overrides)

        ret = query_inplace(query='scene', **cfg)
        if 'error' in ret:
            print(ret['error'])
            return None
        return ret

    def print_progress(progress):
        sys.stdout.write('\r%d%%' % progress)
        sys.stdout.flush()
        if progress == 100:
            sys.stdout.write('\n')

    if len(sys.argv) != 2:
        print('Usage: %s <outfile>' % sys.argv[0])
        sys.exit(0)

    filename = sys.argv[1]
    app = QtGui.QGuiApplication(sys.argv)

    exporter = Exporter(_get_scene, filename, 320, 240)
    exporter.progressed.connect(print_progress)
    exporter.start()
    exporter.wait()


if __name__ == '__main__':
    test_export()
