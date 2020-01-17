#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

import pynodegl as ngl
from pynodegl_utils.misc import scene, Media
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynodegl_utils.tests.cmp_resources import test_resources


def _get_time_scene(cfg):
    m0 = cfg.medias[0]

    media_seek        = 10
    noop_duration     = 2
    prefetch_duration = 2
    freeze_duration   = 3
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start  = range_start   + freeze_duration
    play_stop   = play_start    + playback_duration
    range_stop  = play_stop     + freeze_duration
    duration    = range_stop    + noop_duration

    cfg.duration = duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop,  media_seek + playback_duration),
    ]

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m)
    r = ngl.Render(q)
    r.update_textures(tex0=t)

    time_ranges = [
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(range_start),
        ngl.TimeRangeModeNoop(range_stop),
    ]
    rf = ngl.TimeRangeFilter(r, ranges=time_ranges, prefetch_time=prefetch_duration)

    return rf

@scene()
def flat_remap(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]

    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(-0.4, 1.833),
        ngl.AnimKeyFrameFloat(cfg.duration, 1.833),
    ]

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m)
    render = ngl.Render(q)
    render.update_textures(tex0=t)
    return render


@test_cuepoints(points={'X': (0, -0.625)}, nb_keyframes=15, clear_color=COLORS['violet'])
@scene()
def media_phases_display(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]
    return _get_time_scene(cfg)


@test_resources(nb_keyframes=15)
@scene()
def media_phases_resources(cfg):
    cfg.medias = [Media('ngl-media-test.nut')]
    return _get_time_scene(cfg)
