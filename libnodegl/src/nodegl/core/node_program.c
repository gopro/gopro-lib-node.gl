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

#include <stddef.h>
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define IO_NODES (const int[]){NGL_NODE_IOINT,        \
                               NGL_NODE_IOIVEC2,      \
                               NGL_NODE_IOIVEC3,      \
                               NGL_NODE_IOIVEC4,      \
                               NGL_NODE_IOUINT,       \
                               NGL_NODE_IOUIVEC2,     \
                               NGL_NODE_IOUIVEC3,     \
                               NGL_NODE_IOUIVEC4,     \
                               NGL_NODE_IOFLOAT,      \
                               NGL_NODE_IOVEC2,       \
                               NGL_NODE_IOVEC3,       \
                               NGL_NODE_IOVEC4,       \
                               NGL_NODE_IOMAT3,       \
                               NGL_NODE_IOMAT4,       \
                               NGL_NODE_IOBOOL,       \
                               -1}

#define OFFSET(x) offsetof(struct program_priv, x)
static const struct node_param program_params[] = {
    {"vertex",   PARAM_TYPE_STR, OFFSET(vertex),   {.str=NULL},
                 .desc=NGLI_DOCSTRING("vertex shader")},
    {"fragment", PARAM_TYPE_STR, OFFSET(fragment), {.str=NULL},
                 .desc=NGLI_DOCSTRING("fragment shader")},
    {"properties", PARAM_TYPE_NODEDICT, OFFSET(properties),
                   .node_types=(const int[]){NGL_NODE_RESOURCEPROPS, -1},
                   .desc=NGLI_DOCSTRING("resource properties")},
    {"vert_out_vars", PARAM_TYPE_NODEDICT, OFFSET(vert_out_vars),
                       .node_types=IO_NODES,
                       .desc=NGLI_DOCSTRING("in/out communication variables shared between vertex and fragment stages")},
    {"nb_frag_output", PARAM_TYPE_INT, OFFSET(nb_frag_output),
                       .desc=NGLI_DOCSTRING("number of color outputs in the fragment shader")},
    {NULL}
};

static int program_init(struct ngl_node *node)
{
    struct program_priv *s = node->priv_data;

    if (!s->vertex || !s->fragment) {
        LOG(ERROR, "both vertex and fragment shaders must be set");
        return NGL_ERROR_INVALID_USAGE;
    }

    return 0;
}

const struct node_class ngli_program_class = {
    .id        = NGL_NODE_PROGRAM,
    .name      = "Program",
    .init      = program_init,
    .priv_size = sizeof(struct program_priv),
    .params    = program_params,
    .file      = __FILE__,
};
