/*
 * Copyright 2021 GoPro Inc.
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

#define DEBUG_DISTMAP 1
#if DEBUG_DISTMAP
#include <string.h>
#include <stdio.h>
#endif

#include <math.h>

#include "darray.h"
#include "distmap.h"
#include "distmap_frag.h"
#include "distmap_vert.h"
#include "format.h"
#include "gpu_ctx.h"
#include "log.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "pgcraft.h"
#include "pipeline.h"
#include "rendertarget.h"
#include "texture.h"
#include "topology.h"
#include "type.h"

struct poly3 { // 3rd degree polynomial
    float a, b, c, d;
};

struct shape {
    float x, y, w, h;
};

struct distmap {
    struct ngl_ctx *ctx;

    /*
     * Spread is arbitrary: it represents how far an effect such as glowing
     * could be applied, but it's also used for padding around the shape to be
     * that the extremities of the distance map are always black, and thus not
     * affect neighbor glyph, typically when relying on mipmapping.
     */
    int spread;
    int shape_w, shape_h;

    int texture_w, texture_h;
    int nb_shapes;
    int nb_rows, nb_cols;

    struct darray poly_x;           /* polynomial factors on the x-axis (vec4) */
    struct darray poly_y;           /* polynomial factors on the y-axis (vec4) */
    struct darray poly_start;       /* starting index of the polynomials for a given shape (int) */
    int nb_poly;

    struct texture *texture;
    struct rendertarget *rt;
    struct buffer *vertices;
    struct pgcraft *crafter;
    struct pipeline *pipeline;
};

struct distmap *ngli_distmap_create(struct ngl_ctx *ctx)
{
    struct distmap *d = ngli_calloc(1, sizeof(*d));
    if (!d)
        return NULL;
    d->ctx = ctx;
    ngli_darray_init(&d->poly_x, sizeof(struct poly3), 0);
    ngli_darray_init(&d->poly_y, sizeof(struct poly3), 0);
    ngli_darray_init(&d->poly_start, sizeof(int), 0);
    return d;
}

static const struct pgcraft_iovar vert_out_vars[] = {
    {.name = "var_uvcoord", .type = NGLI_TYPE_VEC2},
};

// XXX closed (text glyph) or not (path)?
int ngli_distmap_init(struct distmap *d, int spread, int shape_w, int shape_h)
{
    d->spread = spread;
    d->shape_w = shape_w;
    d->shape_h = shape_h;
    return 0;
}

int ngli_distmap_add_poly3(struct distmap *d, int shape_id, const float *x, const float *y)
{
    const struct poly3 poly_x = {x[0], x[1], x[2], x[3]};
    const struct poly3 poly_y = {y[0], y[1], y[2], y[3]};

    /* Make sure the shape ID is only incremented, and by one at most. We need
     * this assumption to build the ranges of polynomials for a given shape. */
    ngli_assert(shape_id >= d->nb_shapes - 1);

    /* adding new shapes */
    while (shape_id >= d->nb_shapes) {
        if (!ngli_darray_push(&d->poly_start, &d->nb_poly))
            return NGL_ERROR_MEMORY;
        d->nb_shapes++;
    }

    if (!ngli_darray_push(&d->poly_x, &poly_x) ||
        !ngli_darray_push(&d->poly_y, &poly_y))
        return NGL_ERROR_MEMORY;
    d->nb_poly++;
    return 0;
}

#if DEBUG_DISTMAP
static int save_ppm(const char *filename, uint8_t *data, int width, int height)
{
    int ret = 0;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Unable to open '%s'\n", filename);
        return -1;
    }

    uint8_t *buf = malloc(32 + width * height * 1);
    if (!buf) {
        ret = -1;
        goto end;
    }

    const int header_size = snprintf((char *)buf, 32, "P5 %d %d 255\n", width, height);
    if (header_size < 0) {
        ret = -1;
        fprintf(stderr, "Failed to write PPM header\n");
        goto end;
    }

    uint8_t *dst = buf + header_size;
    for (int i = 0; i < width * height; i++) {
        memcpy(dst, data, 1);
        dst += 1;
        data += 4;
    }

    const int size = header_size + width * height * 1;
    ret = fwrite(buf, 1, size, fp);
    if (ret != size) {
        fprintf(stderr, "Failed to write PPM data\n");
        goto end;
    }

end:
    free(buf);
    fclose(fp);
    return ret;
}
#endif

int ngli_distmap_generate_texture(struct distmap *d)
{
    if (d->texture) {
        LOG(ERROR, "texture already generated");
        return NGL_ERROR_INVALID_USAGE;
    }

    /*
     * Define texture dimension (mostly squared) based on the shape size.
     */
    d->nb_rows = (int)lrintf(sqrtf(d->nb_shapes));
    d->nb_cols = ceilf(d->nb_shapes / (float)d->nb_rows);
    ngli_assert(d->nb_rows * d->nb_cols >= d->nb_shapes);

    const int shape_w_padded = d->shape_w + 2 * d->spread;
    const int shape_h_padded = d->shape_h + 2 * d->spread;
    d->texture_w = shape_w_padded * d->nb_cols;
    d->texture_h = shape_h_padded * d->nb_rows;

    /*
     * Start a dummy next polynomial so that we can obtain the range of
     * polynomial of the last shape in the shader.
     */
    if (!ngli_darray_push(&d->poly_start, &d->nb_poly))
        return NGL_ERROR_MEMORY;

    struct gpu_ctx *gpu_ctx = d->ctx->gpu_ctx;

    const struct texture_params tex_params = {
        .type          = NGLI_TEXTURE_TYPE_2D,
        .width         = d->texture_w,
        .height        = d->texture_h,
        .format        = NGLI_FORMAT_R32_SFLOAT,
        .min_filter    = NGLI_FILTER_LINEAR,
        .mag_filter    = NGLI_FILTER_LINEAR,
        //.mipmap_filter = NGLI_MIPMAP_FILTER_LINEAR,
        .usage         = NGLI_TEXTURE_USAGE_TRANSFER_SRC_BIT
                       | NGLI_TEXTURE_USAGE_TRANSFER_DST_BIT
                       | NGLI_TEXTURE_USAGE_SAMPLED_BIT, // XXX sampled?
    };

    d->texture = ngli_texture_create(gpu_ctx);
    if (!d->texture)
        return NGL_ERROR_MEMORY;

    int ret = ngli_texture_init(d->texture, &tex_params);
    if (ret < 0)
        return ret;

    const struct rendertarget_desc rt_desc = { // XXX can't this be deduced from rt_params?
        .nb_colors = 1,
        .colors[0].format = tex_params.format,
    };
    const struct rendertarget_params rt_params = {
        .width = d->texture_w,
        .height = d->texture_h,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = d->texture,
            .load_op    = NGLI_LOAD_OP_CLEAR,
            .store_op   = NGLI_STORE_OP_STORE,
        },
#if DEBUG_DISTMAP
        .readable = 1,
#endif
    };
    d->rt = ngli_rendertarget_create(gpu_ctx);
    if (!d->rt)
        return NGL_ERROR_MEMORY;
    ret = ngli_rendertarget_init(d->rt, &rt_params);
    if (ret < 0)
        return ret;

    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    d->vertices = ngli_buffer_create(gpu_ctx);
    if (!d->vertices)
        return NGL_ERROR_MEMORY;
    ret = ngli_buffer_init(d->vertices, sizeof(vertices), NGLI_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                          NGLI_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(d->vertices, vertices, sizeof(vertices), 0);
    if (ret < 0)
        return ret;

    // XXX: lifespan?
    // XXX appropriate spread value?
    const float spread_vec[2] = {
        d->spread / (float)(d->shape_w + 2.f*d->spread),
        d->spread / (float)(d->shape_h + 2.f*d->spread),
    };
    const int grid[2] = {d->nb_cols, d->nb_rows};
    const struct pgcraft_uniform uniforms[] = {
        {
            .name  = "spread",
            .type  = NGLI_TYPE_VEC2,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = spread_vec,
        }, {
            .name  = "grid",
            .type  = NGLI_TYPE_IVEC2,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = grid,
        }, {
            .name  = "nb_poly",
            .type  = NGLI_TYPE_INT,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = &d->nb_poly,
        }, {
            .name  = "nb_shapes",
            .type  = NGLI_TYPE_INT,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = &d->nb_shapes,
        }, {
            .name  = "poly_start",
            .type  = NGLI_TYPE_INT,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = ngli_darray_data(&d->poly_start),
            .count = ngli_darray_count(&d->poly_start),
        }, {
            .name  = "poly_x_buf",
            .type  = NGLI_TYPE_VEC4,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = ngli_darray_data(&d->poly_x),
            .count = ngli_darray_count(&d->poly_x),
        }, {
            .name  = "poly_y_buf",
            .type  = NGLI_TYPE_VEC4,
            .stage = NGLI_PROGRAM_SHADER_FRAG,
            .data  = ngli_darray_data(&d->poly_y),
            .count = ngli_darray_count(&d->poly_y),
        }
    };

    const struct pgcraft_attribute attributes[] = {
        {
            .name     = "position",
            .type     = NGLI_TYPE_VEC4,
            .format   = NGLI_FORMAT_R32G32B32A32_SFLOAT,
            .stride   = 4 * 4,
            .buffer   = d->vertices,
        },
    };

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics      = {
            .topology    = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .state       = NGLI_GRAPHICSTATE_DEFAULTS,
            .rt_desc     = rt_desc,
        },
    };

    const struct pgcraft_params crafter_params = {
        .vert_base        = distmap_vert,
        .frag_base        = distmap_frag,
        .uniforms         = uniforms,
        .nb_uniforms      = NGLI_ARRAY_NB(uniforms),
        .attributes       = attributes,
        .nb_attributes    = NGLI_ARRAY_NB(attributes),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };

    d->crafter = ngli_pgcraft_create(d->ctx);
    if (!d->crafter)
        return NGL_ERROR_MEMORY;

    struct pipeline_resource_params pipeline_resource_params = {0};
    ret = ngli_pgcraft_craft(d->crafter, &pipeline_params, &pipeline_resource_params, &crafter_params);
    if (ret < 0)
        return ret;

    d->pipeline = ngli_pipeline_create(gpu_ctx);
    if (!d->pipeline)
        return NGL_ERROR_MEMORY;

    ret = ngli_pipeline_init(d->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    ret = ngli_pipeline_set_resources(d->pipeline, &pipeline_resource_params);
    if (ret < 0)
        return ret;

    /* execute */
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, d->rt);

    int prev_vp[4] = {0};
    ngli_gpu_ctx_get_viewport(gpu_ctx, prev_vp);

    const int vp[4] = {0, 0, d->rt->width, d->rt->height};
    ngli_gpu_ctx_set_viewport(gpu_ctx, vp);

    ngli_pipeline_draw(d->pipeline, 4, 1);

    ngli_gpu_ctx_end_render_pass(gpu_ctx);
    ngli_gpu_ctx_set_viewport(gpu_ctx, prev_vp);

#if DEBUG_DISTMAP
    uint8_t *buf = ngli_calloc(d->rt->width, d->rt->height * 4);
    if (!buf)
        return NGL_ERROR_MEMORY;
    ngli_rendertarget_read_pixels(d->rt, buf);
    save_ppm("/tmp/distmap.ppm", buf, d->rt->width, d->rt->height);
    ngli_freep(&buf);
#endif

    return 0;
}

struct texture *ngli_distmap_get_texture(const struct distmap *d)
{
    return d->texture;
}

void ngli_distmap_get_shape_coords(const struct distmap *d, int shape_id, float *dst)
{
    /* texture must be generated so that all row/cols fields are set */
    ngli_assert(d->texture);

    const int row = shape_id / d->nb_cols;
    const int col = shape_id - row * d->nb_cols;
    const int shape_w_padded = d->shape_w + 2 * d->spread;
    const int shape_h_padded = d->shape_h + 2 * d->spread;
    const int px = col * shape_w_padded + d->spread;
    const int py = row * shape_h_padded + d->spread;
    const float scale_w = 1.f / d->texture_w;
    const float scale_h = 1.f / d->texture_h;
    const float gx = px * scale_w;
    const float gy = py * scale_h;
    const float gw = d->shape_w * scale_w;
    const float gh = d->shape_h * scale_h;
    // XXX: gw and gh should always be the same so we can probably simplify
    // this logic
    const float uvs[] = {
        gx,      gy,
        gx + gw, gy,
        gx,      gy + gh,
        gx + gw, gy + gh,
    };
    //LOG(ERROR, "distmap uvcoord: %g %g %g %g", gx, gy, gw, gh);
    memcpy(dst, uvs, sizeof(uvs));
}

void ngli_distmap_freep(struct distmap **dp)
{
    struct distmap *d = *dp;
    if (!d)
        return;
    ngli_texture_freep(&d->texture);
    ngli_pipeline_freep(&d->pipeline);
    ngli_pgcraft_freep(&d->crafter);
    ngli_buffer_freep(&d->vertices);
    ngli_rendertarget_freep(&d->rt);
    ngli_darray_reset(&d->poly_x);
    ngli_darray_reset(&d->poly_y);
    ngli_darray_reset(&d->poly_start);
    ngli_freep(dp);
}
