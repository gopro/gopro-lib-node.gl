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

import math
from fractions import Fraction
from PySide6 import QtCore, QtGui, QtWidgets


class Seekbar(QtWidgets.QWidget):

    seek = QtCore.Signal(float)
    step = QtCore.Signal(int)

    SLIDER_TIMEBASE = 1000
    SLIDER_TIMESCALE = 1. / SLIDER_TIMEBASE

    def __init__(self, config):
        super().__init__()

        self._slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self._time_lbl = QtWidgets.QLabel()
        self._time_lbl.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))

        fw_btn = QtWidgets.QToolButton()
        fw_btn.setText('>')
        bw_btn = QtWidgets.QToolButton()
        bw_btn.setText('<')

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(bw_btn)
        layout.addWidget(fw_btn)
        layout.addWidget(self._slider)
        layout.addWidget(self._time_lbl)

        self._frame_index = 0
        self._scene_duration = 0
        self._framerate = Fraction(*config.get('framerate'))

        self._slider.sliderMoved.connect(self._slider_moved)
        self._slider.sliderPressed.connect(self._slider_pressed)
        self._slider.sliderReleased.connect(self._slider_released)
        self._slider_dragged = False

        fw_btn.clicked.connect(self._step_fw)
        bw_btn.clicked.connect(self._step_bw)

    @QtCore.Slot(int)
    def _slider_moved(self, value):  # only user move
        if not self._scene_duration:
            return
        self.seek.emit(value * self.SLIDER_TIMESCALE)

    @QtCore.Slot()
    def _slider_pressed(self):
        self._slider_dragged = True

    @QtCore.Slot()
    def _slider_released(self):
        self._slider_dragged = False
        self._refresh()

    @QtCore.Slot()
    def _step_fw(self):
        self.step.emit(1)

    @QtCore.Slot()
    def _step_bw(self):
        self.step.emit(-1)

    def _get_time_lbl_text(self, frame_index, frame_time):
        cur_time = '%02d:%02d' % divmod(frame_time, 60)
        duration = '%02d:%02d' % divmod(self._scene_duration, 60)
        return '%s / %s (%d @ %.4gHz)' % (cur_time, duration, frame_index, self._framerate)

    def _adjust_time_label_size(self):
        # Make the time label flexible again
        self._time_lbl.setMinimumSize(0, 0)
        self._time_lbl.setMaximumSize(0xffffff, 0xffffff)

        # Set the label to its largest possible content (last frame)
        last_frame_index = int(math.ceil(self._scene_duration * self._framerate))
        text = self._get_time_lbl_text(last_frame_index, self._scene_duration)
        self._time_lbl.setText(text)

        # Probe the occupied size and make it fixed for the current scene
        hint = self._time_lbl.sizeHint()
        self._time_lbl.setFixedSize(hint)

    @QtCore.Slot(dict)
    def set_scene_metadata(self, cfg):
        self._scene_duration = cfg['duration']
        self._framerate = Fraction(*cfg['framerate'])
        self._slider.setRange(0, self._scene_duration * self.SLIDER_TIMEBASE)
        self._adjust_time_label_size()
        self._refresh()

    @QtCore.Slot(int, float)
    def set_frame_time(self, frame_index, frame_time):
        self._frame_index = frame_index
        self._refresh()

    def _refresh(self):
        t = self._frame_index / self._framerate
        text = self._get_time_lbl_text(self._frame_index, t)
        self._time_lbl.setText(text)
        if not self._slider_dragged:
            self._slider.setValue(int(t * self.SLIDER_TIMEBASE))
