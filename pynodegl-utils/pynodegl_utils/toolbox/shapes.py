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


def equilateral_triangle_coords(d=1.0):
    '''
    Return the 3 coordinates of an equilateral triangle in that order:
    bottom-left, bottom-right, top-center. d is the diameter of the circle in
    which the centered triangle fits, r its radius.
    '''
    r = d * .5
    b = r * math.sqrt(3) * .5
    c = r * .5
    return (-b, -c, 0), (b, -c, 0), (0, r, 0)
