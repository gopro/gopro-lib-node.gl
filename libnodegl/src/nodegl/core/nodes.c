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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmap.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "memory.h"
#include "params.h"
#include "utils.h"
#include "nodes_register.h"

enum {
    STATE_INIT_FAILED   = -1,
    STATE_UNINITIALIZED = 0, /* post uninit(), default */
    STATE_INITIALIZED   = 1, /* post init() or release() */
    STATE_READY         = 2, /* post prefetch() */
};

/* We depend on the monotically incrementing by 1 property of these fields */
NGLI_STATIC_ASSERT(node_uniform_vec_flt, NGL_NODE_UNIFORMVEC4      - NGL_NODE_UNIFORMFLOAT       == 3);
NGLI_STATIC_ASSERT(node_animkf_vec_flt,  NGL_NODE_ANIMKEYFRAMEVEC4 - NGL_NODE_ANIMKEYFRAMEFLOAT  == 3);
NGLI_STATIC_ASSERT(node_anim_vec_flt,    NGL_NODE_ANIMATEDVEC4     - NGL_NODE_ANIMATEDFLOAT      == 3);

extern const struct param_specs ngli_params_specs[];

#define OFFSET(x) offsetof(struct ngl_node, x)
const struct node_param ngli_base_node_params[] = {
    {"label",     PARAM_TYPE_STR,      OFFSET(label)},
    {NULL}
};

static void *aligned_allocz(size_t size)
{
    void *ptr = ngli_malloc_aligned(size);
    if (!ptr)
        return NULL;
    memset(ptr, 0, size);
    return ptr;
}

static struct ngl_node *node_create(const struct node_class *class)
{
    struct ngl_node *node;
    const size_t node_size = NGLI_ALIGN(sizeof(*node), NGLI_ALIGN_VAL);

    node = aligned_allocz(node_size + class->priv_size);
    if (!node)
        return NULL;
    node->priv_data = ((uint8_t *)node) + node_size;

    /* Make sure the node and its private data are properly aligned */
    ngli_assert((((uintptr_t)node)            & ~(NGLI_ALIGN_VAL - 1)) == (uintptr_t)node);
    ngli_assert((((uintptr_t)node->priv_data) & ~(NGLI_ALIGN_VAL - 1)) == (uintptr_t)node->priv_data);

    node->class = class;
    node->last_update_time = -1.;
    node->visit_time = -1.;

    node->refcount = 1;

    node->state = STATE_UNINITIALIZED;

    return node;
}

#define DEF_NAME_CHR(c) (((c) >= 'A' && (c) <= 'Z') ? (c) ^ 0x20 : (c))

char *ngli_node_default_label(const char *class_name)
{
    char *label = ngli_strdup(class_name);
    if (!label)
        return NULL;
    for (int i = 0; label[i]; i++)
        label[i] = DEF_NAME_CHR(label[i]);
    return label;
}

int ngli_is_default_label(const char *class_name, const char *str)
{
    const size_t len = strlen(class_name);
    if (len != strlen(str))
        return 0;
    for (int i = 0; i < len; i++)
        if (DEF_NAME_CHR(class_name[i]) != str[i])
            return 0;
    return 1;
}

#define REGISTER_NODE(type_name, class)         \
    case type_name: {                           \
        extern const struct node_class class;   \
        ngli_assert(class.id == type_name);     \
        return &class;                          \
    }                                           \

static const struct node_class *get_node_class(int type)
{
    switch (type) {
        NODE_MAP_TYPE2CLASS(REGISTER_NODE)
    }
    return NULL;
}

struct ngl_node *ngl_node_create(int type)
{
    const struct node_class *class = get_node_class(type);
    if (!class) {
        LOG(ERROR, "unknown node type 0x%x", type);
        return NULL;
    }

    struct ngl_node *node = node_create(class);
    if (!node)
        return NULL;

    if (ngli_params_set_defaults((uint8_t *)node, ngli_base_node_params) < 0 ||
        ngli_params_set_defaults(node->priv_data, node->class->params) < 0) {
        ngl_node_unrefp(&node);
        return NULL;
    }

    node->label = ngli_node_default_label(node->class->name);
    if (!node->label) {
        ngl_node_unrefp(&node);
        return NULL;
    }

    LOG(VERBOSE, "CREATED %s @ %p", node->label, node);

    return node;
}

static void node_release(struct ngl_node *node)
{
    if (node->state != STATE_READY)
        return;

    ngli_assert(node->ctx);
    if (node->class->release) {
        TRACE("RELEASE %s @ %p", node->label, node);
        node->class->release(node);
    }
    node->state = STATE_INITIALIZED;
    node->last_update_time = -1.;
}

/*
 * Reset every field of the private data which is not a parameter. This allows
 * the init() to always be called in a clean state.
 */
static void reset_non_params(struct ngl_node *node)
{
    size_t cur_offset = 0;
    const struct node_param *par = node->class->params;
    uint8_t *base_ptr = node->priv_data;

    while (par && par->key) {
        size_t offset = par->offset;
        if (offset != cur_offset)
            memset(base_ptr + cur_offset, 0, offset - cur_offset);
        cur_offset = offset + ngli_params_specs[par->type].size;
        par++;
    }
    memset(base_ptr + cur_offset, 0, node->class->priv_size - cur_offset);
}

static void node_uninit(struct ngl_node *node)
{
    if (node->state == STATE_UNINITIALIZED)
        return;

    ngli_assert(node->ctx);
    ngli_darray_reset(&node->children);
    node_release(node);

    if (node->class->uninit) {
        LOG(VERBOSE, "UNINIT %s @ %p", node->label, node);
        node->class->uninit(node);
    }
    reset_non_params(node);
    node->state = STATE_UNINITIALIZED;
    node->visit_time = -1.;
}

static int track_children(struct ngl_node *node)
{
    uint8_t *base_ptr = node->priv_data;
    const struct node_param *par = node->class->params;

    if (!par)
        return 0;

    while (par->key) {
        switch (par->type) {
            case PARAM_TYPE_NODE: {
                uint8_t *child_p = base_ptr + par->offset;
                struct ngl_node *child = *(struct ngl_node **)child_p;
                if (child && !ngli_darray_push(&node->children, &child))
                    return NGL_ERROR_MEMORY;
                break;
            }
            case PARAM_TYPE_NODELIST: {
                uint8_t *elems_p = base_ptr + par->offset;
                uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
                struct ngl_node **elems = *(struct ngl_node ***)elems_p;
                const int nb_elems = *(int *)nb_elems_p;
                for (int i = 0; i < nb_elems; i++)
                    if (!ngli_darray_push(&node->children, &elems[i]))
                        return NGL_ERROR_MEMORY;
                break;
            }
            case PARAM_TYPE_NODEDICT: {
                struct hmap *hmap = *(struct hmap **)(base_ptr + par->offset);
                if (!hmap)
                    break;
                const struct hmap_entry *entry = NULL;
                while ((entry = ngli_hmap_next(hmap, entry)))
                    if (!ngli_darray_push(&node->children, &entry->data))
                        return NGL_ERROR_MEMORY;
                break;
            }
        }
        par++;
    }

    return 0;
}

static int check_params_sanity(struct ngl_node *node)
{
    const uint8_t *base_ptr = node->priv_data;
    const struct node_param *par = node->class->params;

    if (!par)
        return 0;

    while (par->key) {
        const void *p = base_ptr + par->offset;
        if ((par->flags & PARAM_FLAG_NON_NULL) && !*(uint8_t **)p) {
            LOG(ERROR, "%s: %s parameter can not be null", node->label, par->key);
            return NGL_ERROR_INVALID_ARG;
        }
        par++;
    }

    return 0;
}

static int node_init(struct ngl_node *node)
{
    if (node->state != STATE_UNINITIALIZED)
        return 0;

    int ret = check_params_sanity(node);
    if (ret < 0)
        return ret;

    ngli_darray_init(&node->children, sizeof(struct ngl_node *), 0);

    ngli_assert(node->ctx);
    if (node->class->init) {
        LOG(VERBOSE, "INIT %s @ %p", node->label, node);
        int ret = node->class->init(node);
        if (ret < 0) {
            LOG(ERROR, "initializing node %s failed: %s", node->label, NGLI_RET_STR(ret));
            node->state = STATE_INIT_FAILED;
            node_uninit(node);
            return ret;
        }
    }

    ret = track_children(node);
    if (ret < 0) {
        node->state = STATE_INIT_FAILED;
        node_uninit(node);
        return ret;
    }

    if (node->class->prefetch)
        node->state = STATE_INITIALIZED;
    else
        node->state = STATE_READY;

    return 0;
}

static int node_set_ctx(struct ngl_node *node, struct ngl_ctx *ctx, struct ngl_ctx *pctx);

static int node_set_children_ctx(uint8_t *base_ptr, const struct node_param *params,
                                 struct ngl_ctx *ctx, struct ngl_ctx *pctx)
{
    if (!params)
        return 0;
    for (int i = 0; params[i].key; i++) {
        const struct node_param *par = &params[i];

        if (par->type == PARAM_TYPE_NODE) {
            uint8_t *node_p = base_ptr + par->offset;
            struct ngl_node *node = *(struct ngl_node **)node_p;
            if (node) {
                int ret = node_set_ctx(node, ctx, pctx);
                if (ret < 0)
                    return ret;
            }
        } else if (par->type == PARAM_TYPE_NODELIST) {
            uint8_t *elems_p = base_ptr + par->offset;
            uint8_t *nb_elems_p = base_ptr + par->offset + sizeof(struct ngl_node **);
            struct ngl_node **elems = *(struct ngl_node ***)elems_p;
            const int nb_elems = *(int *)nb_elems_p;
            for (int j = 0; j < nb_elems; j++) {
                int ret = node_set_ctx(elems[j], ctx, pctx);
                if (ret < 0)
                    return ret;
            }
        } else if (par->type == PARAM_TYPE_NODEDICT) {
            struct hmap *hmap = *(struct hmap **)(base_ptr + par->offset);
            if (!hmap)
                continue;
            const struct hmap_entry *entry = NULL;
            while ((entry = ngli_hmap_next(hmap, entry))) {
                struct ngl_node *node = entry->data;
                int ret = node_set_ctx(node, ctx, pctx);
                if (ret < 0)
                    return ret;
            }
        }
    }
    return 0;
}

static int node_set_ctx(struct ngl_node *node, struct ngl_ctx *ctx, struct ngl_ctx *pctx)
{
    int ret;

    /*
     * If node_set_ctx is used to attach a new context (ctx != NULL), the
     * context and the parent must be equal. This is not the case if
     * node_set_ctx is used to detach the context (ctx = NULL).
     */
    ngli_assert(!ctx || ctx == pctx);

    if (ctx) {
        if (node->ctx && node->ctx != ctx) {
            LOG(ERROR, "\"%s\" is associated with another rendering context", node->label);
            return NGL_ERROR_INVALID_USAGE;
        }
    } else {
        if (node->state > STATE_UNINITIALIZED) {
            if (node->ctx != pctx)
                return 0;
            if (node->ctx_refcount-- == 1) {
                node_uninit(node);
                node->ctx = NULL;
            }
        }
        ngli_assert(node->ctx_refcount >= 0);
    }

    if ((ret = node_set_children_ctx(node->priv_data, node->class->params, ctx, pctx)) < 0 ||
        (ret = node_set_children_ctx((uint8_t *)node, ngli_base_node_params, ctx, pctx)) < 0)
        return ret;

    if (ctx) {
        node->ctx = ctx;
        ret = node_init(node);
        if (ret < 0) {
            node->ctx = NULL;
            return ret;
        }
        node->ctx_refcount++;
    }

    return 0;
}

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    int ret = node_set_ctx(node, ctx, ctx);
    if (ret < 0)
        return ret;

    ret = ngli_node_prepare(node);
    if (ret < 0)
        return ret;

    return ret;
}

void ngli_node_detach_ctx(struct ngl_node *node, struct ngl_ctx *ctx)
{
    int ret = node_set_ctx(node, NULL, ctx);
    ngli_assert(ret == 0);
}

int ngli_node_prepare(struct ngl_node *node)
{
    if (node->class->prepare) {
        TRACE("PREPARE %s @ %p", node->label, node);
        int ret = node->class->prepare(node);
        if (ret < 0) {
            LOG(ERROR, "preparing node %s failed: %s", node->label, NGLI_RET_STR(ret));
            return ret;
        }
    } else {
        struct darray *children_array = &node->children;
        struct ngl_node **children = ngli_darray_data(children_array);
        for (int i = 0; i < ngli_darray_count(children_array); i++) {
            struct ngl_node *child = children[i];
            int ret = ngli_node_prepare(child);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

int ngli_node_visit(struct ngl_node *node, int is_active, double t)
{
    /*
     * If a node is inactive and meant to be, there is no need
     * to check for resources below as we can assume they were already released
     * as well (unless they're shared with another branch) by
     * honor_release_prefetch().
     *
     * On the other hand, we cannot do the same if the node is active, because
     * we have to mark every node below for activity to prevent an early
     * release from another branch.
     */
    if (!is_active && !node->is_active)
        return 0;

    const int queue_node = node->visit_time != t;

    if (queue_node) {
        /*
         * If we never passed through this node for that given time, the new
         * active state takes over to replace the one from a previous update.
         */
        node->is_active = is_active;
        node->visit_time = t;
    } else {
        /*
         * This is not the first time we come across that node, so if it's
         * needed in that part of the branch we mark it as active so it doesn't
         * get released.
         */
        node->is_active |= is_active;
    }

    if (node->class->visit) {
        int ret = node->class->visit(node, is_active, t);
        if (ret < 0)
            return ret;
    } else {
        struct darray *children_array = &node->children;
        struct ngl_node **children = ngli_darray_data(children_array);
        for (int i = 0; i < ngli_darray_count(children_array); i++) {
            struct ngl_node *child = children[i];
            int ret = ngli_node_visit(child, is_active, t);
            if (ret < 0)
                return ret;
        }
    }

    if (queue_node && !ngli_darray_push(&node->ctx->activitycheck_nodes, &node))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int node_prefetch(struct ngl_node *node)
{
    if (node->state == STATE_READY)
        return 0;

    if (node->class->prefetch) {
        TRACE("PREFETCH %s @ %p", node->label, node);
        int ret = node->class->prefetch(node);
        if (ret < 0) {
            LOG(ERROR, "prefetching node %s failed: %s", node->label, NGLI_RET_STR(ret));
            node->visit_time = -1.;
            if (node->class->release) {
                LOG(VERBOSE, "RELEASE %s @ %p", node->label, node);
                node->class->release(node);
            }
            return ret;
        }
    }
    node->state = STATE_READY;

    return 0;
}

int ngli_node_honor_release_prefetch(struct darray *nodes_array)
{
    struct ngl_node **nodes = ngli_darray_data(nodes_array);
    for (int i = 0; i < ngli_darray_count(nodes_array); i++) {
        struct ngl_node *node = nodes[i];

        if (node->is_active) {
            int ret = node_prefetch(node);
            if (ret < 0)
                return ret;
        } else {
            node_release(node);
        }
    }
    return 0;
}

int ngli_node_update(struct ngl_node *node, double t)
{
    ngli_assert(node->state == STATE_READY);
    if (node->class->update) {
        if (node->last_update_time != t) {
            TRACE("UPDATE %s @ %p with t=%g", node->label, node, t);
            int ret = node->class->update(node, t);
            if (ret < 0) {
                LOG(ERROR, "updating node %s failed: %s", node->label, NGLI_RET_STR(ret));
                return ret;
            }
            node->last_update_time = t;
            node->draw_count = 0;
        } else {
            TRACE("%s already updated for t=%g, skip it", node->label, t);
        }
    }

    return 0;
}

void ngli_node_draw(struct ngl_node *node)
{
    if (node->class->draw) {
        TRACE("DRAW %s @ %p", node->label, node);
        node->class->draw(node);
        node->draw_count++;
    }
}

const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp)
{
    const struct node_param *par = ngli_params_find(ngli_base_node_params, key);
    *base_ptrp = (uint8_t *)node;

    if (!par) {
        par = ngli_params_find(node->class->params, key);
        *base_ptrp = (uint8_t *)node->priv_data;
    }
    if (!par)
        LOG(ERROR, "parameter %s not found in %s", key, node->class->name);
    return par;
}

int ngl_node_param_add(struct ngl_node *node, const char *key,
                       int nb_elems, void *elems)
{
    int ret = 0;

    uint8_t *base_ptr;
    const struct node_param *par = ngli_node_param_find(node, key, &base_ptr);
    if (!par)
        return NGL_ERROR_NOT_FOUND;

    if (node->ctx && !(par->flags & PARAM_FLAG_ALLOW_LIVE_CHANGE)) {
        LOG(ERROR, "%s.%s can not be live extended", node->label, key);
        return NGL_ERROR_INVALID_USAGE;
    }

    ret = ngli_params_add(base_ptr, par, nb_elems, elems);
    if (ret < 0) {
        LOG(ERROR, "unable to add elements to %s.%s", node->label, key);
        return ret;
    }

    if (node->ctx && par->update_func)
        ret = par->update_func(node);

    return ret;
}

int ngl_node_param_set(struct ngl_node *node, const char *key, ...)
{
    int ret = 0;
    va_list ap;

    uint8_t *base_ptr;
    const struct node_param *par = ngli_node_param_find(node, key, &base_ptr);
    if (!par)
        return NGL_ERROR_NOT_FOUND;

    if (node->ctx && !(par->flags & PARAM_FLAG_ALLOW_LIVE_CHANGE)) {
        LOG(ERROR, "%s.%s can not be live changed", node->label, key);
        return NGL_ERROR_INVALID_USAGE;
    }

    va_start(ap, key);
    ret = ngli_params_set(base_ptr, par, &ap);
    va_end(ap);
    if (ret < 0) {
        LOG(ERROR, "unable to set %s.%s", node->label, key);
        return ret;
    }

    if (node->ctx && par->update_func)
        ret = par->update_func(node);

    return ret;
}

struct ngl_node *ngl_node_ref(struct ngl_node *node)
{
    node->refcount++;
    return node;
}

void ngl_node_unrefp(struct ngl_node **nodep)
{
    int delete = 0;
    struct ngl_node *node = *nodep;

    if (!node)
        return;
    delete = node->refcount-- == 1;
    if (delete) {
        LOG(VERBOSE, "DELETE %s @ %p", node->label, node);
        ngli_assert(!node->ctx);
        ngli_params_free((uint8_t *)node, ngli_base_node_params);
        ngli_params_free(node->priv_data, node->class->params);
        ngli_free_aligned(node);
    }
    *nodep = NULL;
}
