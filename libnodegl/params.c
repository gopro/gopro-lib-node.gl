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

#include <string.h>
#include <inttypes.h>

#include "bstr.h"
#include "log.h"
#include "hmap.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "params.h"
#include "utils.h"

/* We depend on the monotically incrementing by 1 property of these fields */
NGLI_STATIC_ASSERT(param_vec,   PARAM_TYPE_VEC4   - PARAM_TYPE_VEC2 == 2);
NGLI_STATIC_ASSERT(param_ivec,  PARAM_TYPE_IVEC4  - PARAM_TYPE_INT  == 3);
NGLI_STATIC_ASSERT(param_uivec, PARAM_TYPE_UIVEC4 - PARAM_TYPE_UINT == 3);

const struct param_specs ngli_params_specs[] = {
    [PARAM_TYPE_INT] = {
        .name = "int",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Integer"),
    },
    [PARAM_TYPE_IVEC2] = {
        .name = "ivec2",
        .size = sizeof(int[2]),
        .desc = NGLI_DOCSTRING("2 integers"),
    },
    [PARAM_TYPE_IVEC3] = {
        .name = "ivec3",
        .size = sizeof(int[3]),
        .desc = NGLI_DOCSTRING("3 integers"),
    },
    [PARAM_TYPE_IVEC4] = {
        .name = "ivec4",
        .size = sizeof(int[4]),
        .desc = NGLI_DOCSTRING("4 integers"),
    },
    [PARAM_TYPE_UINT] = {
        .name = "uint",
        .size = sizeof(unsigned),
        .desc = NGLI_DOCSTRING("Unsigned integer"),
    },
    [PARAM_TYPE_UIVEC2] = {
        .name = "uivec2",
        .size = sizeof(unsigned[2]),
        .desc = NGLI_DOCSTRING("2 unsigned integers"),
    },
    [PARAM_TYPE_UIVEC3] = {
        .name = "uivec3",
        .size = sizeof(unsigned[3]),
        .desc = NGLI_DOCSTRING("3 unsigned integers"),
    },
    [PARAM_TYPE_UIVEC4] = {
        .name = "uivec4",
        .size = sizeof(unsigned[4]),
        .desc = NGLI_DOCSTRING("4 unsigned integers"),
    },
    [PARAM_TYPE_BOOL] = {
        .name = "bool",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Boolean (map to `int` in C)"),
    },
    [PARAM_TYPE_I64] = {
        .name = "i64",
        .size = sizeof(int64_t),
        .desc = NGLI_DOCSTRING("64-bit integer"),
    },
    [PARAM_TYPE_DBL] = {
        .name = "double",
        .size = sizeof(double),
        .desc = NGLI_DOCSTRING("Double-precision float"),
    },
    [PARAM_TYPE_STR] = {
        .name = "string",
        .size = sizeof(char *),
        .desc = NGLI_DOCSTRING("String"),
    },
    [PARAM_TYPE_DATA] = {
        .name = "data",
        .size = sizeof(void *) + sizeof(int),
        .desc = NGLI_DOCSTRING("Agnostic data buffer"),
    },
    [PARAM_TYPE_VEC2] = {
        .name = "vec2",
        .size = sizeof(float[2]),
        .desc = NGLI_DOCSTRING("2 single-precision floats"),
    },
    [PARAM_TYPE_VEC3] = {
        .name = "vec3",
        .size = sizeof(float[3]),
        .desc = NGLI_DOCSTRING("3 single-precision floats"),
    },
    [PARAM_TYPE_VEC4] = {
        .name = "vec4",
        .size = sizeof(float[4]),
        .desc = NGLI_DOCSTRING("4 single-precision floats"),
    },
    [PARAM_TYPE_MAT4] = {
        .name = "mat4",
        .size = sizeof(float[4*4]),
        .desc = NGLI_DOCSTRING("4x4 single-precision floats"),
    },
    [PARAM_TYPE_NODE] = {
        .name = "Node",
        .size = sizeof(struct ngl_node *),
        .desc = NGLI_DOCSTRING("node.gl Node"),
    },
    [PARAM_TYPE_NODELIST] = {
        .name = "NodeList",
        .size = sizeof(struct ngl_node **) + sizeof(int),
        .desc = NGLI_DOCSTRING("List of node.gl Node"),
    },
    [PARAM_TYPE_DBLLIST] = {
        .name = "doubleList",
        .size = sizeof(double *) + sizeof(int),
        .desc = NGLI_DOCSTRING("List of double-precision floats"),
    },
    [PARAM_TYPE_NODEDICT] = {
        .name = "NodeDict",
        .size = sizeof(struct hmap *),
        .desc = NGLI_DOCSTRING("Dictionary mapping arbitrary string identifiers to node.gl Nodes"),
    },
    [PARAM_TYPE_SELECT] = {
        .name = "select",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Selection of one constant (expressed as a string)"),
    },
    [PARAM_TYPE_FLAGS] = {
        .name = "flags",
        .size = sizeof(int),
        .desc = NGLI_DOCSTRING("Combination of constants (expressed as strings), using `+` as separator. Can be empty for none."),
    },
    [PARAM_TYPE_RATIONAL] = {
        .name = "rational",
        .size = sizeof(int[2]),
        .desc = NGLI_DOCSTRING("Rational number (expressed as 2 integers, respectively as numerator and denominator)"),
    },
};

const struct node_param *ngli_params_find(const struct node_param *params, const char *key)
{
    if (!params)
        return NULL;
    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];
        if (!strcmp(par->key, key))
            return par;
    }
    return NULL;
}

int ngli_params_get_select_val(const struct param_const *consts, const char *s, int *dst)
{
    for (int i = 0; consts[i].key; i++) {
        if (!strcmp(consts[i].key, s)) {
            *dst = consts[i].value;
            return 0;
        }
    }
    return NGL_ERROR_INVALID_ARG;
}

const char *ngli_params_get_select_str(const struct param_const *consts, int val)
{
    for (int i = 0; consts[i].key; i++)
        if (consts[i].value == val)
            return consts[i].key;
    return NULL;
}

/* The first separator (`+`) is used as default separator because `|` can not
 * be used due to markdown table formatting, and ` ` will cause parsing issue
 * in the serialization.
 */
#define FLAGS_SEP "+| "

int ngli_params_get_flags_val(const struct param_const *consts, const char *s, int *dst)
{
    *dst = 0;
    if (!strcmp(s, "0"))
        return 0;
    if (!strcmp(s, "unset")) {
        *dst = -1;
        return 0;
    }
    while (*s) {
        int i;
        const size_t len = strcspn(s, FLAGS_SEP);
        for (i = 0; consts[i].key; i++) {
            if (!strncmp(consts[i].key, s, len)) {
                /* -1 is a reserved value and means that the mask is unset */
                ngli_assert(consts[i].value != -1);
                *dst |= consts[i].value;
                break;
            }
        }
        if (!consts[i].key) {
            LOG(ERROR, "unrecognized \"%.*s\" flag", (int)len, s);
            return NGL_ERROR_INVALID_ARG;
        }
        s += len;
        while (*s && strchr(FLAGS_SEP, *s))
            s++;
    }
    return 0;
}

char *ngli_params_get_flags_str(const struct param_const *consts, int val)
{
    if (!val)
        return ngli_strdup("0");

    if (val == -1)
        return ngli_strdup("unset");

    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    for (int i = 0; consts[i].key; i++)
        if (val & consts[i].value)
            ngli_bstr_printf(b, "%.1s%s", *ngli_bstr_strptr(b) ? FLAGS_SEP : "", consts[i].key);

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return ret;
}

void ngli_params_bstr_print_val(struct bstr *b, uint8_t *base_ptr, const struct node_param *par)
{
    const uint8_t *srcp = base_ptr + par->offset;

    switch (par->type) {
        case PARAM_TYPE_SELECT: {
            const int v = *(const int *)srcp;
            const char *s = ngli_params_get_select_str(par->choices->consts, v);
            ngli_assert(s);
            ngli_bstr_print(b, s);
            break;
        }
        case PARAM_TYPE_FLAGS: {
            const int v = *(const int *)srcp;
            char *s = ngli_params_get_flags_str(par->choices->consts, v);
            if (!s)
                break;
            ngli_assert(*s);
            ngli_bstr_print(b, s);
            ngli_free(s);
            break;
        }
        case PARAM_TYPE_BOOL: {
            const int v = *(const int *)srcp;
            if (v == -1)
                ngli_bstr_print(b, "unset");
            else
                ngli_bstr_printf(b, "%d", v);
            break;
        }
        case PARAM_TYPE_DBL:    ngli_bstr_printf(b, "%g",            *(const double *)srcp);                 break;
        case PARAM_TYPE_INT:    ngli_bstr_printf(b, "%d",            *(const int *)srcp);                    break;
        case PARAM_TYPE_UINT:   ngli_bstr_printf(b, "%u",            *(const unsigned *)srcp);               break;
        case PARAM_TYPE_I64:    ngli_bstr_printf(b, "%" PRId64,      *(const int64_t *)srcp);                break;
        case PARAM_TYPE_IVEC2:  ngli_bstr_printf(b, "(%d,%d)",       NGLI_ARG_VEC2((const int *)srcp));      break;
        case PARAM_TYPE_IVEC3:  ngli_bstr_printf(b, "(%d,%d,%d)",    NGLI_ARG_VEC3((const int *)srcp));      break;
        case PARAM_TYPE_IVEC4:  ngli_bstr_printf(b, "(%d,%d,%d,%d)", NGLI_ARG_VEC4((const int *)srcp));      break;
        case PARAM_TYPE_UIVEC2: ngli_bstr_printf(b, "(%u,%u)",       NGLI_ARG_VEC2((const unsigned *)srcp)); break;
        case PARAM_TYPE_UIVEC3: ngli_bstr_printf(b, "(%u,%u,%u)",    NGLI_ARG_VEC3((const unsigned *)srcp)); break;
        case PARAM_TYPE_UIVEC4: ngli_bstr_printf(b, "(%u,%u,%u,%u)", NGLI_ARG_VEC4((const unsigned *)srcp)); break;
        case PARAM_TYPE_VEC2:   ngli_bstr_printf(b, "(%g,%g)",       NGLI_ARG_VEC2((const float *)srcp));    break;
        case PARAM_TYPE_VEC3:   ngli_bstr_printf(b, "(%g,%g,%g)",    NGLI_ARG_VEC3((const float *)srcp));    break;
        case PARAM_TYPE_VEC4:   ngli_bstr_printf(b, "(%g,%g,%g,%g)", NGLI_ARG_VEC4((const float *)srcp));    break;
        case PARAM_TYPE_MAT4:
            ngli_bstr_printf(b, "(%g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g)", NGLI_ARG_MAT4((const float *)srcp));
            break;
        case PARAM_TYPE_STR: {
            const char *s = *(const char **)srcp;
            if (!s)
                ngli_bstr_print(b, "\"\"");
            else if (strchr(s, '\n')) // print a checksum when the string is multiline (typically, shaders)
                ngli_bstr_printf(b, "%08X <i>(CRC32)</i>", ngli_crc32(s));
            else if (strchr(s, '/')) // assume a file and display only basename
                ngli_bstr_printf(b, "\"%s\"", strrchr(s, '/') + 1);
            else
                ngli_bstr_printf(b, "\"%s\"", s);
            break;
        }
        case PARAM_TYPE_DBLLIST: {
            const uint8_t *nb_elems_p = srcp + sizeof(double *);
            const double *elems = *(const double **)srcp;
            const int nb_elems = *(const int *)nb_elems_p;
            for (int i = 0; i < nb_elems; i++)
                ngli_bstr_printf(b, "%s%g", i ? "," : "", elems[i]);
            break;
        }
        case PARAM_TYPE_RATIONAL:
            ngli_bstr_printf(b, "%d/%d", NGLI_ARG_VEC2((const int *)srcp));
            break;
    }
}

static int allowed_node(const struct ngl_node *node, const int *allowed_ids)
{
    if (!allowed_ids)
        return 1;
    const int id = node->class->id;
    for (int i = 0; allowed_ids[i] != -1; i++)
        if (id == allowed_ids[i])
            return 1;
    return 0;
}

static void node_hmap_free(void *user_arg, void *data)
{
    struct ngl_node *node = data;
    ngl_node_unrefp(&node);
}

int ngli_params_set(uint8_t *base_ptr, const struct node_param *par, va_list *ap)
{
    uint8_t *dstp = base_ptr + par->offset;

    switch (par->type) {
        case PARAM_TYPE_SELECT: {
            int v;
            const char *s = va_arg(*ap, const char *);
            int ret = ngli_params_get_select_val(par->choices->consts, s, &v);
            if (ret < 0) {
                LOG(ERROR, "unrecognized constant \"%s\" for option %s", s, par->key);
                return ret;
            }
            LOG(VERBOSE, "set %s to %s (%d)", par->key, s, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_FLAGS: {
            int v;
            const char *s = va_arg(*ap, const char *);
            int ret = ngli_params_get_flags_val(par->choices->consts, s, &v);
            if (ret < 0) {
                LOG(ERROR, "unrecognized flags \"%s\" for option %s", s, par->key);
                return ret;
            }
            LOG(VERBOSE, "set %s to %s (%d)", par->key, s, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_BOOL:
        case PARAM_TYPE_INT: {
            int v = va_arg(*ap, int);
            if (par->type == PARAM_TYPE_BOOL && v != -1)
                v = !!v;
            LOG(VERBOSE, "set %s to %d", par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_UINT: {
            unsigned v = va_arg(*ap, unsigned);
            LOG(VERBOSE, "set %s to %u", par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_I64: {
            int64_t v = va_arg(*ap, int64_t);
            LOG(VERBOSE, "set %s to %"PRId64, par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_DBL: {
            double v = va_arg(*ap, double);
            LOG(VERBOSE, "set %s to %g", par->key, v);
            memcpy(dstp, &v, sizeof(v));
            break;
        }
        case PARAM_TYPE_STR: {
            char *s = NULL;
            const char *arg_str = va_arg(*ap, const char *);
            if (!arg_str)
                arg_str = par->def_value.str;
            if (arg_str) {
                s = ngli_strdup(arg_str);
                if (!s)
                    return NGL_ERROR_MEMORY;
                LOG(VERBOSE, "set %s to \"%s\"", par->key, s);
            } else {
                LOG(VERBOSE, "set %s to NULL", par->key);
            }
            ngli_free(*(char **)dstp);
            memcpy(dstp, &s, sizeof(s));
            break;
        }
        case PARAM_TYPE_DATA: {
            int size = va_arg(*ap, int);
            void *data = va_arg(*ap, void *);
            LOG(VERBOSE, "set %s to %p (of size %d)", par->key, data, size);
            uint8_t **dst = (uint8_t **)dstp;

            ngli_freep(dst);
            if (data && size) {
                *dst = ngli_malloc(size);
                if (!*dst)
                    return NGL_ERROR_MEMORY;
                memcpy(*dst, data, size);
            } else {
                size = 0;
            }
            memcpy(dstp + sizeof(void *), &size, sizeof(size));
            break;
        }
        case PARAM_TYPE_IVEC2: {
            const int *iv = va_arg(*ap, const int *);
            LOG(VERBOSE, "set %s to (%d,%d)", par->key, NGLI_ARG_VEC2(iv));
            memcpy(dstp, iv, 2 * sizeof(*iv));
            break;
        }
        case PARAM_TYPE_IVEC3: {
            const int *iv = va_arg(*ap, const int *);
            LOG(VERBOSE, "set %s to (%d,%d,%d)", par->key, NGLI_ARG_VEC3(iv));
            memcpy(dstp, iv, 3 * sizeof(*iv));
            break;
        }
        case PARAM_TYPE_IVEC4: {
            const int *iv = va_arg(*ap, const int *);
            LOG(VERBOSE, "set %s to (%d,%d,%d,%d)", par->key, NGLI_ARG_VEC4(iv));
            memcpy(dstp, iv, 4 * sizeof(*iv));
            break;
        }
        case PARAM_TYPE_UIVEC2: {
            const unsigned *uv = va_arg(*ap, const unsigned *);
            LOG(VERBOSE, "set %s to (%u,%u)", par->key, NGLI_ARG_VEC2(uv));
            memcpy(dstp, uv, 2 * sizeof(*uv));
            break;
        }
        case PARAM_TYPE_UIVEC3: {
            const unsigned *uv = va_arg(*ap, const unsigned *);
            LOG(VERBOSE, "set %s to (%u,%u,%u)", par->key, NGLI_ARG_VEC3(uv));
            memcpy(dstp, uv, 3 * sizeof(*uv));
            break;
        }
        case PARAM_TYPE_UIVEC4: {
            const unsigned *uv = va_arg(*ap, const unsigned *);
            LOG(VERBOSE, "set %s to (%u,%u,%u,%u)", par->key, NGLI_ARG_VEC4(uv));
            memcpy(dstp, uv, 4 * sizeof(*uv));
            break;
        }
        case PARAM_TYPE_VEC2: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE, "set %s to (%g,%g)", par->key, NGLI_ARG_VEC2(v));
            memcpy(dstp, v, 2 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_VEC3: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE, "set %s to (%g,%g,%g)", par->key, NGLI_ARG_VEC3(v));
            memcpy(dstp, v, 3 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_VEC4: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE, "set %s to (%g,%g,%g,%g)", par->key, NGLI_ARG_VEC4(v));
            memcpy(dstp, v, 4 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_MAT4: {
            const float *v = va_arg(*ap, const float *);
            LOG(VERBOSE,
                "set %s to (%g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g %g,%g,%g,%g)",
                par->key, NGLI_ARG_MAT4(v));
            memcpy(dstp, v, 16 * sizeof(*v));
            break;
        }
        case PARAM_TYPE_NODE: {
            struct ngl_node *node = va_arg(*ap, struct ngl_node *);
            if (!allowed_node(node, par->node_types)) {
                LOG(ERROR, "%s (%s) is not an allowed type for %s",
                    node->label, node->class->name, par->key);
                return NGL_ERROR_INVALID_ARG;
            }
            ngl_node_unrefp((struct ngl_node **)dstp);
            ngl_node_ref(node);
            LOG(VERBOSE, "set %s to %s", par->key, node->label);
            memcpy(dstp, &node, sizeof(node));
            break;
        }
        case PARAM_TYPE_NODEDICT: {
            int ret;
            const char *name = va_arg(*ap, const char *);
            struct ngl_node *node = va_arg(*ap, struct ngl_node *);
            if (node && !allowed_node(node, par->node_types)) {
                LOG(ERROR, "%s (%s) is not an allowed type for %s",
                    node->label, node->class->name, par->key);
                return NGL_ERROR_INVALID_ARG;
            }
            LOG(VERBOSE, "set %s to (%s,%p)", par->key, name, node);
            struct hmap **hmapp = (struct hmap **)dstp;
            if (!*hmapp) {
                *hmapp = ngli_hmap_create();
                if (!*hmapp)
                    return NGL_ERROR_MEMORY;
                ngli_hmap_set_free(*hmapp, node_hmap_free, NULL);
            }

            ret = ngli_hmap_set(*hmapp, name, node);
            if (ret < 0)
                return ret;
            if (node)
                ngl_node_ref(node);
            break;
        }
        case PARAM_TYPE_RATIONAL: {
            const int num = va_arg(*ap, int);
            const int den = va_arg(*ap, int);
            LOG(VERBOSE, "set %s to %d/%d", par->key, num, den);
            memcpy(dstp, &num, sizeof(num));
            memcpy(dstp + sizeof(num), &den, sizeof(den));
            break;
        }
    }
    return 0;
}

int ngli_params_vset(uint8_t *base_ptr, const struct node_param *par, ...)
{
    va_list ap;
    va_start(ap, par);
    int ret = ngli_params_set(base_ptr, par, &ap);
    va_end(ap);
    return ret;
}

int ngli_params_set_defaults(uint8_t *base_ptr, const struct node_param *params)
{
    int last_offset = 0;

    if (!params)
        return 0;

    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        // The offset must be monotically incrementing to make the reset of the
        // non-params much simpler in the node uninit.
        if (par->offset < last_offset) {
            LOG(ERROR, "offset inconsistency detected around %s", par->key);
            ngli_assert(0);
        }
        last_offset = par->offset;

        int ret = 0;
        // TODO: reindent
            switch (par->type) {
                case PARAM_TYPE_SELECT: {
                    const int v = (int)par->def_value.i64;
                    const char *s = ngli_params_get_select_str(par->choices->consts, v);
                    ngli_assert(s);
                    ret = ngli_params_vset(base_ptr, par, s);
                    break;
                }
                case PARAM_TYPE_FLAGS: {
                    const int v = (int)par->def_value.i64;
                    char *s = ngli_params_get_flags_str(par->choices->consts, v);
                    if (!s)
                        return NGL_ERROR_INVALID_ARG;
                    ngli_assert(*s);
                    ret = ngli_params_vset(base_ptr, par, s);
                    ngli_free(s);
                    break;
                }
                case PARAM_TYPE_BOOL:
                case PARAM_TYPE_INT:
                case PARAM_TYPE_UINT:
                case PARAM_TYPE_I64:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.i64);
                    break;
                case PARAM_TYPE_DBL:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.dbl);
                    break;
                case PARAM_TYPE_STR:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.str);
                    break;
                case PARAM_TYPE_IVEC2:
                case PARAM_TYPE_IVEC3:
                case PARAM_TYPE_IVEC4:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.ivec);
                    break;
                case PARAM_TYPE_UIVEC2:
                case PARAM_TYPE_UIVEC3:
                case PARAM_TYPE_UIVEC4:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.uvec);
                    break;
                case PARAM_TYPE_VEC2:
                case PARAM_TYPE_VEC3:
                case PARAM_TYPE_VEC4:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.vec);
                    break;
                case PARAM_TYPE_MAT4:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.mat);
                    break;
                case PARAM_TYPE_DATA:
                    ret = ngli_params_vset(base_ptr, par, 0, par->def_value.p);
                    break;
                case PARAM_TYPE_RATIONAL:
                    ret = ngli_params_vset(base_ptr, par, par->def_value.r[0], par->def_value.r[1]);
                    break;
            }
        if (ret < 0)
            return ret;
    }
    return 0;
}

int ngli_params_add(uint8_t *base_ptr, const struct node_param *par,
                    int nb_elems, void *elems)
{
    LOG(VERBOSE, "add %d elems to %s", nb_elems, par->key);
    switch (par->type) {
        case PARAM_TYPE_NODELIST: {
            uint8_t *cur_elems_p = base_ptr + par->offset;
            uint8_t *nb_cur_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
            struct ngl_node **cur_elems = *(struct ngl_node ***)cur_elems_p;
            const int nb_cur_elems = *(int *)nb_cur_elems_p;
            const int nb_new_elems = nb_cur_elems + nb_elems;
            struct ngl_node **new_elems = ngli_realloc(cur_elems, nb_new_elems * sizeof(*new_elems));
            struct ngl_node **new_elems_addp = new_elems + nb_cur_elems;
            struct ngl_node **add_elems = elems;

            if (!new_elems)
                return NGL_ERROR_MEMORY;

            *(struct ngl_node ***)cur_elems_p = new_elems;
            for (int i = 0; i < nb_elems; i++) {
                const struct ngl_node *e = add_elems[i];
                if (!allowed_node(e, par->node_types)) {
                    LOG(ERROR, "%s (%s) is not an allowed type for %s list",
                        e->label, e->class->name, par->key);
                    return NGL_ERROR_INVALID_ARG;
                }
            }
            for (int i = 0; i < nb_elems; i++) {
                struct ngl_node *e = add_elems[i];
                new_elems_addp[i] = ngl_node_ref(e);
            }
            *(int *)nb_cur_elems_p = nb_new_elems;
            break;
        }
        case PARAM_TYPE_DBLLIST: {
            uint8_t *cur_elems_p = base_ptr + par->offset;
            uint8_t *nb_cur_elems_p = base_ptr + par->offset + sizeof(double *);
            double *cur_elems = *(double **)cur_elems_p;
            const int nb_cur_elems = *(int *)nb_cur_elems_p;
            const int nb_new_elems = nb_cur_elems + nb_elems;
            double *new_elems = ngli_realloc(cur_elems, nb_new_elems * sizeof(*new_elems));
            double *new_elems_addp = new_elems + nb_cur_elems;
            double *add_elems = elems;

            if (!new_elems)
                return NGL_ERROR_MEMORY;
            for (int i = 0; i < nb_elems; i++)
                new_elems_addp[i] = add_elems[i];
            *(double **)cur_elems_p = new_elems;
            *(int *)nb_cur_elems_p = nb_new_elems;
            break;
        }
        default:
            LOG(ERROR, "parameter %s is not a list", par->key);
            return NGL_ERROR_INVALID_USAGE;
    }
    return 0;
}

void ngli_params_free(uint8_t *base_ptr, const struct node_param *params)
{
    if (!params)
        return;

    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];
        uint8_t *parp = base_ptr + par->offset;

        switch (par->type) {
            case PARAM_TYPE_STR: {
                char *s = *(char **)parp;
                ngli_free(s);
                break;
            }
            case PARAM_TYPE_DATA: {
                uint8_t *data = *(uint8_t **)parp;
                ngli_free(data);
                break;
            }
            case PARAM_TYPE_NODE: {
                uint8_t *node_p = base_ptr + par->offset;
                struct ngl_node *node = *(struct ngl_node **)node_p;
                ngl_node_unrefp(&node);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                int j;
                uint8_t *elems_p = base_ptr + par->offset;
                uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
                struct ngl_node **elems = *(struct ngl_node ***)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                for (j = 0; j < nb_elems; j++)
                    ngl_node_unrefp(&elems[j]);
                ngli_free(elems);
                break;
            }
            case PARAM_TYPE_DBLLIST: {
                uint8_t *elems_p = base_ptr + par->offset;
                double *elems = *(double **)elems_p;
                ngli_free(elems);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap **hmapp = (struct hmap **)(base_ptr + par->offset);
                ngli_hmap_freep(hmapp);
                break;
            }
        }
    }
}
