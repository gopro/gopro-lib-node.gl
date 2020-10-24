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

#ifdef __ELF__
/* non-exec stack */
.section .note.GNU-stack, "", %progbits
#endif

.text

#ifdef __APPLE__
# define FUNCPREFIX _ngli_
#else
# define FUNCPREFIX ngli_
#endif

#define FUNCSUFFIX _aarch64

.macro func name
    .align 2
    .global FUNCPREFIX\name\()FUNCSUFFIX
#ifdef __ELF__
    .type FUNCPREFIX\name\()FUNCSUFFIX, %function
#endif
FUNCPREFIX\()\name\()FUNCSUFFIX:
.endm

.macro endfunc
.endm

func mat4_mul
    ld1     {v0.4S-v3.4S}, [x1]
    ld1     {v4.4S-v7.4S}, [x2]

    fmul    v16.4S, v0.4S, v4.S[0]
    fmul    v17.4S, v0.4S, v5.S[0]
    fmul    v18.4S, v0.4S, v6.S[0]
    fmul    v19.4S, v0.4S, v7.S[0]

    fmla    v16.4S, v1.4S, v4.S[1]
    fmla    v17.4S, v1.4S, v5.S[1]
    fmla    v18.4S, v1.4S, v6.S[1]
    fmla    v19.4S, v1.4S, v7.S[1]

    fmla    v16.4S, v2.4S, v4.S[2]
    fmla    v17.4S, v2.4S, v5.S[2]
    fmla    v18.4S, v2.4S, v6.S[2]
    fmla    v19.4S, v2.4S, v7.S[2]

    fmla    v16.4S, v3.4S, v4.S[3]
    fmla    v17.4S, v3.4S, v5.S[3]
    fmla    v18.4S, v3.4S, v6.S[3]
    fmla    v19.4S, v3.4S, v7.S[3]

    st1     {v16.4S-v19.4S}, [x0]
    ret
endfunc

func mat4_mul_vec4
    ld1     {v0.4S-v3.4S}, [x1]
    ld1     {v4.4S},       [x2]

    fmul    v5.4S, v0.4S, v4.S[0]
    fmla    v5.4S, v1.4S, v4.S[1]
    fmla    v5.4S, v2.4S, v4.S[2]
    fmla    v5.4S, v3.4S, v4.S[3]

    st1     {v5.4S}, [x0]
    ret
endfunc
