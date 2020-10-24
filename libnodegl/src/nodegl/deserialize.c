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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "darray.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"

#define CASE_LITERAL(param_type, type, parse_func)      \
case param_type: {                                      \
    type v;                                             \
    len = parse_func(str, &v);                          \
    if (len < 0)                                        \
        return NGL_ERROR_INVALID_DATA;                  \
    int ret = ngli_params_vset(base_ptr, par, v);       \
    if (ret < 0)                                        \
        return ret;                                     \
    break;                                              \
}

#define CASE_VEC(parse_func, vals, expected_nb_vals) do {       \
    int nb_vals;                                                \
    len = parse_func(str, &vals, &nb_vals);                     \
    if (len < 0 || nb_vals != expected_nb_vals) {               \
        ngli_free(vals);                                        \
        return NGL_ERROR_INVALID_DATA;                          \
    }                                                           \
    int ret = ngli_params_vset(base_ptr, par, vals);            \
    ngli_free(vals);                                            \
    if (ret < 0)                                                \
        return ret;                                             \
} while (0)


static int parse_int(const char *s, int *valp)
{
    char *endptr = NULL;
    *valp = (int)strtol(s, &endptr, 10);
    return (int)(endptr - s);
}

static int parse_uint(const char *s, unsigned *valp)
{
    char *endptr = NULL;
    *valp = (unsigned)strtol(s, &endptr, 10);
    return (int)(endptr - s);
}

static int parse_hexint(const char *s, int *valp)
{
    char *endptr = NULL;
    *valp = (int)strtol(s, &endptr, 16);
    return (int)(endptr - s);
}

static int64_t parse_i64(const char *s, int64_t *valp)
{
    char *endptr = NULL;
    *valp = strtoll(s, &endptr, 10);
    return (int)(endptr - s);
}

static int parse_bool(const char *s, int *valp)
{
    int ret = parse_int(s, valp);
    if (ret < 0)
        return ret;
    if (*valp != -1)
        *valp = !!*valp;
    return ret;
}

#define DECLARE_FLT_PARSE_FUNC(type, nbit, shift_exp, expected_z)           \
static int parse_##type(const char *s, type *valp)                          \
{                                                                           \
    int consumed = 0;                                                       \
    union { uint##nbit##_t i; type f; } u = {.i = 0};                       \
                                                                            \
    if (*s == '-') {                                                        \
        u.i = 1ULL << (nbit - 1);                                           \
        consumed++;                                                         \
    }                                                                       \
                                                                            \
    char *endptr = NULL;                                                    \
    uint##nbit##_t exp = nbit == 64 ? strtoull(s + consumed, &endptr, 16)   \
                                    : strtoul(s + consumed, &endptr, 16);   \
    if (*endptr++ != expected_z)                                            \
        return NGL_ERROR_INVALID_DATA;                                      \
    uint##nbit##_t mant = nbit == 64 ? strtoull(endptr, &endptr, 16)        \
                                     : strtoul(endptr, &endptr, 16);        \
                                                                            \
    u.i |= exp<<shift_exp | mant;                                           \
                                                                            \
    *valp = u.f;                                                            \
    return (int)(endptr - s);                                               \
}

DECLARE_FLT_PARSE_FUNC(float,  32, 23, 'z')
DECLARE_FLT_PARSE_FUNC(double, 64, 52, 'Z')

#define DECLARE_PARSE_LIST_FUNC(type, parse_func)                           \
static int parse_func##s(const char *s, type **valsp, int *nb_valsp)        \
{                                                                           \
    type *vals = NULL;                                                      \
    int nb_vals = 0, consumed = 0, len;                                     \
                                                                            \
    for (;;) {                                                              \
        type v;                                                             \
        len = parse_func(s, &v);                                            \
        if (len < 0) {                                                      \
            consumed = -1;                                                  \
            break;                                                          \
        }                                                                   \
        type *new_vals = ngli_realloc(vals,                                 \
                                      (nb_vals + 1) * sizeof(*new_vals));   \
        if (!new_vals) {                                                    \
            consumed = -1;                                                  \
            break;                                                          \
        }                                                                   \
        s += len;                                                           \
        consumed += len;                                                    \
        new_vals[nb_vals++] = v;                                            \
        vals = new_vals;                                                    \
        if (*s != ',')                                                      \
            break;                                                          \
        s++;                                                                \
        consumed++;                                                         \
    }                                                                       \
    if (consumed < 0) {                                                     \
        ngli_freep(&vals);                                                  \
        nb_vals = 0;                                                        \
    }                                                                       \
    *valsp = vals;                                                          \
    *nb_valsp = nb_vals;                                                    \
    return consumed;                                                        \
}

DECLARE_PARSE_LIST_FUNC(float,    parse_float)
DECLARE_PARSE_LIST_FUNC(double,   parse_double)
DECLARE_PARSE_LIST_FUNC(int,      parse_hexint)
DECLARE_PARSE_LIST_FUNC(int,      parse_int)
DECLARE_PARSE_LIST_FUNC(unsigned, parse_uint)

#define FREE_KVS(count, keys, vals) do {                                    \
    for (int k = 0; k < (count); k++)                                       \
        ngli_free((keys)[k]);                                               \
    ngli_freep(&keys);                                                      \
    ngli_freep(&vals);                                                      \
} while (0)

static int parse_kvs(const char *s, int *nb_kvsp, char ***keysp, int **valsp)
{
    char **keys = NULL;
    int *vals = NULL;
    int nb_vals = 0, consumed = 0, len;

    for (;;) {
        char key[63 + 1];
        int val;
        int n = sscanf(s, "%63[^=]=%x%n", key, &val, &len);
        if (n != 2) {
            consumed = -1;
            break;
        }

        char **new_keys = ngli_realloc(keys, (nb_vals + 1) * sizeof(*new_keys));
        if (!new_keys) {
            consumed = -1;
            break;
        }
        keys = new_keys;

        int *new_vals = ngli_realloc(vals, (nb_vals + 1) * sizeof(*new_vals));
        if (!new_vals) {
            consumed = -1;
            break;
        }
        vals = new_vals;

        s += len;
        consumed += len;
        keys[nb_vals] = ngli_strdup(key);
        vals[nb_vals] = val;
        nb_vals++;
        if (*s != ',')
            break;
        s++;
        consumed++;
    }
    if (consumed < 0) {
        FREE_KVS(nb_vals, keys, vals);
        nb_vals = 0;
    }
    *keysp = keys;
    *valsp = vals;
    *nb_kvsp = nb_vals;
    return consumed;
}

static struct ngl_node **get_abs_node(struct darray *nodes_array, int id)
{
    return ngli_darray_get(nodes_array, ngli_darray_count(nodes_array) - id - 1);
}

static const uint8_t hexm[256] = {
    ['0'] = 0x0, ['1'] = 0x1, ['2'] = 0x2, ['3'] = 0x3,
    ['4'] = 0x4, ['5'] = 0x5, ['6'] = 0x6, ['7'] = 0x7,
    ['8'] = 0x8, ['9'] = 0x9, ['a'] = 0xa, ['b'] = 0xb,
    ['c'] = 0xc, ['d'] = 0xd, ['e'] = 0xe, ['f'] = 0xf,
};

#define CHR_FROM_HEX(s) (hexm[(uint8_t)(s)[0]]<<4 | hexm[(uint8_t)(s)[1]])

static int parse_param(struct darray *nodes_array, uint8_t *base_ptr,
                       const struct node_param *par, const char *str)
{
    int len = -1;

    switch (par->type) {
        CASE_LITERAL(PARAM_TYPE_INT,  int,      parse_int)
        CASE_LITERAL(PARAM_TYPE_UINT, unsigned, parse_uint)
        CASE_LITERAL(PARAM_TYPE_BOOL, int,      parse_bool)
        CASE_LITERAL(PARAM_TYPE_I64,  int64_t,  parse_i64)
        CASE_LITERAL(PARAM_TYPE_DBL,  double,   parse_double)

        case PARAM_TYPE_RATIONAL: {
            int r[2] = {0};
            int ret = sscanf(str, "%d/%d%n", &r[0], &r[1], &len);
            if (ret != 2)
                return NGL_ERROR_INVALID_DATA;
            ret = ngli_params_vset(base_ptr, par, r[0], r[1]);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_FLAGS:
        case PARAM_TYPE_SELECT: {
            len = strcspn(str, " \n");
            char *s = ngli_malloc(len + 1);
            if (!s)
                return NGL_ERROR_MEMORY;
            memcpy(s, str, len);
            s[len] = 0;
            int ret = ngli_params_vset(base_ptr, par, s);
            ngli_free(s);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_STR: {
            len = strcspn(str, " \n");
            char *s = ngli_malloc(len + 1);
            if (!s)
                return NGL_ERROR_MEMORY;
            char *sstart = s;
            for (int i = 0; i < len; i++) {
                if (str[i] == '%' && i + 2 < len) {
                    *s++ = CHR_FROM_HEX(str + i + 1);
                    i += 2;
                } else {
                    *s++ = str[i];
                }
            }
            *s = 0;
            int ret = ngli_params_vset(base_ptr, par, sstart);
            ngli_free(sstart);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_DATA: {
            int size = 0;
            int consumed = 0;
            const char *cur = str;
            const char *end = str + strlen(str);
            int ret = sscanf(str, "%d,%n", &size, &consumed);
            if (ret != 1)
                return NGL_ERROR_INVALID_DATA;
            if (!size)
                break;
            if (cur >= end - consumed)
                return NGL_ERROR_INVALID_DATA;
            cur += consumed;
            uint8_t *data = ngli_calloc(size, sizeof(*data));
            if (!data)
                return NGL_ERROR_MEMORY;
            for (int i = 0; i < size; i++) {
                if (cur > end - 2) {
                    ngli_free(data);
                    return NGL_ERROR_INVALID_DATA;
                }
                data[i] = CHR_FROM_HEX(cur);
                cur += 2;
            }
            ret = ngli_params_vset(base_ptr, par, size, data);
            ngli_free(data);
            if (ret < 0)
                return ret;
            len = cur - str;
            break;
        }

        case PARAM_TYPE_IVEC2:
        case PARAM_TYPE_IVEC3:
        case PARAM_TYPE_IVEC4: {
            int *iv = NULL;
            CASE_VEC(parse_ints, iv, par->type - PARAM_TYPE_IVEC2 + 2);
            break;
        }

        case PARAM_TYPE_UIVEC2:
        case PARAM_TYPE_UIVEC3:
        case PARAM_TYPE_UIVEC4: {
            unsigned *uv = NULL;
            CASE_VEC(parse_uints, uv, par->type - PARAM_TYPE_UIVEC2 + 2);
            break;
        }

        case PARAM_TYPE_VEC2:
        case PARAM_TYPE_VEC3:
        case PARAM_TYPE_VEC4: {
            float *v = NULL;
            CASE_VEC(parse_floats, v, par->type - PARAM_TYPE_VEC2 + 2);
            break;
        }

        case PARAM_TYPE_MAT4: {
            float *m = NULL;
            CASE_VEC(parse_floats, m, 16);
            break;
        }

        case PARAM_TYPE_NODE: {
            int node_id;
            len = parse_hexint(str, &node_id);
            if (len < 0)
                return NGL_ERROR_INVALID_DATA;
            struct ngl_node **nodep = get_abs_node(nodes_array, node_id);
            if (!nodep)
                return NGL_ERROR_INVALID_DATA;
            int ret = ngli_params_vset(base_ptr, par, *nodep);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_NODELIST: {
            int *node_ids, nb_node_ids;
            len = parse_hexints(str, &node_ids, &nb_node_ids);
            if (len < 0)
                return len;
            for (int i = 0; i < nb_node_ids; i++) {
                struct ngl_node **nodep = get_abs_node(nodes_array, node_ids[i]);
                if (!nodep) {
                    ngli_free(node_ids);
                    return NGL_ERROR_INVALID_DATA;
                }
                int ret = ngli_params_add(base_ptr, par, 1, nodep);
                if (ret < 0) {
                    ngli_free(node_ids);
                    return ret;
                }
            }
            ngli_free(node_ids);
            break;
        }

        case PARAM_TYPE_DBLLIST: {
            double *dbls;
            int nb_dbls;
            len = parse_doubles(str, &dbls, &nb_dbls);
            if (len < 0)
                return len;
            int ret = ngli_params_add(base_ptr, par, nb_dbls, dbls);
            ngli_free(dbls);
            if (ret < 0)
                return ret;
            break;
        }

        case PARAM_TYPE_NODEDICT: {
            char **node_keys;
            int *node_ids, nb_nodes;
            len = parse_kvs(str, &nb_nodes, &node_keys, &node_ids);
            if (len < 0)
                return len;
            for (int i = 0; i < nb_nodes; i++) {
                const char *key = node_keys[i];
                struct ngl_node **nodep = get_abs_node(nodes_array, node_ids[i]);
                if (!nodep) {
                    FREE_KVS(nb_nodes, node_keys, node_ids);
                    return NGL_ERROR_INVALID_DATA;
                }
                int ret = ngli_params_vset(base_ptr, par, key, *nodep);
                if (ret < 0) {
                    FREE_KVS(nb_nodes, node_keys, node_ids);
                    return ret;
                }
            }
            FREE_KVS(nb_nodes, node_keys, node_ids);
            break;
        }

        default:
            LOG(ERROR, "cannot deserialize %s: "
                "unsupported parameter type", par->key);
    }
    return len;
}

static int set_node_params(struct darray *nodes_array, char *str,
                           const struct ngl_node *node)
{
    uint8_t *base_ptr = node->priv_data;
    const struct node_param *params = node->class->params;

    if (!params)
        return 0;

    for (;;) {
        char *eok = strchr(str, ':');
        if (!eok)
            break;
        *eok = 0;

        const struct node_param *par = ngli_node_param_find(node, str, &base_ptr);
        if (!par) {
            LOG(ERROR, "unable to find parameter %s.%s",
                node->class->name, str);
            return NGL_ERROR_INVALID_DATA;
        }

        str = eok + 1;
        int ret = parse_param(nodes_array, base_ptr, par, str);
        if (ret < 0) {
            LOG(ERROR, "unable to set node param %s.%s: %s",
                node->class->name, par->key, NGLI_RET_STR(ret));
            return ret;
        }

        str += ret;
        if (*str != ' ')
            break;
        str++;
    }

    return 0;
}

struct ngl_node *ngl_node_deserialize(const char *str)
{
    struct ngl_node *node = NULL;
    struct darray nodes_array;

    ngli_darray_init(&nodes_array, sizeof(struct ngl_node *), 0);

    char *s = ngli_strdup(str);
    if (!s)
        return NULL;

    char *sstart = s;
    char *send = s + strlen(s);

    int major, minor, micro;
    int n = sscanf(s, "# Node.GL v%d.%d.%d", &major, &minor, &micro);
    if (n != 3) {
        LOG(ERROR, "invalid serialized scene");
        goto end;
    }
    if (NODEGL_VERSION_INT != NODEGL_GET_VERSION(major, minor, micro)) {
        LOG(ERROR, "mismatching version: %d.%d.%d != %d.%d.%d",
            major, minor, micro,
            NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
        goto end;
    }
    s += strcspn(s, "\n");
    if (*s == '\n')
        s++;

    while (s < send - 4) {
        const int type = NGLI_FOURCC(s[0], s[1], s[2], s[3]);
        s += 4;
        if (*s == ' ')
            s++;

        node = ngl_node_create(type);
        if (!node)
            break;

        if (!ngli_darray_push(&nodes_array, &node)) {
            ngl_node_unrefp(&node);
            break;
        }

        size_t eol = strcspn(s, "\n");
        s[eol] = 0;

        int ret = set_node_params(&nodes_array, s, node);
        if (ret < 0) {
            node = NULL;
            break;
        }

        s += eol + 1;
    }

    if (node)
        ngl_node_ref(node);

    struct ngl_node **nodes = ngli_darray_data(&nodes_array);
    for (int i = 0; i < ngli_darray_count(&nodes_array); i++)
        ngl_node_unrefp(&nodes[i]);

end:
    ngli_darray_reset(&nodes_array);
    ngli_free(sstart);
    return node;
}
