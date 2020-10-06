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


class Clock:

    TIMEBASE = 1000000000  # nanoseconds

    def __init__(self, framerate, duration):
        self._playback_index = 0
        self.configure(framerate, duration)

    def configure(self, framerate, duration):
        self._framerate = framerate
        self._duration = duration * self.TIMEBASE

    def get_playback_time_info(self):
        playback_time = self._playback_index * self._framerate[1] / float(self._framerate[0])
        return (self._playback_index, playback_time)

    def set_playback_time(self, time):
        self._playback_index = int(round(
            time * self._framerate[0] / self._framerate[1]
        ))

    def step_playback_index(self, step):
        max_duration_index = int(round(
            self._duration * self._framerate[0] / float(self._framerate[1] * self.TIMEBASE)
        ))
        self._playback_index = min(max(self._playback_index + step, 0), max_duration_index)
