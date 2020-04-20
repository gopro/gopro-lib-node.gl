#!/usr/bin/env python
#
# Copyright 2016 GoPro Inc.
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

from PySide2 import QtWidgets, QtCore

from .ui.main_window import MainWindow


def run():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('-m', dest='module', default='pynodegl_utils.examples',
                        help='set the module name containing the scene functions')
    parser.add_argument('--hooks-dir', dest='hooksdir',
                        help='set the directory path containing event hooks')
    pargs = parser.parse_args(sys.argv[1:])

    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow(pargs.module, pargs.hooksdir)
    window.show()
    app.exec_()
