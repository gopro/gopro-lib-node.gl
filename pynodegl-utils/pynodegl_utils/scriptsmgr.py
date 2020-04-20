#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

import os.path as op

from threading import Timer
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

from PySide2 import QtCore

from .com import query_subproc

MIN_RELOAD_INTERVAL = 0.0015


class ScriptsManager(QtCore.QObject):

    scriptsChanged = QtCore.Signal(list)
    error = QtCore.Signal(str)

    def __init__(self, module_pkgname):
        super().__init__()
        self._module_pkgname = module_pkgname
        self._dirs_to_watch = set()
        self._files_to_watch = set()

        self._event_handler = FileSystemEventHandler()
        self._event_handler.on_any_event = self._on_any_event
        self._observer = Observer()
        self._observer.start()
        self._timer = None

    def start(self):
        self.reload()

    def resume(self):
        self._observer.unschedule_all()
        for path in self._dirs_to_watch:
            if path == '':
                path = '.'
            self._observer.schedule(self._event_handler, path)

    def pause(self):
        self._observer.unschedule_all()

    @QtCore.Slot()
    def reload(self):
        self.pause()
        odict = query_subproc(query='list', pkg=self._module_pkgname)
        if 'error' in odict:
            self.update_filelist(odict['filelist'])
            self.resume()
            self.error.emit(odict['error'])
            return
        self.error.emit(None)

        self.set_filelist(odict['filelist'])
        self.resume()

        scripts = odict['scenes']
        self.scriptsChanged.emit(scripts)

    def _on_any_event(self, event):
        def print_reload():
            print('Reloading scene')
            self.reload()

        if event.src_path in self._files_to_watch:
            print('Change detected in %s' % event.src_path)
            if self._timer is not None:
                self._timer.cancel()
            self._timer = Timer(MIN_RELOAD_INTERVAL, print_reload, ())
            self._timer.start()

    def _update_dirs_to_watch(self):
        self._dirs_to_watch = set(op.dirname(f) for f in self._files_to_watch)

    @QtCore.Slot(list)
    def update_filelist(self, filelist):
        self._files_to_watch.update(filelist)
        self._update_dirs_to_watch()

    @QtCore.Slot(list)
    def set_filelist(self, filelist):
        self._files_to_watch = set(filelist)
        self._update_dirs_to_watch()
