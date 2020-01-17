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

import math
import pynodegl as ngl


class AutoGrid:

    def __init__(self, elems):
        self.elems = elems
        self.nb = len(elems)
        self.nb_rows = int(round(math.sqrt(self.nb)))
        self.nb_cols = int(math.ceil(self.nb / float(self.nb_rows)))
        self.dim = max(self.nb_rows, self.nb_cols)
        self.scale = 1. / self.dim

    def _get_coords(self, pos):
        col, row = pos
        pos_x = self.scale * (col *  2. + 1.) - 1.
        pos_y = self.scale * (row * -2. - 1.) + 1.
        return pos_x, pos_y, 0.0

    def transform_coords(self, coords, pos):
        adjust = self._get_coords(pos)
        scales = [self.scale, self.scale, 1.0]
        return [c * s + a for c, s, a in zip(coords, scales, adjust)]

    def place_node(self, node, pos):
        pos_x, pos_y, pos_z = self._get_coords(pos)
        # This is equivalent to Translate(Scale(node))
        mat = [
            self.scale, 0, 0, 0,
            0, self.scale, 0, 0,
            0, 0, 1, 0,
            pos_x, pos_y, pos_z, 1,
        ]
        return ngl.Transform(node, matrix=mat, label='grid(col=%d,row=%d)' % pos)

    def __iter__(self):
        i = 0
        for row in range(self.nb_rows):
            for col in range(self.nb_cols):
                yield self.elems[i], i, col, row
                i += 1
                if i == self.nb:
                    return


def autogrid_simple(scenes):
    ag = AutoGrid(scenes)
    g = ngl.Group()
    for scene, scene_id, col, row in ag:
        scene = ag.place_node(scene, (col, row))
        g.add_children(scene)
    return g


def autogrid_queue(scenes, duration, overlap_time):
    ag = AutoGrid(scenes)
    g = ngl.Group()
    for scene, scene_id, col, row in ag:
        if duration is not None:
            assert overlap_time is not None
            scene = ngl.TimeRangeFilter(scene)
            start = scene_id * duration / ag.nb
            if start:
                scene.add_ranges(ngl.TimeRangeModeNoop(0))
            scene.add_ranges(ngl.TimeRangeModeCont(start),
                             ngl.TimeRangeModeNoop(start + duration/ag.nb + overlap_time))
        scene = ag.place_node(scene, (col, row))
        g.add_children(scene)
    return g


if __name__ == '__main__':
    N = 10
    for n in range(1, N * N + 1):
        ag = AutoGrid(range(n))
        buf = [['.'] * ag.nb_cols for i in range(ag.nb_rows)]
        for _, i, x, y in ag:
            buf[y][x] = 'x'
        print '#%d (cols:%d rows:%d)' % (n, ag.nb_cols, ag.nb_rows)
        print '\n'.join(''.join(line) for line in buf)
        print
