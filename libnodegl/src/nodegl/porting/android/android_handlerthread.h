/*
 * Copyright 2017 GoPro Inc.
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

#ifndef ANDROID_HANDLERTHREAD_H
#define ANDROID_HANDLERTHREAD_H

struct android_handlerthread;
struct android_handlerthread *ngli_android_handlerthread_new(void);
void *ngli_android_handlerthread_get_native_handler(struct android_handlerthread *thread);
void ngli_android_handlerthread_free(struct android_handlerthread **thread);

#endif /* ANDROID_HANDLERTHREAD_H */
