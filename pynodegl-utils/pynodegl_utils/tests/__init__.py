#!/usr/bin/env python
#
# Copyright 2020 GoPro Inc.
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

import os
import os.path as op
import sys

from pynodegl_utils.com import load_script


def _run_test(func_name, tester, ref_data, out_data):
    err = []
    if len(ref_data) != len(out_data):
        err = ['{}: data len mismatch (ref:{} out:{})'.format(func_name, len(ref_data), len(out_data))]
    err += tester.compare_data(func_name, ref_data, out_data)
    return err


def _set_ref_data(tester, ref_filepath, data):
    with open(ref_filepath, "w") as ref_file:
        ref_file.write(tester.serialize(data))


def _get_ref_data(tester, ref_filepath):
    with open(ref_filepath) as ref_file:
        serialized_data = ref_file.read()
    return tester.deserialize(serialized_data)


def _run_test_default(func_name, tester, ref_filepath):
    if not op.exists(ref_filepath):
        sys.stderr.write('{}: reference file {} not found, use GEN=yes to create it\n'.format(func_name, ref_filepath))
        sys.exit(1)
    ref_data = _get_ref_data(tester, ref_filepath)
    out_data = tester.get_out_data()
    return _run_test(func_name, tester, ref_data, out_data)


def _run_test_gen_yes(func_name, tester, ref_filepath):
    out_data = tester.get_out_data()
    if not op.exists(ref_filepath):
        sys.stderr.write('{}: creating {}\n'.format(func_name, ref_filepath))
        _set_ref_data(tester, ref_filepath, out_data)
        return None
    ref_data = _get_ref_data(tester, ref_filepath)
    return _run_test(func_name, tester, ref_data, out_data)


def _run_test_gen_update(func_name, tester, ref_filepath):
    out_data = tester.get_out_data()
    if not op.exists(ref_filepath):
        sys.stderr.write('{}: creating {}\n'.format(func_name, ref_filepath))
        _set_ref_data(tester, ref_filepath, out_data)
        return None
    ref_data = _get_ref_data(tester, ref_filepath)
    err = _run_test(func_name, tester, ref_data, out_data)
    if err:
        sys.stderr.write('{}: re-generating {}\n'.format(func_name, ref_filepath))
        _set_ref_data(tester, ref_filepath, out_data)
    return None


def _run_test_gen_force(func_name, tester, ref_filepath):
    if not op.exists(ref_filepath):
        sys.stderr.write('{}: creating {}\n'.format(func_name, ref_filepath))
    else:
        sys.stderr.write('{}: re-generating {}\n'.format(func_name, ref_filepath))
    out_data = tester.get_out_data()
    _set_ref_data(tester, ref_filepath, out_data)
    return []


_gen_map = {
    'yes': _run_test_gen_yes,
    'update': _run_test_gen_update,
    'force': _run_test_gen_force,
}


def run():
    gen_opt = os.environ.get('GEN')

    allowed_gen_opt = ('yes', 'update', 'force')
    if gen_opt is not None and gen_opt not in allowed_gen_opt:
        sys.stderr.write('GEN environment variable must be any of {}\n'.format(', '.join(allowed_gen_opt)))
        sys.exit(1)

    if len(sys.argv) != 4:
        sys.stderr.write('''Usage: [GEN={}] {} <script_path> <func_name> <ref_filepath>

    GEN=yes     Create the reference file if not present
    GEN=update  Same as "yes" and update the reference if the test fails
    GEN=force   Same as "yes" and always replace the reference file. This may
                change the references even if the tests are passing due to the
                threshold/tolerance mechanism.
'''.format('|'.join(allowed_gen_opt), op.basename(sys.argv[0])))
        sys.exit(1)

    script_path, func_name, ref_filepath = sys.argv[1:4]
    module = load_script(script_path)
    func = getattr(module, func_name)
    tester = func.tester

    test_func = _gen_map.get(gen_opt, _run_test_default)
    err = test_func(func_name, tester, ref_filepath)
    if err:
        sys.stderr.write('\n'.join(err) + '\n')
        sys.exit(1)

    sys.exit(0)
