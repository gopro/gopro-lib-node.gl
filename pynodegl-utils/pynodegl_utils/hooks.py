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

import hashlib
import os
import os.path as op
import tempfile
import time
import importlib
import platform

from PySide6 import QtCore

from pynodegl_utils.com import load_script

class _HooksCaller:

    _HOOKS = ('get_session_info', 'get_sessions', 'scene_change', 'sync_file')

    def __init__(self, hooks_script):
        self._module = load_script(hooks_script)
        if not all(hasattr(self._module, hook) for hook in self._HOOKS):
            raise NotImplementedError(f'{hooks_script}: the following functions must be implemented: {self._HOOKS}')

    def _has_hook(self, name):
        return hasattr(self._module, name)

    def get_session_info(self, session_id):
        return self._module.get_session_info(session_id)

    def get_sessions(self):
        sessions = []
        session_list = self._module.get_sessions()
        for session in session_list:
            session_id, session_desc = session
            # TODO: asynchronous calls? (individual session queries could take some time)
            session_info = self._module.get_session_info(session_id)
            sessions.append((
                session_id,
                session_desc,
                session_info.get('backend', ''),
                session_info.get('system', ''),
            ))
        return sessions

    @staticmethod
    def _uint_clear_color(vec4_color):
        uint_color = 0
        for i, comp in enumerate(vec4_color):
            comp_val = int(round(comp*0xff)) & 0xff
            uint_color |= comp_val << (24 - i*8)
        return uint_color

    def scene_change(self, session_id, local_scene, cfg):
        return self._module.scene_change(
            session_id,
            local_scene,
            cfg['duration'],
            cfg['aspect_ratio'],
            cfg['framerate'],
            self._uint_clear_color(cfg['clear_color']),
            cfg['samples'],
        )

    @staticmethod
    def _hash_filename(filename):
        statinfo = os.stat(filename)
        sha256 = hashlib.sha256()
        sha256.update(filename.encode())
        sha256.update(str(statinfo.st_size).encode())
        sha256.update(str(statinfo.st_mtime).encode())
        digest = sha256.hexdigest()
        _, ext = op.splitext(filename)
        return op.join(digest + ext)

    def sync_file(self, session_id, localfile):
        return self._module.sync_file(session_id, localfile, self._hash_filename(localfile))


class HooksCaller:

    def __init__(self, hooks_scripts):
        self._callers = [_HooksCaller(hooks_script) for hooks_script in hooks_scripts]

    def _get_caller_session_id(self, unique_session_id):
        istr, session_id = unique_session_id.split(':', maxsplit=1)
        return self._callers[int(istr)], session_id

    def get_sessions(self):
        '''
        Session IDs may be identical accross hook systems. A pathological case
        is with several instances of the same hook system, but it could
        happen in other situations as well since hook systems don't know each
        others and may pick common identifiers), so we need to make them
        unique.

        This methods makes these session IDs unique by adding the index of
        the caller as prefix.
        '''
        sessions = []
        for caller_id, caller in enumerate(self._callers):
            caller_sessions = caller.get_sessions()
            for (session_id, desc, backend, system) in caller_sessions:
                sessions.append((f'{caller_id}:{session_id}', desc, backend, system))
        return sessions

    def get_session_info(self, session_id):
        caller, session_id = self._get_caller_session_id(session_id)
        return caller.get_session_info(session_id)

    def scene_change(self, session_id, local_scene, cfg):
        caller, session_id = self._get_caller_session_id(session_id)
        return caller.scene_change(session_id, local_scene, cfg)

    def sync_file(self, session_id, localfile):
        caller, session_id = self._get_caller_session_id(session_id)
        return caller.sync_file(session_id, localfile)


class _HooksThread(QtCore.QThread):

    uploadingFile = QtCore.Signal(str, int, int, str)
    buildingScene = QtCore.Signal(str, str, str)
    sendingScene = QtCore.Signal(str, str)
    done = QtCore.Signal(str, str, float)
    error = QtCore.Signal(str, str)

    def __init__(self, get_scene_func, hooks_caller, session_id, backend, system, module_name, scene_name):
        super().__init__()
        self._get_scene_func = get_scene_func
        self._hooks_caller = hooks_caller
        self._session_id = session_id
        self._scene_id = f'{module_name}.{scene_name}'

        # The graphic backend can be different when using hooks: the scene might
        # be rendered on a remote device different from the one constructing
        # the scene graph
        self._target_backend = backend
        self._target_system = system

    @staticmethod
    def _filename_escape(filename):
        s = ''
        for c in filename:
            cval = ord(c)
            if cval >= ord('!') and cval <= ord('~') and cval != ord('%'):
                s += c
            else:
                s += '%%%02x' % (cval & 0xff)
        return s

    def _change_scene(self, scenefile, cfg, start_time):
        self.sendingScene.emit(self._session_id, self._scene_id)
        try:
            self._hooks_caller.scene_change(self._session_id, scenefile, cfg)
        except Exception as e:
            self.error.emit(self._session_id, 'Error while sending scene: %s' % str(e))
            return
        self.done.emit(self._session_id, self._scene_id, time.time() - start_time)

    def run(self):
        start_time = time.time()
        session_id, backend, system = self._session_id, self._target_backend, self._target_system

        self.buildingScene.emit(session_id, backend, system)
        cfg = self._get_scene_func(backend=backend, system=system)
        if not cfg:
            self.error.emit(session_id, 'Error getting scene')
            return

        try:
            # The serialized scene is associated with a bunch of assets which we
            # need to sync. Similarly, the remote assets directory might be
            # different from the one in local, so we need to fix up the scene
            # appropriately.
            serialized_scene = cfg['scene'].decode('ascii')
            filelist = [m.filename for m in cfg['medias']] + cfg['files']
            for i, localfile in enumerate(filelist, 1):
                self.uploadingFile.emit(session_id, i, len(filelist), localfile)
                try:
                    remotefile = self._hooks_caller.sync_file(session_id, localfile)
                except Exception as e:
                    self.error.emit(session_id, 'Error while uploading %s: %s' % (localfile, str(e)))
                    return
                serialized_scene = serialized_scene.replace(
                        self._filename_escape(localfile),
                        self._filename_escape(remotefile))

            # The serialized scene is then stored in a file which is then
            # communicated with additional parameters to the user
            # temfile.NamedTemporaryFile is not supported on Windows, see
            # https://docs.python.org/3/library/tempfile.html#tempfile.NamedTemporaryFile
            if platform.system() == 'Windows':
                fd, fname = tempfile.mkstemp(prefix='ngl_scene_', suffix='.ngl')
                try:
                    with os.fdopen(fd, "w", newline='\n') as f:
                        f.write(serialized_scene)
                    self._change_scene(fname, cfg, start_time)
                finally:
                    os.remove(fname)
            else:
                with tempfile.NamedTemporaryFile('w', prefix='ngl_scene_', suffix='.ngl', delete=True) as f:
                    f.write(serialized_scene)
                    f.flush()
                    self._change_scene(f.name, cfg, start_time)
        except Exception as e:
            self.error.emit(session_id, 'Error: %s' % str(e))


class HooksController(QtCore.QObject):

    def __init__(self, get_scene_func, hooks_view, hooks_caller):
        super().__init__()
        self._get_scene_func = get_scene_func
        self._hooks_view = hooks_view
        self._hooks_caller = hooks_caller
        self._threads = []

    def _wait_threads(self):
        for hook_thread in self._threads:
            hook_thread.wait()
        self._threads = []

    def process(self, module_name, scene_name):
        self._wait_threads()  # Wait previous batch
        data = self._hooks_view.get_data_from_model()
        for session_id, data_row in data.items():
            if not data_row['checked']:
                continue
            hook_thread = _HooksThread(self._get_scene_func, self._hooks_caller,
                                       session_id, data_row['backend'], data_row['system'],
                                       module_name, scene_name)
            self._threads.append(hook_thread)
            hook_thread.uploadingFile.connect(self._hooks_uploading)
            hook_thread.buildingScene.connect(self._hooks_building_scene)
            hook_thread.sendingScene.connect(self._hooks_sending_scene)
            hook_thread.done.connect(self._hooks_done)
            hook_thread.error.connect(self._hooks_error)
            hook_thread.start()

    @QtCore.Slot(str, int, int, str)
    def _hooks_uploading(self, session_id, i, n, filename):
        self._hooks_view.update_status(session_id, 'Uploading [%d/%d]: %s...' % (i, n, filename))

    @QtCore.Slot(str, str, str)
    def _hooks_building_scene(self, session_id, backend, system):
        self._hooks_view.update_status(session_id, f'Building {system} scene in {backend}...')

    @QtCore.Slot(str, str)
    def _hooks_sending_scene(self, session_id, scene):
        self._hooks_view.update_status(session_id, 'Sending scene %s...' % scene)

    @QtCore.Slot(str, str, float)
    def _hooks_done(self, session_id, scene, t):
        self._hooks_view.update_status(session_id, f'Submitted {scene} in {t:f}')

    @QtCore.Slot(str, str)
    def _hooks_error(self, session_id, err):
        self._hooks_view.update_status(session_id, err)
