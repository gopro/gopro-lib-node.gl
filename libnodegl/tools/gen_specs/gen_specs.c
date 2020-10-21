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

#include "hmap.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "nodes_register.h"

#define CLASS_LIST(type_name, class) extern const struct node_class class;
NODE_MAP_TYPE2CLASS(CLASS_LIST)

extern const struct node_param ngli_base_node_params[];
extern const struct param_specs ngli_params_specs[];

static void print_node_params(const char *name, const struct node_param *p)
{
    printf("- %s:\n", name);
    if (p) {
        while (p->key) {
            printf("    - [%s, %s]\n", p->key, ngli_params_specs[p->type].name);
            p++;
        }
    }
    printf("\n");
}

#define CLASS_COMMALIST(type_name, class) &class,

int main(void)
{
    printf("#\n# Nodes specifications for node.gl v%d.%d.%d\n#\n\n",
           NODEGL_VERSION_MAJOR, NODEGL_VERSION_MINOR, NODEGL_VERSION_MICRO);
    print_node_params("_Node", ngli_base_node_params);

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
            char *pname = ngli_asprintf("_%s", c->params_id);
            if (!pname) {
                ngli_free(pname);
                return -1;
            }
            if (mapped_param) {
                ngli_assert(mapped_param == p);
            } else {
                print_node_params(pname, p);
                ngli_hmap_set(params_map, c->params_id, (void *)p);
            }
            printf("- %s: %s\n\n", c->name, pname);
            ngli_free(pname);
        } else {
            print_node_params(c->name, p);
        }
    }

    ngli_hmap_freep(&params_map);
    return 0;
}
