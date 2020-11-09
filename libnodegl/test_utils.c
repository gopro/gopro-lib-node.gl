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

#include "utils.h"
#include "memory.h"

static void test_numbered_line(uint32_t crc, const char *s)
{
    char *p = ngli_numbered_lines(s);
    ngli_assert(ngli_crc32(p) == crc);
    ngli_freep(&p);
}

int main(void)
{
    ngli_assert(ngli_crc32("") == 0);
    ngli_assert(ngli_crc32("Hello world !@#$%^&*()_+") == 0xDCEB8676);

    char buf[256];
    for (int i = 0; i <= 0xff; i++)
        buf[i] = 0xff - i;
    ngli_assert(ngli_crc32(buf) == 0x5473AA4D);

#define X "x\n"
#define S "foo\nbar\nhello\nworld\nbla\nxxx\nyyy\n"
    test_numbered_line(0x2d7f40af, S S S S S S S S);
    test_numbered_line(0xbcea3585, "\n");
    test_numbered_line(0xc58462d3, "foo\nbar");
    test_numbered_line(0x00000000, "");
    test_numbered_line(0x25b15360, X X X X X X X X X);
    test_numbered_line(0x759455a5, X X X X X X X X X X);
    return 0;
}
