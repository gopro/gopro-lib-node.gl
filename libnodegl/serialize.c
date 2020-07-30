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
#include <string.h>

#include "bstr.h"
#include "darray.h"
#include "hmap.h"
#include "log.h"
#include "memory.h"
#include "nodes.h"
#include "nodegl.h"
#include "utils.h"

extern const struct node_param ngli_base_node_params[];

static void free_func(void *arg, void *data)
{
    ngli_free(data);
}

static int register_node(struct hmap *nlist,
                          const struct ngl_node *node)
{
    char key[32];
    int ret = snprintf(key, sizeof(key), "%p", node);
    if (ret < 0)
        return ret;
    char *val = ngli_asprintf("%x", ngli_hmap_count(nlist));
    if (!val)
        return NGL_ERROR_MEMORY;
    ret = ngli_hmap_set(nlist, key, val);
    if (ret < 0)
        ngli_free(val);
    return ret;
}

static int get_node_id(const struct hmap *nlist, const struct ngl_node *node)
{
    char key[32];
    (void)snprintf(key, sizeof(key), "%p", node);
    const char *val = ngli_hmap_get(nlist, key);
    return val ? strtol(val, NULL, 16) : -1;
}

static int get_rel_node_id(const struct hmap *nlist, const struct ngl_node *node)
{
    return ngli_hmap_count(nlist) - get_node_id(nlist, node);
}

#define DECLARE_FLT_PRINT_FUNC(type, nbit, shift_exp, z)                \
static void print_##type(struct bstr *b, type f)                        \
{                                                                       \
    const union { uint##nbit##_t i; type f; } u = {.f = f};             \
    const uint##nbit##_t v = u.i;                                       \
    const int sign = v >> (nbit - 1);                                   \
    const uint##nbit##_t exp_mask = (1 << (nbit - shift_exp - 1)) - 1;  \
    const uint##nbit##_t exp  = v >> shift_exp & exp_mask;              \
    const uint##nbit##_t mant = v & ((1ULL << shift_exp) - 1);          \
    ngli_bstr_printf(b, "%s%" PRIX##nbit "%c%" PRIX##nbit,              \
                    sign ? "-" : "", exp, z, mant);                     \
}                                                                       \

DECLARE_FLT_PRINT_FUNC(float,  32, 23, 'z')
DECLARE_FLT_PRINT_FUNC(double, 64, 52, 'Z')

#define print_int(b, v)      ngli_bstr_printf(b, "%d", v)
#define print_unsigned(b, v) ngli_bstr_printf(b, "%u", v)

#define DECLARE_PRINT_FUNC(type)                                        \
static void print_##type##s(struct bstr *b, int n, const type *v)       \
{                                                                       \
    for (int i = 0; i < n; i++) {                                       \
        ngli_bstr_printf(b, "%s", i ? "," : "");                        \
        print_##type(b, v[i]);                                          \
    }                                                                   \
}

DECLARE_PRINT_FUNC(float)
DECLARE_PRINT_FUNC(double)
DECLARE_PRINT_FUNC(int)
DECLARE_PRINT_FUNC(unsigned)

struct item {
    const char *key;
    void *data;
};

static int cmp_item(const void *p1, const void *p2)
{
    const struct item *i1 = p1;
    const struct item *i2 = p2;
    return strcmp(i1->key, i2->key);
}

static int hmap_to_sorted_items(struct darray *items_array, struct hmap *hm)
{
    ngli_darray_init(items_array, sizeof(struct item), 0);

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(hm, entry))) {
        struct item item = {.key = entry->key, .data = entry->data};
        if (!ngli_darray_push(items_array, &item)) {
            ngli_darray_reset(items_array);
            return NGL_ERROR_MEMORY;
        }
    }

    void *items = ngli_darray_data(items_array);
    const int nb_items = ngli_darray_count(items_array);
    qsort(items, nb_items, sizeof(struct item), cmp_item);

    return 0;
}

static int serialize_options(struct hmap *nlist,
                             struct bstr *b,
                             const struct ngl_node *node,
                             uint8_t *priv,
                             const struct node_param *p)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_SELECT: {
                const int v = *(int *)(priv + p->offset);
                const char *s = ngli_params_get_select_str(p->choices->consts, v);
                ngli_assert(s);
                if (v != p->def_value.i64)
                    ngli_bstr_printf(b, " %s:%s", p->key, s);
                break;
            }
            case PARAM_TYPE_FLAGS: {
                const int v = *(int *)(priv + p->offset);
                char *s = ngli_params_get_flags_str(p->choices->consts, v);
                if (!s) {
                    LOG(ERROR, "unable to allocate param flags string");
                    return NGL_ERROR_MEMORY;
                }
                ngli_assert(*s);
                if (v != p->def_value.i64)
                    ngli_bstr_printf(b, " %s:%s", p->key, s);
                ngli_free(s);
                break;
            }
            case PARAM_TYPE_BOOL:
            case PARAM_TYPE_INT: {
                const int v = *(int *)(priv + p->offset);
                if (v != p->def_value.i64)
                    ngli_bstr_printf(b, " %s:%d", p->key, v);
                break;
            }
            case PARAM_TYPE_UINT: {
                const int v = *(int *)(priv + p->offset);
                if (v != p->def_value.i64)
                    ngli_bstr_printf(b, " %s:%u", p->key, v);
                break;
            }
            case PARAM_TYPE_I64: {
                const int64_t v = *(int64_t *)(priv + p->offset);
                if (v != p->def_value.i64)
                    ngli_bstr_printf(b, " %s:%"PRId64, p->key, v);
                break;
            }
            case PARAM_TYPE_DBL: {
                const double v = *(double *)(priv + p->offset);
                if (v != p->def_value.dbl) {
                    ngli_bstr_printf(b, " %s:", p->key);
                    print_double(b, v);
                }
                break;
            }
            case PARAM_TYPE_RATIONAL: {
                const int *r = (int *)(priv + p->offset);
                if (memcmp(r, p->def_value.r, sizeof(p->def_value.r)))
                    ngli_bstr_printf(b, " %s:%d/%d", p->key, r[0], r[1]);
                break;
            }
            case PARAM_TYPE_STR: {
                const char *s = *(char **)(priv + p->offset);
                if (!s || (p->def_value.str && !strcmp(s, p->def_value.str)))
                    break;
                if (!strcmp(p->key, "label") &&
                    ngli_is_default_label(node->class->name, s))
                    break;
                ngli_bstr_printf(b, " %s:", p->key);
                for (int i = 0; s[i]; i++)
                    if (s[i] >= '!' && s[i] <= '~' && s[i] != '%')
                        ngli_bstr_printf(b, "%c", s[i]);
                    else
                        ngli_bstr_printf(b, "%%%02x", s[i] & 0xff);
                break;
            }
            case PARAM_TYPE_DATA: {
                const uint8_t *data = *(uint8_t **)(priv + p->offset);
                const int size = *(int *)(priv + p->offset + sizeof(uint8_t *));
                if (!data || !size)
                    break;
                ngli_bstr_printf(b, " %s:%d,", p->key, size);
                for (int i = 0; i < size; i++) {
                    ngli_bstr_printf(b, "%02x", data[i]);
                }
                break;
            }
            case PARAM_TYPE_IVEC2:
            case PARAM_TYPE_IVEC3:
            case PARAM_TYPE_IVEC4: {
                const int *iv = (const int *)(priv + p->offset);
                const int n = p->type - PARAM_TYPE_IVEC2 + 2;
                if (memcmp(iv, p->def_value.ivec, n * sizeof(*iv))) {
                    ngli_bstr_printf(b, " %s:", p->key);
                    print_ints(b, n, iv);
                }
                break;
            }
            case PARAM_TYPE_UIVEC2:
            case PARAM_TYPE_UIVEC3:
            case PARAM_TYPE_UIVEC4: {
                const unsigned *uv = (const unsigned *)(priv + p->offset);
                const int n = p->type - PARAM_TYPE_UIVEC2 + 2;
                if (memcmp(uv, p->def_value.uvec, n * sizeof(*uv))) {
                    ngli_bstr_printf(b, " %s:", p->key);
                    print_unsigneds(b, n, uv);
                }
                break;
            }
            case PARAM_TYPE_VEC2:
            case PARAM_TYPE_VEC3:
            case PARAM_TYPE_VEC4: {
                const float *v = (float *)(priv + p->offset);
                const int n = p->type - PARAM_TYPE_VEC2 + 2;
                if (memcmp(v, p->def_value.vec, n * sizeof(*v))) {
                    ngli_bstr_printf(b, " %s:", p->key);
                    print_floats(b, n, v);
                }
                break;
            }
            case PARAM_TYPE_MAT4: {
                const float *m = (float *)(priv + p->offset);
                if (memcmp(m, p->def_value.mat, 16 * sizeof(*m))) {
                    ngli_bstr_printf(b, " %s:", p->key);
                    print_floats(b, 16, m);
                }
                break;
            }
            case PARAM_TYPE_NODE: {
                const struct ngl_node *node = *(struct ngl_node **)(priv + p->offset);
                if (!node)
                    break;
                const int node_id = get_rel_node_id(nlist, node);
                ngli_bstr_printf(b, " %s:%x", p->key, node_id);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **nodes = *(struct ngl_node ***)(priv + p->offset);
                const int nb_nodes = *(int *)(priv + p->offset + sizeof(struct ngl_node **));
                if (!nb_nodes)
                    break;
                ngli_bstr_printf(b, " %s:", p->key);
                for (int i = 0; i < nb_nodes; i++) {
                    const int node_id = get_rel_node_id(nlist, nodes[i]);
                    ngli_bstr_printf(b, "%s%x", i ? "," : "", node_id);
                }
                break;
            }
            case PARAM_TYPE_DBLLIST: {
                uint8_t *elems_p = priv + p->offset;
                uint8_t *nb_elems_p = priv + p->offset + sizeof(double *);
                const double *elems = *(double **)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                if (!nb_elems)
                    break;
                ngli_bstr_printf(b, " %s:", p->key);
                print_doubles(b, nb_elems, elems);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(priv + p->offset);
                const int nb_nodes = hmap ? ngli_hmap_count(hmap) : 0;
                if (!nb_nodes)
                    break;
                ngli_bstr_printf(b, " %s:", p->key);

                struct darray items_array;
                ngli_darray_init(&items_array, sizeof(struct item), 0);
                int ret = hmap_to_sorted_items(&items_array, hmap);
                if (ret < 0)
                    return ret;
                const struct item *items = ngli_darray_data(&items_array);
                for (int i = 0; i < ngli_darray_count(&items_array); i++) {
                    const struct item *item = &items[i];
                    const int node_id = get_rel_node_id(nlist, item->data);
                    ngli_bstr_printf(b, "%s%s=%x", i ? "," : "", item->key, node_id);
                }
                ngli_darray_reset(&items_array);
                break;
            }
            default:
                LOG(ERROR, "cannot serialize %s: unsupported parameter type", p->key);
                return NGL_ERROR_BUG;
        }
        p++;
    }
    return 0;
}

static int serialize(struct hmap *nlist,
                     struct bstr *b,
                     const struct ngl_node *node);

static int serialize_children(struct hmap *nlist,
                               struct bstr *b,
                               const struct ngl_node *node,
                               uint8_t *priv,
                               const struct node_param *p)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child) {
                    int ret = serialize(nlist, b, child);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                for (int i = 0; i < nb_children; i++) {
                    int ret = serialize(nlist, b, children[i]);
                    if (ret < 0)
                        return ret;
                }
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(priv + p->offset);
                if (!hmap)
                    break;

                struct darray items_array;
                int ret = hmap_to_sorted_items(&items_array, hmap);
                if (ret < 0)
                    return ret;
                const struct item *items = ngli_darray_data(&items_array);
                for (int i = 0; i < ngli_darray_count(&items_array); i++) {
                    const struct item *item = &items[i];
                    int ret = serialize(nlist, b, item->data);
                    if (ret < 0) {
                        ngli_darray_reset(&items_array);
                        return ret;
                    }
                }
                ngli_darray_reset(&items_array);
                break;
            }
        }
        p++;
    }
    return 0;
}

static int serialize(struct hmap *nlist,
                     struct bstr *b,
                     const struct ngl_node *node)
{
    if (get_node_id(nlist, node) >= 0)
        return 0;

    int ret;

    if ((ret = serialize_children(nlist, b, node, (uint8_t *)node, ngli_base_node_params)) < 0 ||
        (ret = serialize_children(nlist, b, node, node->priv_data, node->class->params)) < 0)
        return ret;

    const uint32_t tag = node->class->id;
    ngli_bstr_printf(b, "%c%c%c%c",
                    tag >> 24 & 0xff,
                    tag >> 16 & 0xff,
                    tag >>  8 & 0xff,
                    tag       & 0xff);
    if ((ret = serialize_options(nlist, b, node, node->priv_data, node->class->params)) < 0 ||
        (ret = serialize_options(nlist, b, node, (uint8_t *)node, ngli_base_node_params)) < 0)
        return ret;

    ngli_bstr_print(b, "\n");

    return register_node(nlist, node);
}

char *ngl_node_serialize(const struct ngl_node *node)
{
    char *s = NULL;
    struct hmap *nlist = ngli_hmap_create();
    struct bstr *b = ngli_bstr_create();
    if (!nlist || !b)
        goto end;

    ngli_hmap_set_free(nlist, free_func, NULL);
    ngli_bstr_printf(b, "# Node.GL v%d.%d.%d\n",
                    NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    if (serialize(nlist, b, node) < 0)
        goto end;
    s = ngli_bstr_strdup(b);

end:
    ngli_hmap_freep(&nlist);
    ngli_bstr_freep(&b);
    return s;
}
