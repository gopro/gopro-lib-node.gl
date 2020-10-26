/*
 * Copyright 2016-2017 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef ANDROID_LOOPER_H
#define ANDROID_LOOPER_H

struct android_looper;
struct android_looper *ngli_android_looper_new(void);
int ngli_android_looper_prepare(struct android_looper *looper);
int ngli_android_looper_loop(struct android_looper *looper);
int ngli_android_looper_quit(struct android_looper *looper);
void ngli_android_looper_free(struct android_looper **looper);

#endif /* ANDROID_LOOPER_H */
