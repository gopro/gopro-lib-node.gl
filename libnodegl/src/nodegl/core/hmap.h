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

#ifndef HMAP_H
#define HMAP_H

#ifndef HMAP_SIZE_NBIT
#define HMAP_SIZE_NBIT 3
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct hmap;

struct hmap_entry {
    char *key;
    void *data;
    int bucket_id;
};

typedef void (*user_free_func_type)(void *user_arg, void *data);

struct hmap *ngli_hmap_create(void);
void ngli_hmap_set_free(struct hmap *hm, user_free_func_type user_free_func, void *user_arg);
int ngli_hmap_count(const struct hmap *hm);
int ngli_hmap_set(struct hmap *hm, const char *key, void *data);
void *ngli_hmap_get(const struct hmap *hm, const char *key);
const struct hmap_entry *ngli_hmap_next(const struct hmap *hm,
                                        const struct hmap_entry *prev);
void ngli_hmap_freep(struct hmap **hmp);

#ifdef __cplusplus
}
#endif

#endif
