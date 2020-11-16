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

import importlib
import inspect
import os
import os.path as op
import pickle
import pkgutil
import subprocess
import sys
import traceback
import pynodegl as ngl
from pynodegl_utils.filetracker import FileTracker


IPC_READ_BUFSIZE = 4096


def load_script(path):
    # https://docs.python.org/3/library/importlib.html#importing-a-source-file-directly
    name, _ = op.splitext(op.basename(path))
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def query_subproc(**idict):

    '''
    Run the query in a sub-process. IPC happens using pipes and pickle
    serialization of dict.

    The parent process will write the query to the writable input fd
    (fd_w) and read the result from the readable output fd (fd_r).

    The executed child (ngl-com sub-process) will read the query from the
    readable input fd (child_fd_r) and write result to writable output fd
    (child_fd_w).
    '''

    child_fd_r, fd_w = os.pipe()
    fd_r, child_fd_w = os.pipe()

    cmd = [sys.executable, '-m', 'pynodegl_utils.com', str(child_fd_r), str(child_fd_w)]
    ret = subprocess.Popen(cmd, pass_fds=(child_fd_r, child_fd_w))
    os.close(child_fd_r)
    os.close(child_fd_w)

    # Send input
    idata = pickle.dumps(idict)
    os.write(fd_w, idata)
    os.close(fd_w)

    # Receive output
    odata = b''
    while True:
        rdata = os.read(fd_r, IPC_READ_BUFSIZE)
        if not rdata:
            break
        odata += rdata
    os.close(fd_r)
    odict = pickle.loads(odata)

    ret.wait()

    return odict


def query_inplace(**idict):

    '''
    Run the query in-place.
    '''

    module_pkgname = idict['pkg']
    module_is_script = module_pkgname.endswith('.py')

    # Start tracking the imported modules and opened files
    ftrack = FileTracker()
    ftrack.start_hooking()

    odict = {}

    try:
        if idict['query'] == 'scene':

            # Get module.func
            module_name, scene_name = idict['scene']
            if module_is_script:
                module = load_script(module_pkgname)
            else:
                import_name = '%s.%s' % (module_pkgname, module_name)
                module = importlib.import_module(import_name)
            func = getattr(module, scene_name)

            # Call user constructing function
            odict = func(idict, **idict.get('extra_args', {}))
            scene = odict['scene']
            del odict['scene']
            scene.set_label(scene_name)

            # Prepare output data
            odict['scene'] = scene.dot() if idict.get('fmt') == 'dot' else scene.serialize()

        elif idict['query'] == 'list':

            scripts = []

            # Import the script, or the package and its sub-modules
            if module_is_script:
                module_pkgname = op.realpath(module_pkgname)
                module = load_script(module_pkgname)
                scripts.append((module.__name__, module))
            else:
                module = importlib.import_module(module_pkgname)
                for submod in pkgutil.iter_modules(module.__path__):
                    module_finder, module_name, ispkg = submod
                    if ispkg:
                        continue
                    script = importlib.import_module('.' + module_name, module_pkgname)
                    scripts.append((module_name, script))

            # Find all the scenes
            scenes = []
            for module_name, script in scripts:
                all_funcs = inspect.getmembers(script, inspect.isfunction)
                sub_scenes = []
                for func in all_funcs:
                    scene_name, func_wrapper = func
                    if not hasattr(func_wrapper, 'iam_a_ngl_scene_func'):
                        continue
                    sub_scenes.append((scene_name, func_wrapper.__doc__, func_wrapper.widgets_specs))
                if sub_scenes:
                    scenes.append((module_name, sub_scenes))

            # Prepare output data
            odict['scenes'] = scenes

    except:
        odict = {'error': traceback.format_exc()}

    # End of file and modules tracking
    ftrack.end_hooking()
    odict['filelist'] = ftrack.filelist
    if module_is_script:
        odict['filelist'].update([module_pkgname])

    return odict


# Entry point for the ngl-com tool
def run():
    fd_r, fd_w = int(sys.argv[1]), int(sys.argv[2])

    # Read input
    idata = b''
    while True:
        rdata = os.read(fd_r, IPC_READ_BUFSIZE)
        if not rdata:
            break
        idata += rdata
    os.close(fd_r)
    idict = pickle.loads(idata)

    # Execute the query
    odict = query_inplace(**idict)

    # Write output
    odata = pickle.dumps(odict)
    os.write(fd_w, odata)
    os.close(fd_w)


if __name__ == '__main__':
    run()
