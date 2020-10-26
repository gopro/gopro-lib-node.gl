/*
 * Copyright 2020 GoPro Inc.
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

#include "nodegl/porting/ngfx/gtimer_ngfx.h"
#include "nodegl/memory.h"
#include "nodegl/log.h"

struct gtimer *ngli_gtimer_ngfx_create(struct gctx *gctx)
{ TODO();
    return NULL;
}

int ngli_gtimer_ngfx_init(struct gtimer *s) { TODO(); return 0; }
int ngli_gtimer_ngfx_start(struct gtimer *s) { TODO(); return 0; }
int ngli_gtimer_ngfx_stop(struct gtimer *s) { TODO(); return 0; }
int64_t ngli_gtimer_ngfx_read(struct gtimer *s) { TODO(); return 0; }
void ngli_gtimer_ngfx_freep(struct gtimer **sp) { TODO();}
