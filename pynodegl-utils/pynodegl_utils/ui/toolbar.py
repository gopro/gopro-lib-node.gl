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
from fractions import Fraction
from PySide2 import QtCore, QtGui, QtWidgets

import pynodegl as ngl

from pynodegl_utils.config import Config
from pynodegl_utils.control_widgets import control_to_widget


class Toolbar(QtWidgets.QWidget):

    sceneChanged = QtCore.Signal(str, str)
    aspectRatioChanged = QtCore.Signal(tuple)
    samplesChanged = QtCore.Signal(int)
    frameRateChanged = QtCore.Signal(tuple)
    logLevelChanged = QtCore.Signal(str)
    clearColorChanged = QtCore.Signal(tuple)
    backendChanged = QtCore.Signal(str)

    def __init__(self, config):
        super().__init__()

        self._scene_opts_widget = None
        self._scene_extra_args = {}

        self._scn_view = QtWidgets.QTreeView()
        self._scn_view.setHeaderHidden(True)
        self._scn_view.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self._scn_view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._scn_mdl = QtGui.QStandardItemModel()
        self._scn_view.setModel(self._scn_mdl)

        self._current_scene_data = None

        all_ar = config.CHOICES['aspect_ratio']
        default_ar = config.get('aspect_ratio')
        self._ar_cbbox = QtWidgets.QComboBox()
        for ar in all_ar:
            self._ar_cbbox.addItem('%d:%d' % ar)
        self._ar_cbbox.setCurrentIndex(all_ar.index(default_ar))
        ar_lbl = QtWidgets.QLabel('Aspect ratio:')
        ar_hbox = QtWidgets.QHBoxLayout()
        ar_hbox.addWidget(ar_lbl)
        ar_hbox.addWidget(self._ar_cbbox)

        self._far_lbl = QtWidgets.QLabel('Forced aspect ratio:')
        self._far_lbl.setStyleSheet('color: red;')
        self._far_lbl.setVisible(False)
        self._far_lbl2 = QtWidgets.QLabel('1:1')
        self._far_lbl2.setStyleSheet('color: red;')
        self._far_lbl2.setVisible(False)
        far_hbox = QtWidgets.QHBoxLayout()
        far_hbox.addWidget(self._far_lbl)
        far_hbox.addWidget(self._far_lbl2)

        all_samples = config.CHOICES['samples']
        default_samples = config.get('samples')
        self._samples_cbbox = QtWidgets.QComboBox()
        for samples in all_samples:
            self._samples_cbbox.addItem('%dx' % samples if samples else 'Disabled')
        self._samples_cbbox.setCurrentIndex(all_samples.index(default_samples))
        samples_lbl = QtWidgets.QLabel('MSAA:')
        samples_hbox = QtWidgets.QHBoxLayout()
        samples_hbox.addWidget(samples_lbl)
        samples_hbox.addWidget(self._samples_cbbox)

        all_fr = config.CHOICES['framerate']
        default_fr = config.get('framerate')
        self._fr_cbbox = QtWidgets.QComboBox()
        for fr in all_fr:
            self._fr_cbbox.addItem('%.5g FPS' % (fr[0] / float(fr[1])))
        self._fr_cbbox.setCurrentIndex(all_fr.index(default_fr))
        fr_lbl = QtWidgets.QLabel('Frame rate:')
        fr_hbox = QtWidgets.QHBoxLayout()
        fr_hbox.addWidget(fr_lbl)
        fr_hbox.addWidget(self._fr_cbbox)

        all_loglevels = config.CHOICES['log_level']
        default_loglevel = config.get('log_level')
        self._loglevel_cbbox = QtWidgets.QComboBox()
        for level in all_loglevels:
            self._loglevel_cbbox.addItem(level.title())
        log_level_idx = all_loglevels.index(default_loglevel)
        self._loglevel_cbbox.setCurrentIndex(log_level_idx)
        self._set_loglevel(log_level_idx)
        loglevel_lbl = QtWidgets.QLabel('Min log level:')
        loglevel_hbox = QtWidgets.QHBoxLayout()
        loglevel_hbox.addWidget(loglevel_lbl)
        loglevel_hbox.addWidget(self._loglevel_cbbox)

        backend_names = {
            'opengl': 'OpenGL',
            'opengles': 'OpenGL ES',
        }
        all_backends = config.CHOICES['backend']
        default_backend = config.get('backend')
        self._backend_cbbox = QtWidgets.QComboBox()
        for backend in all_backends:
            self._backend_cbbox.addItem(backend_names[backend])
        backend_idx = all_backends.index(default_backend)
        self._backend_cbbox.setCurrentIndex(backend_idx)
        backend_lbl = QtWidgets.QLabel('Backend:')
        backend_hbox = QtWidgets.QHBoxLayout()
        backend_hbox.addWidget(backend_lbl)
        backend_hbox.addWidget(self._backend_cbbox)

        default_clearcolor = config.get('clear_color')
        self._clearcolor_btn = QtWidgets.QPushButton()
        color = QtGui.QColor()
        color.setRgbF(*default_clearcolor)
        self._set_widget_clear_color(color)
        self._clearcolor_btn.pressed.connect(self._set_clear_color)

        clearcolor_lbl = QtWidgets.QLabel('Clear color:')
        clearcolor_hbox = QtWidgets.QHBoxLayout()
        clearcolor_hbox.addWidget(clearcolor_lbl)
        clearcolor_hbox.addWidget(self._clearcolor_btn)

        self.reload_btn = QtWidgets.QPushButton('Force scripts reload')

        self._scene_toolbar_layout = QtWidgets.QVBoxLayout(self)
        self._scene_toolbar_layout.addLayout(ar_hbox)
        self._scene_toolbar_layout.addLayout(far_hbox)
        self._scene_toolbar_layout.addLayout(samples_hbox)
        self._scene_toolbar_layout.addLayout(fr_hbox)
        self._scene_toolbar_layout.addLayout(loglevel_hbox)
        self._scene_toolbar_layout.addLayout(backend_hbox)
        self._scene_toolbar_layout.addLayout(clearcolor_hbox)
        self._scene_toolbar_layout.addWidget(self.reload_btn)
        self._scene_toolbar_layout.addWidget(self._scn_view)

        self._scn_view.clicked.connect(self._scn_view_selected)
        self._scn_view.activated.connect(self._scn_view_selected)
        self._ar_cbbox.currentIndexChanged.connect(self._set_aspect_ratio)
        self._samples_cbbox.currentIndexChanged.connect(self._set_samples)
        self._fr_cbbox.currentIndexChanged.connect(self._set_frame_rate)
        self._loglevel_cbbox.currentIndexChanged.connect(self._set_loglevel)
        self._backend_cbbox.currentIndexChanged.connect(self._set_backend)

    def _replace_scene_opts_widget(self, widget):
        if self._scene_opts_widget:
            self._scene_toolbar_layout.removeWidget(self._scene_opts_widget)
            self._scene_opts_widget.deleteLater()
            self._scene_opts_widget = None
        if widget:
            self._scene_toolbar_layout.addWidget(widget)
            self._scene_opts_widget = widget
            widget.show()

    @QtCore.Slot(str, object)
    def _widget_scene_reload(self, name, value):
        self._scene_extra_args[name] = value
        self._load_current_scene(load_widgets=False)

    def _get_opts_widget_from_specs(self, widgets_specs):
        widgets = []
        for key, default, ctl_id, ctl_data in widgets_specs:
            widget_cls = control_to_widget[ctl_id]
            widget = widget_cls(key, default, **ctl_data)
            widget.needSceneReload.connect(self._widget_scene_reload)
            widgets.append(widget)
        if not widgets:
            return None

        groupbox = QtWidgets.QGroupBox('Custom scene options')
        vbox = QtWidgets.QVBoxLayout()
        for widget in widgets:
            vbox.addWidget(widget)
        groupbox.setLayout(vbox)
        return groupbox

    def get_cfg(self):
        choices = Config.CHOICES
        return {
                'scene': self._current_scene_data[:2] if self._current_scene_data else None,
                'aspect_ratio': choices['aspect_ratio'][self._ar_cbbox.currentIndex()],
                'framerate': choices['framerate'][self._fr_cbbox.currentIndex()],
                'samples': choices['samples'][self._samples_cbbox.currentIndex()],
                'extra_args': self._scene_extra_args,
                'clear_color': self._clear_color,
                'backend': choices['backend'][self._backend_cbbox.currentIndex()],
        }

    def load_scene_from_name(self, module_name, scene_name):

        def get_func_info(module_item, func_name):
            nb_funcs = module_item.rowCount()
            for i in range(nb_funcs):
                func_item = module_item.child(i)
                if func_item.text() == scene_name:
                    return func_item.data()
            return None

        module_items = self._scn_mdl.findItems(module_name)
        if module_items:
            self._current_scene_data = get_func_info(module_items[0], scene_name)
            self._load_current_scene()

    def _load_current_scene(self, load_widgets=True):
        if not self._current_scene_data:
            return
        module_name, scene_name, widgets_specs = self._current_scene_data
        if load_widgets:
            self._scene_extra_args = {}
            scene_opts_widget = self._get_opts_widget_from_specs(widgets_specs)
            self._replace_scene_opts_widget(scene_opts_widget)
        self.sceneChanged.emit(module_name, scene_name)

    def clear_scripts(self):
        self._scn_mdl.clear()

    def _reload_scene_view(self, scenes):
        self._scn_mdl.clear()
        for module_name, sub_scenes in scenes:
            qitem_script = QtGui.QStandardItem(module_name)
            for scene_name, scene_doc, widgets_specs in sub_scenes:
                scene_data = (module_name, scene_name, widgets_specs)

                # update cached widget specs if module and scene match
                if self._current_scene_data:
                    cur_module_name, cur_scene_name, _ = self._current_scene_data
                    if module_name == cur_module_name and scene_name == cur_scene_name:
                        self._current_scene_data = scene_data

                qitem_func = QtGui.QStandardItem(scene_name)
                if scene_doc:
                    qitem_func.setToolTip(scene_doc.strip())
                qitem_script.appendRow(qitem_func)
                qitem_func.setData(scene_data)
            self._scn_mdl.appendRow(qitem_script)
        self._scn_view.expandAll()

    @QtCore.Slot(QtCore.QModelIndex)
    def _scn_view_selected(self, index):
        data = self._scn_mdl.itemFromIndex(index).data()
        if data:
            self._current_scene_data = data
            self._load_current_scene()

    @QtCore.Slot(list)
    def on_scripts_changed(self, scenes):
        self._reload_scene_view(scenes)
        self._load_current_scene()

    def set_cfg(self, cfg):
        try:
            cfg_ar = Fraction(*cfg['aspect_ratio'])
            cfg_ar = (cfg_ar.numerator, cfg_ar.denominator)
            ar = Fraction(*Config.CHOICES['aspect_ratio'][self._ar_cbbox.currentIndex()])
            ar = (ar.numerator, ar.denominator)
            if ar != cfg_ar:
                self._far_lbl2.setText('%d:%d' % cfg_ar)
                self._far_lbl.setVisible(True)
                self._far_lbl2.setVisible(True)
            else:
                self._far_lbl.setVisible(False)
                self._far_lbl2.setVisible(False)
        except ValueError:
            pass

    @QtCore.Slot(int)
    def _set_loglevel(self, index):
        level_str = Config.CHOICES['log_level'][index]
        ngl_level = eval('ngl.LOG_%s' % level_str.upper())
        ngl.log_set_min_level(ngl_level)
        self.logLevelChanged.emit(level_str)

    @QtCore.Slot(int)
    def _set_aspect_ratio(self, index):
        ar = Config.CHOICES['aspect_ratio'][index]
        self.aspectRatioChanged.emit(ar)
        self._load_current_scene()

    @QtCore.Slot(int)
    def _set_frame_rate(self, index):
        fr = Config.CHOICES['framerate'][index]
        self.frameRateChanged.emit(fr)
        self._load_current_scene()

    @QtCore.Slot(int)
    def _set_samples(self, index):
        samples = Config.CHOICES['samples'][index]
        self.samplesChanged.emit(samples)
        self._load_current_scene()

    def _set_widget_clear_color(self, color):
        color_name = color.name()
        self._clearcolor_btn.setStyleSheet('background-color: %s;' % color_name)
        self._clear_color = color.getRgbF()

    @QtCore.Slot()
    def _set_clear_color(self):
        initial_color = QtGui.QColor()
        initial_color.setRgbF(*self._clear_color)
        color = QtWidgets.QColorDialog.getColor(initial=initial_color)
        if not color.isValid():
            return
        self._set_widget_clear_color(color)
        self.clearColorChanged.emit(color.getRgbF())
        self._load_current_scene()

    @QtCore.Slot(int)
    def _set_backend(self, index):
        backend = Config.CHOICES['backend'][index]
        self.backendChanged.emit(backend)
        self._load_current_scene()
