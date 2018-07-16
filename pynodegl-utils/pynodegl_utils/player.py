#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

import time
from PyQt5 import QtCore

import pynodegl as ngl
from pynodegl_utils import misc


class Clock(object):

    TIMEBASE = 1000000000  # nanoseconds

    def __init__(self, framerate, duration):
        self._playback_index = 0
        self._running_time_offset = 0
        self._running = False
        self.configure(framerate, duration)

    def configure(self, framerate, duration):
        self._framerate = framerate
        self._duration = duration * self.TIMEBASE
        self.reset_running_time()

    def start(self):
        if self._running:
            return
        self.reset_running_time()
        self._running = True

    def stop(self):
        self._running = False

    def is_running(self):
        return self._running

    def reset_running_time(self):
        playback_time = self._playback_index * self.TIMEBASE * self._framerate[1] / self._framerate[0]
        self._running_time_offset = int(time.time() * self.TIMEBASE) - playback_time

    def get_playback_time_info(self):
        if not self._running:
            playback_time = self._playback_index * self._framerate[1] / float(self._framerate[0])
            return (self._playback_index, playback_time)

        playback_time = int(time.time() * self.TIMEBASE) - self._running_time_offset
        if playback_time < 0 or playback_time > self._duration:
            self._playback_index = 0
            self.reset_running_time()

        self._playback_index = int(round(
            playback_time * self._framerate[0] / float(self._framerate[1] * self.TIMEBASE)
        ))
        playback_time = self._playback_index * self._framerate[1] / float(self._framerate[0])

        return (self._playback_index, playback_time)

    def set_playback_time(self, time):
        self._playback_index = int(round(
            time * self._framerate[0] / self._framerate[1]
        ))
        self.reset_running_time()

    def step_playback_index(self, step):
        max_duration_index = int(round(
            self._duration * self._framerate[0] / float(self._framerate[1] * self.TIMEBASE)
        ))
        self._playback_index = min(max(self._playback_index + step, 0), max_duration_index)
        self.reset_running_time()


class Player(QtCore.QThread):

    on_play = QtCore.pyqtSignal(name='onPlay')
    on_pause = QtCore.pyqtSignal(name='onPause')
    on_scene_metadata = QtCore.pyqtSignal(dict, name='onSceneMetadata')
    on_frame = QtCore.pyqtSignal(int, float, name='onFrame')

    def __init__(self, window, width, height, config):
        super(Player, self).__init__()

        self._mutex = QtCore.QMutex()
        self._cond = QtCore.QWaitCondition()

        self._window = window
        self._width = width
        self._height = height

        self._scene = None
        self._framerate = config.get('framerate')
        self._duration = 0.0
        self._clear_color = config.get('clear_color')
        self._aspect_ratio = config.get('aspect_ratio')
        self._samples = config.get('samples')
        self._backend = config.get('backend')

        self._viewer = None
        self._events = []
        self._wait_first_frame = True
        self._clock = Clock(self._framerate, self._duration)
        self._viewer = ngl.Viewer()
        self._configure_viewer()

    def _configure_viewer(self):
        self._viewer.configure(
            platform=ngl.GLPLATFORM_AUTO,
            backend=misc.get_backend(self._backend),
            wrapped=0,
            window=self._window,
            width=self._width,
            height=self._height,
            viewport=misc.get_viewport(self._width, self._height, self._aspect_ratio),
            swap_interval=1,
            samples=self._samples,
            clear_color=self._clear_color,
        )

    def _render(self):
        frame_index, frame_time = self._clock.get_playback_time_info()
        self._viewer.draw(frame_time)
        if self._wait_first_frame:
            self._clock.reset_running_time()
            self._wait_first_frame = False
        self.onFrame.emit(frame_index, frame_time)

    def run(self):
        while True:
            self._render()
            should_stop = self._handle_events()
            if should_stop:
                break
        del self._viewer
        self._viewer = None

    def _handle_events(self):
        should_stop = False
        self._mutex.lock()
        if not self._events and not self._clock.is_running():
            self._cond.wait(self._mutex)
        while self._events:
            event = self._events.pop(0)
            should_stop = event()
            if should_stop:
                break
        self._mutex.unlock()
        return should_stop

    def _push_event(self, event):
        self._mutex.lock()
        self._events.append(event)
        self._cond.wakeAll()
        self._mutex.unlock()

    def draw(self):
        self._push_event(lambda: False)

    def play(self):
        self._push_event(lambda: self._play())

    def _play(self):
        if not self._scene:
            return False
        self._clock.start()
        self._wait_first_frame = True
        self.onPlay.emit()
        return False

    def pause(self):
        self._push_event(lambda: self._pause())

    def _pause(self):
        self._clock.stop()
        self.onPause.emit()
        return False

    def stop(self):
        self._push_event(lambda: self._stop())

    def _stop(self):
        self._is_stopped = True
        return True

    def seek(self, time):
        self._push_event(lambda: self._seek(time))

    def _seek(self, time):
        self._clock.set_playback_time(time)
        self._wait_first_frame = True
        return False

    def step(self, step):
        self._push_event(lambda: self._step(step))

    def _step(self, step):
        self._clock.step_playback_index(step)
        self._clock.stop()
        self.onPause.emit()
        return False

    def resize(self, width, height):
        self._push_event(lambda: self._resize(width, height))

    def _resize(self, width, height):
        self._width = width
        self._height = height
        self._configure_viewer()
        return False

    def set_scene(self, cfg):
        self._push_event(lambda: self._set_scene(cfg))

    def _set_scene(self, cfg):
        if cfg is None:
            self._pause()
            self._scene = None
            self._viewer.set_scene(None)
            return False
        self._scene = cfg['scene']
        self._framerate = cfg['framerate']
        self._duration = cfg['duration']
        self._clear_color = cfg['clear_color']
        self._aspect_ratio = cfg['aspect_ratio']
        self._backend = cfg['backend']
        self._samples = cfg['samples']
        self._viewer.set_scene_from_string(self._scene)
        self._configure_viewer()
        self._clock.configure(self._framerate, self._duration)
        self.onSceneMetadata.emit({'framerate': self._framerate, 'duration': self._duration})
        return False

    def reset_scene(self):
        self._push_event(lambda: self._set_scene(None))

    def set_aspect_ratio(self, aspect_ratio):
        self._push_event(lambda: self._set_aspect_ratio(aspect_ratio))

    def _set_aspect_ratio(self, aspect_ratio):
        self._aspect_ratio = aspect_ratio
        self._resize(self._width, self._height)
        return False

    def set_samples(self, samples):
        self._push_event(lambda: self._set_samples(samples))

    def _set_samples(self, samples):
        self._samples = samples
        self._configure_viewer()
        return False

    def set_clear_color(self, color):
        self._push_event(lambda: self._set_clear_color(color))

    def _set_clear_color(self, clear_color):
        self._clear_color = clear_color
        self._configure_viewer()
        return False

    def set_framerate(self, framerate):
        self._push_event(lambda: self._set_framerate(framerate))

    def _set_framerate(self, framerate):
        self._framerate = framerate
        self._clock.configure(self._framerate, self._duration)
        self.onSceneMetadata.emit({'framerate': self._framerate, 'duration': self._duration})
        return False

    def set_backend(self, backend):
        self._push_event(lambda: self._set_backend(backend))

    def _set_backend(self, backend):
        self._backend = backend
        self._configure_viewer()
        return False
