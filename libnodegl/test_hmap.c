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

#include <string.h>

#define HMAP_SIZE_NBIT 1
#include "hmap.c"
#include "memory.h"
#include "utils.h"

#define PRINT_HMAP(...) do {                                    \
    printf(__VA_ARGS__);                                        \
    const struct hmap_entry *e = NULL;                          \
    while ((e = ngli_hmap_next(hm, e)))                         \
        printf("  %s: %s\n", e->key, (const char *)e->data);    \
    printf("\n");                                               \
} while (0)

#define RSTR "replaced"

static void free_func(void *arg, void *data)
{
    ngli_free(data);
}

int main(void)
{
    static const struct {
        const char *key;
        const char *val;
    } kvs[] = {
        {"foo",     "bar"},
        {"hello",   "world"},
        {"lorem",   "ipsum"},
        {"bazbaz",  ""},
        {"abc",     "def"},
        {"last",    "samurai"},
    };

    for (int custom_alloc = 0; custom_alloc <= 1; custom_alloc++) {
        struct hmap *hm = ngli_hmap_create();

        if (custom_alloc)
            ngli_hmap_set_free(hm, free_func, NULL);

        /* Test addition */
        for (int i = 0; i < NGLI_ARRAY_NB(kvs); i++) {
            void *data = custom_alloc ? ngli_strdup(kvs[i].val) : (void*)kvs[i].val;
            ngli_assert(ngli_hmap_set(hm, kvs[i].key, data) >= 0);
            const char *val = ngli_hmap_get(hm, kvs[i].key);
            ngli_assert(val);
            ngli_assert(!strcmp(val, kvs[i].val));
        }

        PRINT_HMAP("init [%d entries] [custom_alloc:%s]:\n",
                   ngli_hmap_count(hm), custom_alloc ? "yes" : "no");

        for (int i = 0; i < NGLI_ARRAY_NB(kvs) - 1; i++) {

            /* Test replace */
            if (i & 1) {
                void *data = custom_alloc ? ngli_strdup(RSTR) : RSTR;
                ngli_assert(ngli_hmap_set(hm, kvs[i].key, data) == 0);
                const char *val = ngli_hmap_get(hm, kvs[i].key);
                ngli_assert(val);
                ngli_assert(strcmp(val, RSTR) == 0);
                PRINT_HMAP("replace %s:\n", kvs[i].key);
            }

            /* Test delete */
            ngli_assert(ngli_hmap_set(hm, kvs[i].key, NULL) == 1);
            ngli_assert(ngli_hmap_set(hm, kvs[i].key, NULL) == 0);
            PRINT_HMAP("drop %s (%d remaining):\n", kvs[i].key, ngli_hmap_count(hm));
        }

        ngli_hmap_freep(&hm);
    }

    return 0;
}
