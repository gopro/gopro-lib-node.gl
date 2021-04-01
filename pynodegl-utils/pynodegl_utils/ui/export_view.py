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

from PySide6 import QtCore, QtWidgets
from fractions import Fraction

from pynodegl_utils.export import Exporter


class ExportView(QtWidgets.QWidget):

    def __init__(self, get_scene_func, config):
        super().__init__()

        self._get_scene_func = get_scene_func
        self._framerate = config.get('framerate')
        self._aspect_ratio = config.get('aspect_ratio')

        self._ofile_text = QtWidgets.QLineEdit(config.get('export_filename'))
        ofile_btn = QtWidgets.QPushButton('Browse')

        file_box = QtWidgets.QHBoxLayout()
        file_box.addWidget(self._ofile_text)
        file_box.addWidget(ofile_btn)

        self._spinbox_width = QtWidgets.QSpinBox()
        self._spinbox_width.setRange(1, 0xffff)
        self._spinbox_width.setValue(config.get('export_width'))

        self._spinbox_height = QtWidgets.QSpinBox()
        self._spinbox_height.setRange(1, 0xffff)
        self._spinbox_height.setValue(config.get('export_height'))

        self._encopts_text = QtWidgets.QLineEdit()
        self._encopts_text.setText(config.get('export_extra_enc_args'))

        self._export_btn = QtWidgets.QPushButton('Export')
        btn_hbox = QtWidgets.QHBoxLayout()
        btn_hbox.addStretch()
        btn_hbox.addWidget(self._export_btn)

        self._warning_label = QtWidgets.QLabel()
        self._warning_label.setStyleSheet('color: red')
        self._warning_label.hide()

        form = QtWidgets.QFormLayout(self)
        form.addRow('Filename:', file_box)
        form.addRow('Width:',    self._spinbox_width)
        form.addRow('Height:',   self._spinbox_height)
        form.addRow('Extra encoder arguments:', self._encopts_text)
        form.addRow(self._warning_label)
        form.addRow(btn_hbox)

        ofile_btn.clicked.connect(self._select_ofile)
        self._export_btn.clicked.connect(self._export)
        self._ofile_text.textChanged.connect(self._check_settings)
        self._ofile_text.textChanged.connect(config.set_export_filename)
        self._spinbox_width.valueChanged.connect(self._check_settings)
        self._spinbox_width.valueChanged.connect(config.set_export_width)
        self._spinbox_height.valueChanged.connect(self._check_settings)
        self._spinbox_height.valueChanged.connect(config.set_export_height)
        self._encopts_text.textChanged.connect(config.set_export_extra_enc_args)

        self._exporter = None

    def enter(self):
        cfg = self._get_scene_func()
        if not cfg:
            return

        self._framerate = cfg['framerate']
        self._aspect_ratio = cfg['aspect_ratio']

        self._check_settings()

    @QtCore.Slot()
    def _check_settings(self):

        warnings = []

        if self._ofile_text.text().endswith('.gif'):
            fr = self._framerate
            gif_recommended_framerate = (Fraction(25, 1), Fraction(50, 1))
            if Fraction(*fr) not in gif_recommended_framerate:
                gif_framerates = ', '.join('%s' % x for x in gif_recommended_framerate)
                warnings.append(f'It is recommended to use one of these frame rate when exporting to GIF: {gif_framerates}')

        if warnings:
            self._warning_label.setText('\n'.join('⚠ ' + w for w in warnings))
            self._warning_label.show()
        else:
            self._warning_label.hide()

    @QtCore.Slot(int)
    def _progress(self, value):
        self._pgd.setValue(value)

    @QtCore.Slot()
    def _cancel(self):
        # Exporter.cancel() gracefuly stops the exporter thread and fires a
        # finished signal
        if self._exporter:
            self._exporter.cancel()

    @QtCore.Slot()
    def _fail(self):
        self._finish()
        QtWidgets.QMessageBox.critical(self,
                                       'Error',
                                       "You didn't select any scene to export.",
                                       QtWidgets.QMessageBox.Ok)

    @QtCore.Slot()
    def _finish(self):
        self._pgd.close()
        if self._exporter:
            self._exporter.wait()
            self._exporter = None

    @QtCore.Slot()
    def _export(self):
        if self._exporter:
            return

        ofile = self._ofile_text.text()
        width = self._spinbox_width.value()
        height = self._spinbox_height.value()
        extra_enc_args = self._encopts_text.text().split()

        self._pgd = QtWidgets.QProgressDialog('Exporting to %s' % ofile, 'Stop', 0, 100, self)
        self._pgd.setWindowModality(QtCore.Qt.WindowModal)
        self._pgd.setMinimumDuration(100)

        self._exporter = Exporter(self._get_scene_func, ofile, width, height, extra_enc_args)

        self._pgd.canceled.connect(self._cancel)
        self._exporter.progressed.connect(self._progress)
        self._exporter.failed.connect(self._fail)
        self._exporter.export_finished.connect(self._finish)

        self._exporter.start()

    @QtCore.Slot()
    def _select_ofile(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        self._ofile_text.setText(filenames[0])
