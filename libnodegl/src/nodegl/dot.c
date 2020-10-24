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

#include <stdint.h>
#include <string.h>

#include "bstr.h"
#include "hmap.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"

#define LB "<br align=\"left\"/>"
#define HSLFMT "\"0.%u 0.6 0.9\""
#define INACTIVE_COLOR "\"#333333\""

extern const struct node_param ngli_base_node_params[];

static int visited(struct hmap *ptr_set, const void *id)
{
    char key[32];
    int ret = snprintf(key, sizeof(key), "%p", id);
    if (ret < 0)
        return ret;
    if (ngli_hmap_get(ptr_set, key))
        return 1;
    return ngli_hmap_set(ptr_set, key, "");
}

static unsigned get_hue(const char *name)
{
    const uint32_t hash = ngli_crc32(name);
    const double hue = (double)hash / (double)0xffffffff;
    return (unsigned)(hue * 1000);
}

static int vec_is_set(uint8_t *base_ptr, const struct node_param *par)
{
    const int n = par->type - PARAM_TYPE_VEC2 + 2;
    const float *v = (const float *)(base_ptr + par->offset);
    return memcmp(v, par->def_value.vec, n * sizeof(*v));
}

static int mat_is_set(uint8_t *base_ptr, const struct node_param *par)
{
    const float *v = (const float *)(base_ptr + par->offset);
    return memcmp(v, par->def_value.mat, 16 * sizeof(*v));
}

static int should_print_par(uint8_t *priv, const struct node_param *par)
{
    switch (par->type) {
        case PARAM_TYPE_DBL: {
            const double v = *(double *)(priv + par->offset);
            return v != par->def_value.dbl;
        }
        case PARAM_TYPE_BOOL:
        case PARAM_TYPE_FLAGS:
        case PARAM_TYPE_SELECT:
        case PARAM_TYPE_INT: {
            const int v = *(int *)(priv + par->offset);
            return v != par->def_value.i64;
        }
        case PARAM_TYPE_I64: {
            const int64_t v = *(int64_t *)(priv + par->offset);
            return v != par->def_value.i64;
        }
        case PARAM_TYPE_RATIONAL: {
            const int *r = (int *)(priv + par->offset);
            return memcmp(r, par->def_value.r, sizeof(par->def_value.r));
        }
        case PARAM_TYPE_STR: {
            const char *s = *(const char **)(priv + par->offset);
            return s && (!par->def_value.str || strcmp(s, par->def_value.str));
        }
        case PARAM_TYPE_VEC2:
        case PARAM_TYPE_VEC3:
        case PARAM_TYPE_VEC4:
            return vec_is_set(priv, par);
        case PARAM_TYPE_MAT4:
            return mat_is_set(priv, par);
    }
    return 0;
}

static void print_custom_priv_options(struct bstr *b, const struct ngl_node *node)
{
    const struct node_param *par = node->class->params;
    uint8_t *priv = node->priv_data;

    if (!par)
        return;

    while (par->key) {
        if (should_print_par(priv, par)) {
            ngli_bstr_printf(b, "%s: ", par->key);
            ngli_params_bstr_print_val(b, priv, par);
            ngli_bstr_print(b, LB);
        }
        par++;
    }
}

static void print_decls(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *decls);

static void print_all_decls(struct bstr *b, const struct ngl_node *node, struct hmap *decls)
{
    if (visited(decls, node))
        return;

    ngli_bstr_printf(b, "    %s_%p[label=<<b>%s</b><br/>",
                    node->class->name, node, node->class->name);
    if (!ngli_is_default_label(node->class->name, node->label) && *node->label)
        ngli_bstr_printf(b, "<i>%s</i><br/>", node->label);
    print_custom_priv_options(b, node);
    if (!node->ctx || node->is_active)
        ngli_bstr_printf(b, ">,color="HSLFMT"]\n", get_hue(node->class->name));
    else
        ngli_bstr_print(b, ">,color="INACTIVE_COLOR"]\n");

    print_decls(b, node, ngli_base_node_params, (uint8_t *)node, decls);
    print_decls(b, node, node->class->params, node->priv_data, decls);
}

static void print_packed_decls(struct bstr *b, const char *label,
                               struct ngl_node **children, int nb_children,
                               int is_active)
{
    ngli_bstr_printf(b, "    %s_%p[label=<<b>%s</b> (x%d)", label, children, label, nb_children);
    for (int i = 0; i < nb_children; i++) {
        const struct ngl_node *node = children[i];
        char *info_str = node->class->info_str ? node->class->info_str(node) : NULL;
        ngli_bstr_printf(b, LB "- %s", info_str ? info_str : "?");
        ngli_free(info_str);
    }
    ngli_bstr_print(b, LB ">,shape=box,color=");
    if (is_active)
        ngli_bstr_printf(b, HSLFMT"]\n", get_hue(label));
    else
        ngli_bstr_print(b, INACTIVE_COLOR "]\n");
}

static void print_decls(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *decls)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child)
                    print_all_decls(b, child, decls);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                if (nb_children && (p->flags & PARAM_FLAG_DOT_DISPLAY_PACKED)) {
                    if (visited(decls, children))
                        break;
                    print_packed_decls(b, p->key, children, nb_children, !node->ctx || node->is_active);
                    break;
                }

                for (int i = 0; i < nb_children; i++)
                    print_all_decls(b, children[i], decls);
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(priv + p->offset);
                if (!hmap)
                    break;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry)))
                    print_all_decls(b, entry->data, decls);
                break;
            }
        }
        p++;
    }
}

static void print_link(struct bstr *b,
                       const struct ngl_node *x, const struct ngl_node *y,
                       const char *label)
{
    ngli_bstr_printf(b, "    %s_%p -> %s_%p%s\n",
                    x->class->name, x, y->class->name, y, label);
}

static void print_links(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *links);

static void print_all_links(struct bstr *b, const struct ngl_node *node, struct hmap *links)
{
    if (visited(links, node))
        return;

    print_links(b, node, ngli_base_node_params, (uint8_t *)node, links);
    print_links(b, node, node->class->params, node->priv_data, links);
}

static void print_links(struct bstr *b, const struct ngl_node *node,
                        const struct node_param *p, uint8_t *priv,
                        struct hmap *links)
{
    while (p && p->key) {
        char *label = ngli_asprintf("[label=\"%s\"]",
                                    (p->flags & PARAM_FLAG_DOT_DISPLAY_FIELDNAME) ? p->key : "");
        if (!label)
            return;
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child) {
                    print_link(b, node, child, label);
                    print_all_links(b, child, links);
                }
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                if (nb_children && (p->flags & PARAM_FLAG_DOT_DISPLAY_PACKED)) {
                    ngli_bstr_printf(b, "    %s_%p -> %s_%p%s\n",
                                    node->class->name, node, p->key, children, label);
                    break;
                }

                for (int i = 0; i < nb_children; i++) {
                    char numlbl[64];
                    snprintf(numlbl, sizeof(numlbl), "[label=\"#%d\"]", i);
                    const struct ngl_node *child = children[i];

                    print_link(b, node, child, numlbl);
                    print_all_links(b, child, links);
                }
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(priv + p->offset);
                if (!hmap)
                    break;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry))) {
                    char *key;
                    const struct ngl_node *child = entry->data;

                    if (p->flags & PARAM_FLAG_DOT_DISPLAY_FIELDNAME)
                        key = ngli_asprintf("[label=\"%s:%s\"]", p->key, entry->key);
                    else
                        key = ngli_asprintf("[label=\"%s\"]", entry->key);

                    if (!key) {
                        ngli_free(label);
                        return;
                    }

                    print_link(b, node, child, key);
                    ngli_free(key);

                    print_all_links(b, child, links);
                }
                break;
            }
        }
        ngli_free(label);
        p++;
    }
}

char *ngl_node_dot(const struct ngl_node *node)
{
    if (!node)
        return NULL;

    char *graph = NULL;
    struct hmap *decls = ngli_hmap_create();
    struct hmap *links = ngli_hmap_create();
    struct bstr *b = ngli_bstr_create();
    if (!decls || !links || !b)
        goto end;

    const char *font_settings="fontsize=9,fontname=Arial";

    ngli_bstr_printf(b, "digraph G {\n"
                    "    bgcolor=\"#222222\";\n"
                    "    edge [%s,color=\"#dddddd\",fontcolor=\"#dddddd\",arrowsize=0.7];\n"
                    "    node [style=filled,%s];\n",
                    font_settings, font_settings);

    print_all_decls(b, node, decls);
    print_all_links(b, node, links);

    ngli_bstr_print(b, "}\n");

    graph = ngli_bstr_strdup(b);

end:
    ngli_bstr_freep(&b);
    ngli_hmap_freep(&decls);
    ngli_hmap_freep(&links);
    return graph;
}

char *ngl_dot(struct ngl_ctx *s, double t)
{
    int ret = ngli_prepare_draw(s, t);
    if (ret < 0)
        return NULL;
    return ngl_node_dot(s->scene);
}
