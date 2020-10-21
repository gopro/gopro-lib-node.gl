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

#include "hmap.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "nodes_register.h"
#include "utils.h"

#define CLASS_LIST(type_name, class) extern const struct node_class class;
NODE_MAP_TYPE2CLASS(CLASS_LIST)

extern const struct node_param ngli_base_node_params[];
extern const struct param_specs ngli_params_specs[];

#define TYPE2CLASS(type_name, class)            \
    case type_name: {                           \
        extern const struct node_class class;   \
        return &class;                          \
    }                                           \

static const struct node_class *get_node_class(int type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(TYPE2CLASS)
    }
    return NULL;
}

#define LOWER(c) (((c) >= 'A' && (c) <= 'Z') ? (c) ^ 0x20 : (c))

static void print_node_type(struct bstr *b, const struct node_class *class)
{
    const char *class_ref = class->params_id ? class->params_id : class->name;

    ngli_bstr_printf(b, "[%s](#", class->name);
    for (int i = 0; class_ref[i]; i++)
        ngli_bstr_printf(b, "%c", LOWER(class_ref[i]));
    ngli_bstr_print(b, ")");
}

static char *get_type_str(const struct node_param *p)
{
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    if (p->choices) {
        ngli_bstr_printf(b, "[`%s`](#%s-choices)", p->choices->name, p->choices->name);
    } else {
        ngli_bstr_printf(b, "[`%s`](#parameter-types)", ngli_params_specs[p->type].name);
    }
    if (p->node_types) {
        ngli_bstr_print(b, " (");
        for (int i = 0; p->node_types[i] != -1; i++) {
            if (i)
                ngli_bstr_print(b, ", ");
            print_node_type(b, get_node_class(p->node_types[i]));
        }
        ngli_bstr_print(b, ")");
    }

    char *type = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return type;
}

static char *get_default_str(const struct node_param *p)
{
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    switch (p->type) {
        case PARAM_TYPE_SELECT: {
            const int v = (int)p->def_value.i64;
            const char *s = ngli_params_get_select_str(p->choices->consts, v);
            ngli_assert(s);
            ngli_bstr_printf(b, "`%s`", s);
            break;
        }
        case PARAM_TYPE_FLAGS: {
            const int v = (int)p->def_value.i64;
            char *s = ngli_params_get_flags_str(p->choices->consts, v);
            if (!s)
                return NULL;
            ngli_assert(*s);
            ngli_bstr_printf(b, "`%s`", s);
            ngli_free(s);
            break;
        }
        case PARAM_TYPE_DBL:
            ngli_bstr_printf(b, "`%g`", p->def_value.dbl);
            break;
        case PARAM_TYPE_BOOL:
            if (p->def_value.i64 < 0)
                ngli_bstr_print(b, "`unset`");
            else
                ngli_bstr_printf(b, "`%d`", (int)p->def_value.i64);
            break;
        case PARAM_TYPE_INT:
            ngli_bstr_printf(b, "`%d`", (int)p->def_value.i64);
            break;
        case PARAM_TYPE_UINT:
            ngli_bstr_printf(b, "`%u`", (unsigned)p->def_value.i64);
            break;
        case PARAM_TYPE_I64:
            ngli_bstr_printf(b, "`%" PRId64 "`", p->def_value.i64);
            break;
        case PARAM_TYPE_IVEC2:  ngli_bstr_printf(b, "(`%d`,`%d`)",           NGLI_ARG_VEC2(p->def_value.ivec)); break;
        case PARAM_TYPE_IVEC3:  ngli_bstr_printf(b, "(`%d`,`%d`,`%d`)",      NGLI_ARG_VEC3(p->def_value.ivec)); break;
        case PARAM_TYPE_IVEC4:  ngli_bstr_printf(b, "(`%d`,`%d`,`%d`,`%d`)", NGLI_ARG_VEC4(p->def_value.ivec)); break;
        case PARAM_TYPE_UIVEC2: ngli_bstr_printf(b, "(`%u`,`%u`)",           NGLI_ARG_VEC2(p->def_value.uvec)); break;
        case PARAM_TYPE_UIVEC3: ngli_bstr_printf(b, "(`%u`,`%u`,`%u`)",      NGLI_ARG_VEC3(p->def_value.uvec)); break;
        case PARAM_TYPE_UIVEC4: ngli_bstr_printf(b, "(`%u`,`%u`,`%u`,`%u`)", NGLI_ARG_VEC4(p->def_value.uvec)); break;
        case PARAM_TYPE_VEC2:   ngli_bstr_printf(b, "(`%g`,`%g`)",           NGLI_ARG_VEC2(p->def_value.vec));  break;
        case PARAM_TYPE_VEC3:   ngli_bstr_printf(b, "(`%g`,`%g`,`%g`)",      NGLI_ARG_VEC3(p->def_value.vec));  break;
        case PARAM_TYPE_VEC4:   ngli_bstr_printf(b, "(`%g`,`%g`,`%g`,`%g`)", NGLI_ARG_VEC4(p->def_value.vec));  break;
    }

    char *def = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return def;
}

static void print_node_params(const char *name, const struct node_param *p)
{
    printf("\n## %s\n\n", name);
    if (!p)
        return;
    printf("Parameter | Live-chg. | Type | Description | Default\n");
    printf("--------- | :-------: | ---- | ----------- | :-----:\n");
    while (p->key) {
        char *type = get_type_str(p);
        char *def = get_default_str(p);

        if (type && def) {
            ngli_assert(p->desc);
            printf("`%s` | %s | %s | %s | %s\n",
                   p->key,
                   (p->flags & PARAM_FLAG_ALLOW_LIVE_CHANGE) ? "✓" : "",
                   type, p->desc, def);
        }

        ngli_free(def);
        ngli_free(type);
        p++;
    }
    printf("\n\n");
}

static void print_choices(const struct param_choices *choices)
{
    printf("\n## %s choices\n\n", choices->name);
    const struct param_const *consts = choices->consts;
    printf("Constant | Description\n");
    printf("-------- | -----------\n");
    for (int i = 0; consts[i].key; i++) {
        const struct param_const *c = &consts[i];
        ngli_assert(c->desc);
        printf("`%s` | %s\n", c->key, c->desc);
    }
}

#define CLASS_COMMALIST(type_name, class) &class,

static void print_source(const char *cfile)
{
    ngli_assert(cfile);
    printf("**Source**: [%s](/libnodegl/%s)\n\n", cfile, cfile);
}

int main(void)
{
    printf("libnodegl\n");
    printf("=========\n");

    static const struct node_class *node_classes[] = {
        NODE_MAP_TYPE2CLASS(CLASS_COMMALIST)
    };

    struct hmap *params_map = ngli_hmap_create();
    if (!params_map)
        return -1;

    for (int i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        const struct node_param *p = &c->params[0];

        if (c->params_id) {
            void *mapped_param = ngli_hmap_get(params_map, c->params_id);
            char *pname = ngli_asprintf("%s*", c->params_id);
            if (!pname) {
                ngli_hmap_freep(&params_map);
                return -1;
            }
            if (mapped_param) {
                ngli_assert(mapped_param == p);
            } else {
                print_node_params(pname, p);
                ngli_hmap_set(params_map, c->params_id, (void *)p);
                print_source(c->file);
                printf("List of `%s*` nodes:\n\n", c->params_id);
            }
            printf("- `%s`\n", c->name);
            ngli_free(pname);
        } else {
            print_node_params(c->name, p);
            print_source(c->file);
        }
    }

    ngli_hmap_freep(&params_map);

    printf("Parameter types\n");
    printf("===============\n");
    printf("\n");
    printf("Type | Description\n");
    printf("---- | -----------\n");
    for (int i = 0; i < NB_PARAMS; i++) {
        const struct param_specs *ps = &ngli_params_specs[i];
        ngli_assert(ps->name && ps->desc);
        printf("`%s` | %s\n", ps->name, ps->desc);
    }
    printf("\n");

    printf("Constants for choices parameters\n");
    printf("================================\n");

    struct hmap *choices_map = ngli_hmap_create();
    if (!choices_map)
        return -1;

    for (int i = 0; i < NGLI_ARRAY_NB(node_classes); i++) {
        const struct node_class *c = node_classes[i];
        const struct node_param *p = c->params;

        if (p) {
            while (p->key) {
                if (p->choices) {
                    void *mapped_choices = ngli_hmap_get(choices_map, p->choices->name);
                    if (!mapped_choices) {
                        print_choices(p->choices);
                        ngli_hmap_set(choices_map, p->choices->name, (void *)p->choices);
                    }
                }
                p++;
            }
        }
    }

    ngli_hmap_freep(&choices_map);
    return 0;
}
