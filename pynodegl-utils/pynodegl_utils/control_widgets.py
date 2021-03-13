#!/usr/bin/env python
#
# Copyright 2019 GoPro Inc.
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
from PySide6 import QtCore, QtGui, QtWidgets


class _ControlWidget(QtWidgets.QWidget):

    needSceneReload = QtCore.Signal(str, object)

    def __init__(self, name):
        super().__init__()
        self._name = name
        self.layout = QtWidgets.QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)

    def signal_change(self, value):
        self.needSceneReload.emit(self._name, value)

    def get_label_text(self, value=None):
        return f'<b>{self._name}:</b> {value}' if value is not None else f'<b>{self._name}:</b>'


class Slider(_ControlWidget):

    def __init__(self, name, value, range, unit_base):
        super().__init__(name)
        slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self._unit_base = unit_base
        slider.setRange(range[0] * self._unit_base, range[1] * self._unit_base)
        slider.setValue(value * self._unit_base)
        self._label = QtWidgets.QLabel(self.get_label_text(value))
        slider.valueChanged.connect(self._slider_value_changed)
        self.layout.addWidget(self._label)
        self.layout.addWidget(slider)

    @QtCore.Slot(int)
    def _slider_value_changed(self, value):
        real_value = value if self._unit_base == 1 else value / float(self._unit_base)
        self._label.setText(self.get_label_text(real_value))
        self.signal_change(real_value)


class VectorWidget(_ControlWidget):

    def __init__(self, name, value, n, minv, maxv):
        super().__init__(name)
        hlayout = QtWidgets.QHBoxLayout()
        self._spinboxes = []
        elems_min = minv if minv is not None else [0] * n
        elems_max = maxv if maxv is not None else [1] * n
        assert 2 <= n <= 4
        assert len(elems_min) == len(elems_max) == len(value) == n
        for elem_value, elem_min, elem_max in zip(value, elems_min, elems_max):
            spin = QtWidgets.QDoubleSpinBox()
            spin.setMinimum(elem_min)
            spin.setMaximum(elem_max)
            spin.setSingleStep(0.01)
            spin.setValue(elem_value)
            self._spinboxes.append(spin)
            spin.valueChanged.connect(self._spin_value_changed)
            hlayout.addWidget(spin)
        label = QtWidgets.QLabel(self.get_label_text())
        self.layout.addWidget(label)
        self.layout.addLayout(hlayout)

    @QtCore.Slot(float)
    def _spin_value_changed(self, value):
        self.signal_change(tuple(spin.value() for spin in self._spinboxes))


class ColorPicker(_ControlWidget):

    def __init__(self, name, value):
        super().__init__(name)
        self._color_btn = QtWidgets.QPushButton()
        color = QtGui.QColor()
        color.setRgbF(*value)
        color_name = color.name()
        self._color_btn.setStyleSheet('background-color: %s;' % color_name)
        self._label = QtWidgets.QLabel(self.get_label_text(color_name))
        self._color_btn.pressed.connect(self._pick_color)
        self._qcolor = color
        self.layout.addWidget(self._label)
        self.layout.addWidget(self._color_btn)

    @QtCore.Slot()
    def _pick_color(self):
        color = QtWidgets.QColorDialog.getColor(initial=self._qcolor)
        if not color.isValid():
            return
        self._color_btn.setStyleSheet('background-color: %s;' % color.name())
        self._qcolor = color
        self._label.setText(self.get_label_text(color.name()))
        self.signal_change(color.getRgbF())


class Checkbox(_ControlWidget):

    def __init__(self, name, value):
        super().__init__(name)
        self._chkbox = QtWidgets.QCheckBox(name)
        self._chkbox.setChecked(value)
        self._chkbox.stateChanged.connect(self._checkbox_toggle)
        self.layout.addWidget(self._chkbox)

    @QtCore.Slot()
    def _checkbox_toggle(self):
        self.signal_change(self._chkbox.isChecked())


class FilePicker(_ControlWidget):

    def __init__(self, name, value, filter):
        super().__init__(name)
        self._filter = filter
        dialog_btn = QtWidgets.QPushButton('Open file')
        self._label = QtWidgets.QLabel(self.get_label_text(value))
        dialog_btn.pressed.connect(self._choose_filename)
        self.layout.addWidget(self._label)
        self.layout.addWidget(dialog_btn)

    @QtCore.Slot()
    def _choose_filename(self):
        filename = QtWidgets.QFileDialog.getOpenFileName(self, 'Open file', '', self._filter)
        if not filename[0]:
            return
        self._label.setText(self.get_label_text(op.basename(filename[0])))
        self.signal_change(filename[0])


class ComboBox(_ControlWidget):

    def __init__(self, name, value, choices):
        super().__init__(name)
        combobox = QtWidgets.QComboBox()
        label = QtWidgets.QLabel(self.get_label_text())
        combobox.addItems(choices)
        combobox.setCurrentIndex(choices.index(value))
        combobox.currentTextChanged.connect(self._combobox_select)
        self.layout.addWidget(label)
        self.layout.addWidget(combobox)

    @QtCore.Slot(str)
    def _combobox_select(self, text):
        self.signal_change(text)


class TextInput(_ControlWidget):

    def __init__(self, name, value):
        super().__init__(name)
        self._text = QtWidgets.QPlainTextEdit()
        self._text.setPlainText(value)
        label = QtWidgets.QLabel(self.get_label_text())
        self.layout.addWidget(label)
        self.layout.addWidget(self._text)
        submit_btn = QtWidgets.QPushButton('OK')
        submit_btn.pressed.connect(self._submit_text)
        self.layout.addWidget(label)
        self.layout.addWidget(self._text)
        self.layout.addWidget(submit_btn)

    @QtCore.Slot()
    def _submit_text(self):
        self.signal_change(self._text.toPlainText())


control_to_widget = dict(
    Range=Slider,
    Vector=VectorWidget,
    Color=ColorPicker,
    Bool=Checkbox,
    File=FilePicker,
    List=ComboBox,
    Text=TextInput,
)
