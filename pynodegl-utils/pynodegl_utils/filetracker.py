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
import builtins
import os.path as op
import distutils.sysconfig


class FileTracker:

    def __init__(self):
        self.filelist = set()
        self._builtin_open = builtins.open
        self._pysysdir = op.realpath(distutils.sysconfig.get_python_lib(standard_lib=True))

    def _open_hook(self, name, *args, **kwargs):
        ret = self._builtin_open(name, *args, **kwargs)
        if op.isfile(name):
            self.filelist.update([op.realpath(name)])
        return ret

    def _get_trackable_files(self):
        files = set()
        for mod in sys.modules.values():
            if not hasattr(mod, '__file__') or mod.__file__ is None:
                continue
            path = op.realpath(mod.__file__)
            modpath = op.dirname(path)
            if modpath.startswith(self._pysysdir):
                continue
            if path.endswith('.pyc'):
                path = path[:-1]
            elif not path.endswith('.py'):
                continue
            files.update([path])
        return files

    def start_hooking(self):
        self._start_files = self._get_trackable_files()
        builtins.open = self._open_hook

    def end_hooking(self):
        new_files = self._get_trackable_files() - self._start_files
        self.filelist.update(new_files)
        builtins.open = self._builtin_open
