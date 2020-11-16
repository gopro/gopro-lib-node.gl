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

import sys

from PySide2 import QtCore, QtGui, QtWidgets

from pynodegl_utils.com import query_subproc
from pynodegl_utils.config import Config
from pynodegl_utils.scriptsmgr import ScriptsManager
from pynodegl_utils.hooks import HooksController, HooksCaller

from pynodegl_utils.ui.graph_view import GraphView
from pynodegl_utils.ui.export_view import ExportView
from pynodegl_utils.ui.hooks_view import HooksView
from pynodegl_utils.ui.medias_view import MediasView
from pynodegl_utils.ui.serial_view import SerialView
from pynodegl_utils.ui.toolbar import Toolbar


class MainWindow(QtWidgets.QSplitter):

    error = QtCore.Signal(str)

    def __init__(self, module_pkgname, hooksdirs):
        super().__init__(QtCore.Qt.Horizontal)
        self._win_title_base = 'Node.gl controller'
        self.setWindowTitle(self._win_title_base)

        self._module_pkgname = module_pkgname
        self._scripts_mgr = ScriptsManager(module_pkgname)
        self._hooks_caller = HooksCaller(hooksdirs)

        get_scene_func = self._get_scene

        self._config = Config(module_pkgname)

        # Apply previous geometry (position + dimensions)
        rect = self._config.get('geometry')
        if rect:
            geometry = QtCore.QRect(*rect)
            self.setGeometry(geometry)

        graph_view = GraphView(get_scene_func, self._config)
        export_view = ExportView(get_scene_func, self._config)
        hooks_view = HooksView(self._hooks_caller, self._config)
        self._medias_view = MediasView(self._config)
        serial_view = SerialView(get_scene_func)

        self._tabs = [
            ('Hooks', hooks_view),
            ('Graph view', graph_view),
            ('Export', export_view),
            ('Medias', self._medias_view),
            ('Serialization', serial_view),
        ]
        self._last_tab_index = -1

        self._tab_widget = QtWidgets.QTabWidget()
        for tab_name, tab_view in self._tabs:
            self._tab_widget.addTab(tab_view, tab_name)
        self._tab_widget.currentChanged.connect(self._currentTabChanged)

        self._hooks_ctl = HooksController(self._get_scene, hooks_view, self._hooks_caller)

        self._scene_toolbar = Toolbar(self._config)
        self._scene_toolbar.sceneChanged.connect(self._scene_changed)
        self._scene_toolbar.sceneChanged.connect(self._scene_changed_hook)
        self._scene_toolbar.sceneChanged.connect(self._config.scene_changed)
        self._scene_toolbar.aspectRatioChanged.connect(self._config.set_aspect_ratio)
        self._scene_toolbar.samplesChanged.connect(self._config.set_samples)
        self._scene_toolbar.frameRateChanged.connect(self._config.set_frame_rate)
        self._scene_toolbar.logLevelChanged.connect(self._config.set_log_level)
        self._scene_toolbar.clearColorChanged.connect(self._config.set_clear_color)
        self._scene_toolbar.backendChanged.connect(self._config.set_backend)

        self._errbuf = QtWidgets.QPlainTextEdit()
        self._errbuf.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
        self._errbuf.setReadOnly(True)
        self._errbuf.hide()

        tabs_and_errbuf = QtWidgets.QVBoxLayout()
        tabs_and_errbuf.addWidget(self._tab_widget)
        tabs_and_errbuf.addWidget(self._errbuf)
        tabs_and_errbuf_widget = QtWidgets.QWidget()
        tabs_and_errbuf_widget.setLayout(tabs_and_errbuf)

        self.addWidget(self._scene_toolbar)
        self.addWidget(tabs_and_errbuf_widget)
        self.setStretchFactor(1, 1)

        self._scene_toolbar.reload_btn.clicked.connect(self._scripts_mgr.reload)  # TODO: drop
        self._scripts_mgr.error.connect(self._all_scripts_err)
        self._scripts_mgr.scriptsChanged.connect(self._scene_toolbar.on_scripts_changed)
        self._scripts_mgr.start()

        self.error.connect(self._scene_err)

        # Load the previous scene if the current and previously loaded
        # module packages match
        prev_pkgname = self._config.get('pkg')
        prev_module = self._config.get('module')
        prev_scene = self._config.get('scene')
        if prev_pkgname == module_pkgname:
            self._scene_toolbar.load_scene_from_name(prev_module, prev_scene)

    @QtCore.Slot(str)
    def _scene_err(self, err_str):
        if err_str:
            self._errbuf.setPlainText(err_str)
            self._errbuf.show()
            sys.stderr.write(err_str)
        else:
            self._errbuf.hide()

    @QtCore.Slot(str)
    def _all_scripts_err(self, err_str):
        self._scene_toolbar.clear_scripts()
        self._scene_err(err_str)

    def _get_scene(self, **cfg_overrides):
        cfg = self._scene_toolbar.get_cfg()
        if cfg['scene'] is None:
            return None
        cfg['pkg'] = self._module_pkgname
        medias = self._medias_view.get_medias()
        cfg['medias'] = medias if medias else None
        cfg.update(cfg_overrides)

        self._scripts_mgr.pause()
        ret = query_subproc(query='scene', **cfg)
        if 'error' in ret:
            self._scripts_mgr.update_filelist(ret['filelist'])
            self._scripts_mgr.resume()
            self.error.emit(ret['error'])
            return None

        self.error.emit(None)
        self._scripts_mgr.set_filelist(ret['filelist'])
        self._scripts_mgr.resume()
        self._scene_toolbar.set_cfg(ret)

        return ret

    @QtCore.Slot(str, str)
    def _scene_changed(self, module_name, scene_name):
        self.setWindowTitle('%s - %s.%s' % (self._win_title_base, module_name, scene_name))
        self._currentTabChanged(self._tab_widget.currentIndex())

    @QtCore.Slot(str, str)
    def _scene_changed_hook(self, module_name, scene_name):
        self._hooks_ctl.process(module_name, scene_name)

    def _emit_geometry(self):
        geometry = (self.x(), self.y(), self.width(), self.height())
        self._config.geometry_changed(geometry)

    @QtCore.Slot(QtGui.QResizeEvent)
    def resizeEvent(self, resize_event):
        super().resizeEvent(resize_event)
        self._emit_geometry()

    @QtCore.Slot(QtGui.QMoveEvent)
    def moveEvent(self, move_event):
        super().moveEvent(move_event)
        self._emit_geometry()

    @QtCore.Slot(QtGui.QCloseEvent)
    def closeEvent(self, close_event):
        for name, widget in self._tabs:
            widget.close()
        super().closeEvent(close_event)

    @QtCore.Slot(int)
    def _currentTabChanged(self, index):
        next_view = self._tabs[index][1]
        prev_view = self._tabs[self._last_tab_index][1]
        if index != self._last_tab_index and hasattr(prev_view, 'leave'):
            prev_view.leave()
        if hasattr(next_view, 'enter'):
            next_view.enter()
        self._last_tab_index = index
