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

import os.path as op
import tempfile
import subprocess
from PySide2 import QtCore, QtGui, QtWidgets, QtSvg

from .seekbar import Seekbar

from pynodegl_utils import clock
from pynodegl_utils import misc

import pynodegl as ngl


class _SVGGraphView(QtWidgets.QGraphicsView):

    def __init__(self):
        super().__init__()
        self.setDragMode(QtWidgets.QGraphicsView.ScrollHandDrag)
        self._zoom_level = 0

    def wheelEvent(self, event):
        y_degrees = event.angleDelta().y() / 8.0
        self._zoom_level += y_degrees / 15.0  # in the most common mouse step unit
        m = QtGui.QTransform()
        factor = 1.25 ** self._zoom_level
        m.scale(factor, factor)
        self.setTransform(m)


class GraphView(QtWidgets.QWidget):

    def __init__(self, get_scene_func, config):
        super().__init__()

        self._get_scene_func = get_scene_func
        self._framerate = config.get('framerate')
        self._duration = 0.0
        self._samples = config.get('samples')
        self._ctx = None

        self._save_btn = QtWidgets.QPushButton('Save image')

        self._scene = QtWidgets.QGraphicsScene()
        self._view = _SVGGraphView()
        self._view.setScene(self._scene)

        self._seek_chkbox = QtWidgets.QCheckBox('Show graph at a given time')
        self._seekbar = Seekbar(config, stop_button=False)
        self._seekbar.setEnabled(False)

        hbox = QtWidgets.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self._save_btn)

        graph_layout = QtWidgets.QVBoxLayout(self)
        graph_layout.addWidget(self._seek_chkbox)
        graph_layout.addWidget(self._seekbar)
        graph_layout.addWidget(self._view)
        graph_layout.addLayout(hbox)

        self._save_btn.clicked.connect(self._save_to_file)
        self._seek_chkbox.stateChanged.connect(self._seek_check_changed)

        self._seekbar.play.connect(self._play)
        self._seekbar.pause.connect(self._pause)
        self._seekbar.seek.connect(self._seek)
        self._seekbar.step.connect(self._step)

        self._timer = QtCore.QTimer()
        self._timer.timeout.connect(self._update)

        self._clock = clock.Clock(self._framerate, 0.0)

    @QtCore.Slot()
    def _play(self):
        self._timer.start()
        self._clock.start()
        self._seekbar.set_play_state()

    @QtCore.Slot()
    def _pause(self):
        self._timer.stop()
        self._clock.stop()
        self._seekbar.set_pause_state()

    @QtCore.Slot(float)
    def _seek(self, time):
        self._clock.set_playback_time(time)
        self._update()

    @QtCore.Slot(int)
    def _step(self, step):
        self._clock.step_playback_index(step)
        self._pause()
        self._update()

    @QtCore.Slot()
    def _update(self):
        if not self._ctx:
            return
        frame_index, frame_time = self._clock.get_playback_time_info()
        dot_scene = self._ctx.dot(frame_time)
        if dot_scene:
            self._update_graph(dot_scene)
            self._seekbar.set_frame_time(frame_index, frame_time)

    @QtCore.Slot()
    def _save_to_file(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        rect_size = self._scene.sceneRect().size()
        img_size = QtCore.QSize(int(rect_size.width()), int(rect_size.height()))
        img = QtGui.QImage(img_size, QtGui.QImage.Format_ARGB32_Premultiplied)
        painter = QtGui.QPainter(img)
        self._scene.render(painter)
        img.save(filenames[0])
        painter.end()

    def enter(self):
        cfg_overrides = {}
        if not self._seek_chkbox.isChecked():
            cfg_overrides['fmt'] = 'dot'

        cfg = self._get_scene_func(**cfg_overrides)
        if not cfg:
            return

        self._seekbar.set_scene_metadata(cfg)

        if self._seek_chkbox.isChecked():
            self._init_ctx(cfg['backend'])
            self._framerate = cfg['framerate']
            self._duration = cfg['duration']
            self._ctx.set_scene_from_string(cfg['scene'])
            self._clock.configure(self._framerate, self._duration)
            self._timer.setInterval(self._framerate[1] * 1000 / self._framerate[0])  # in milliseconds
            self._update()
        else:
            self._reset_ctx()
            dot_scene = cfg['scene']
            self._update_graph(dot_scene)

    def leave(self):
        self._pause()

    def _reset_ctx(self):
        if not self._ctx:
            return
        del self._ctx
        self._ctx = None

    def _init_ctx(self, rendering_backend):
        if self._ctx:
            return
        self._ctx = ngl.Context()
        self._ctx.configure(
            backend=misc.get_backend(rendering_backend),
            offscreen=1,
            width=16,
            height=16
        )

    def _update_graph(self, dot_scene):
        basename = op.join(tempfile.gettempdir(), 'ngl_scene.')
        dotfile = basename + 'dot'
        svgfile = basename + 'svg'
        with open(dotfile, 'w') as f:
            f.write(dot_scene.decode('ascii'))
        try:
            subprocess.call(['dot', '-Tsvg', dotfile, '-o' + svgfile])
        except OSError as e:
            QtWidgets.QMessageBox.critical(self, 'Graphviz error',
                                           'Error while executing dot (Graphviz): %s' % e.strerror,
                                           QtWidgets.QMessageBox.Ok)
            return

        item = QtSvg.QGraphicsSvgItem(svgfile)
        self._scene.clear()
        self._scene.addItem(item)
        self._scene.setSceneRect(item.boundingRect())

    @QtCore.Slot(int)
    def _seek_check_changed(self, state):
        if state:
            self._seekbar.setEnabled(True)
        else:
            self._seekbar.setEnabled(False)
        self.enter()
