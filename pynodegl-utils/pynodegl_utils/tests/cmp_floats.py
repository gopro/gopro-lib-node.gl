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

from .cmp import CompareBase, get_test_decorator


class _CompareFloats(CompareBase):

    def __init__(self, func, tolerance=0.001, **func_kwargs):
        self._func = func
        self._func_kwargs = func_kwargs
        self._tolerance = tolerance

    @staticmethod
    def serialize(data):
        ret = ''
        for float_set in data:
            ret += '%s: %s\n' % (float_set[0], ' '.join('%f' % f for f in float_set[1:]))
        return ret

    @staticmethod
    def deserialize(data):
        ret = []
        for line in data.splitlines():
            name, floats = line.split(':', 1)
            ret.append([name] + [float(f) for f in floats.split()])
        return ret

    def get_out_data(self):
        return self._func(**self._func_kwargs)

    def compare_data(self, test_name, ref_data, out_data):
        err = []
        for float_set_id, (ref_floats, out_floats) in enumerate(zip(ref_data, out_data)):
            name = ref_floats[0]
            if name != out_floats[0]:
                err.append('{} float sets {} have different names: "{}" vs "{}"'.format(
                    test_name, float_set_id, ref_floats[0], out_floats[0]))
            assert len(ref_floats) == len(out_floats)
            for i, (f0, f1) in enumerate(zip(ref_floats[1:], out_floats[1:])):
                diff = abs(f0 - f1)
                if diff > self._tolerance:
                    err.append('{} diff too large for float {}[{}]: |{}-{}|={} (>{})'.format(
                        test_name, name, i, f0, f1, diff, self._tolerance))
        return err


test_floats = get_test_decorator(_CompareFloats)
