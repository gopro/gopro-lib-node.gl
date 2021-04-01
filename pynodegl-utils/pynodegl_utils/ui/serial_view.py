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

from PySide6 import QtCore, QtGui, QtWidgets


class SerialView(QtWidgets.QWidget):

    def __init__(self, get_scene_func):
        super().__init__()

        self._get_scene_func = get_scene_func

        self._save_btn = QtWidgets.QPushButton('Save to file')
        self._graph_lbl = QtWidgets.QLabel()
        self._text = QtWidgets.QPlainTextEdit()
        self._text.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
        self._text.setReadOnly(True)

        hbox = QtWidgets.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self._save_btn)

        serial_layout = QtWidgets.QVBoxLayout(self)
        serial_layout.addWidget(self._text)
        serial_layout.addLayout(hbox)

        self._save_btn.clicked.connect(self._save_to_file)

    @QtCore.Slot()
    def _save_to_file(self):
        data = self._text.toPlainText()
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        with open(filenames[0], 'w') as f:
            f.write(data)

    def enter(self):
        cfg = self._get_scene_func()
        if not cfg:
            return
        self._text.setPlainText(cfg['scene'].decode('ascii'))
