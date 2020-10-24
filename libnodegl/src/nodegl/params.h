/*
 * Copyright 2016 GoPro Inc.
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

#ifndef PARAMS_H
#define PARAMS_H

#include <stdarg.h>

#include "bstr.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PARAM_TYPE_INT,
    PARAM_TYPE_IVEC2,
    PARAM_TYPE_IVEC3,
    PARAM_TYPE_IVEC4,
    PARAM_TYPE_BOOL,
    PARAM_TYPE_UINT,
    PARAM_TYPE_UIVEC2,
    PARAM_TYPE_UIVEC3,
    PARAM_TYPE_UIVEC4,
    PARAM_TYPE_I64,
    PARAM_TYPE_DBL,
    PARAM_TYPE_STR,
    PARAM_TYPE_DATA,
    PARAM_TYPE_VEC2,
    PARAM_TYPE_VEC3,
    PARAM_TYPE_VEC4,
    PARAM_TYPE_MAT4,
    PARAM_TYPE_NODE,
    PARAM_TYPE_NODELIST,
    PARAM_TYPE_DBLLIST,
    PARAM_TYPE_NODEDICT,
    PARAM_TYPE_SELECT,
    PARAM_TYPE_FLAGS,
    PARAM_TYPE_RATIONAL,
    NB_PARAMS
};

struct param_specs {
    const char *name;
    size_t size;
    const char *desc;
};

struct param_const {
    const char *key;
    int value;
    const char *desc;
};

struct param_choices {
    const char *name;
    const struct param_const consts[];
};

#define PARAM_FLAG_NON_NULL (1<<0)
#define PARAM_FLAG_DOT_DISPLAY_PACKED (1<<1)
#define PARAM_FLAG_DOT_DISPLAY_FIELDNAME (1<<2)
#define PARAM_FLAG_ALLOW_LIVE_CHANGE (1<<3)
struct node_param {
    const char *key;
    int type;
    int offset;
    union {
        int64_t i64;
        double dbl;
        const char *str;
        void *p;
        float vec[4];
        int ivec[4];
        unsigned uvec[4];
        float mat[4*4];
        int r[2];
    } def_value;
    int flags;
    const int *node_types;
    const char *desc;
    const struct param_choices *choices;
    int (*update_func)(struct ngl_node *node);
};

int ngli_params_get_select_val(const struct param_const *consts, const char *s, int *dst);
const char *ngli_params_get_select_str(const struct param_const *consts, int val);
int ngli_params_get_flags_val(const struct param_const *consts, const char *s, int *dst);
char *ngli_params_get_flags_str(const struct param_const *consts, int val);
const struct node_param *ngli_params_find(const struct node_param *params, const char *key);
void ngli_params_bstr_print_val(struct bstr *b, uint8_t *base_ptr, const struct node_param *par);
int ngli_params_set(uint8_t *base_ptr, const struct node_param *par, va_list *ap);
int ngli_params_vset(uint8_t *base_ptr, const struct node_param *par, ...);
int ngli_params_set_defaults(uint8_t *base_ptr, const struct node_param *params);
int ngli_params_add(uint8_t *base_ptr, const struct node_param *par, int nb_elems, void *elems);
void ngli_params_free(uint8_t *base_ptr, const struct node_param *params);

#ifdef __cplusplus
}
#endif

#endif
