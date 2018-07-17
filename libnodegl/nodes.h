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

#ifndef NODES_H
#define NODES_H

#include <stdlib.h>
#include <sxplayer.h>
#include <pthread.h>

#ifdef TARGET_ANDROID
#include "android_handlerthread.h"
#include "android_surface.h"
#endif

#ifdef TARGET_IPHONE
#include <CoreVideo/CoreVideo.h>
#endif

#ifdef VULKAN_BACKEND
#include <vulkan/vulkan.h>
#else
#include "glincludes.h"
#endif

#include "glcontext.h"
#include "glstate.h"

#include "hmap.h"
#include "nodegl.h"
#include "params.h"

struct node_class;

enum {
    STATE_UNINITIALIZED = 0, /* post uninit(), default */
    STATE_INITIALIZED   = 1, /* post init() */
    STATE_READY         = 2, /* post prefetch() */
    STATE_IDLE          = 3, /* post release() */
};

typedef int (*cmd_func_type)(struct ngl_ctx *s, void *arg);

struct ngl_ctx {
    /* Controller-only fields */
    int configured;
    pthread_t worker_tid;
    int has_thread;

    /* Worker-only fields */
    struct glcontext *glcontext;
    const struct backend *backend;
    struct glstate glstate;
    struct ngl_node *scene;
    struct ngl_config config;
    int timer_active;

    /* Shared fields */
    pthread_mutex_t lock;
    pthread_cond_t cond_ctl;
    pthread_cond_t cond_wkr;
    cmd_func_type cmd_func;
    void *cmd_arg;
    int cmd_ret;
};

struct ngl_node {
    const struct node_class *class;
    struct ngl_ctx *ctx;

    int refcount;
    NGLI_ALIGNED_MAT(modelview_matrix);
    NGLI_ALIGNED_MAT(projection_matrix);
    int state;

    double last_update_time;

    int is_active;
    double visit_time;

    char *name;

    void *priv_data;
};

#define TRANSFORM_TYPES_LIST (const int[]){NGL_NODE_ROTATE,    \
                                           NGL_NODE_TRANSFORM, \
                                           NGL_NODE_TRANSLATE, \
                                           NGL_NODE_SCALE,     \
                                           -1}

struct graphicconfig {
    struct ngl_node *child;

    int blend;
    int blend_src_factor;
    int blend_dst_factor;
    int blend_src_factor_a;
    int blend_dst_factor_a;
    int blend_op;
    int blend_op_a;

    int color_write_mask;

    int depth_test;
    int depth_write_mask;
    int depth_func;

    int stencil_test;
    int stencil_write_mask;
    int stencil_func;
    int stencil_ref;
    int stencil_read_mask;
    int stencil_fail;
    int stencil_depth_fail;
    int stencil_depth_pass;

    struct glstate states[2];
};

struct camera {
    struct ngl_node *child;
    float eye[3];
    float center[3];
    float up[3];
    float perspective[4];

    struct ngl_node *eye_transform;
    struct ngl_node *center_transform;
    struct ngl_node *up_transform;

    struct ngl_node *fov_anim;

    float ground[3];

    int pipe_fd;
    int pipe_width, pipe_height;
    int hflip;
    uint8_t *pipe_buf;

#ifdef VULKAN_BACKEND
    // TODO
#else
    GLuint framebuffer_id;
    GLuint texture_id;
#endif
};

struct geometry {
    /* quad params */
    float quad_corner[3];
    float quad_width[3];
    float quad_height[3];

    float quad_uv_corner[2];
    float quad_uv_width[2];
    float quad_uv_height[2];

    /* triangle params */
    float triangle_edges[9];
    float triangle_uvs[6];

    /* circle params */
    double radius;
    int npoints;

    /* geometry params */
    struct ngl_node *vertices_buffer;
    struct ngl_node *uvcoords_buffer;
    struct ngl_node *normals_buffer;
    struct ngl_node *indices_buffer;

    int draw_mode;
};

struct ngl_node *ngli_geometry_generate_buffer(struct ngl_ctx *ctx, int type, int count, int size, void *data);
struct ngl_node *ngli_geometry_generate_indices_buffer(struct ngl_ctx *ctx, int count);

struct buffer {
    int count;              // number of elements
    uint8_t *data;          // buffer of <count> elements
    int data_size;          // total buffer data size in bytes
    char *filename;         // filename from which the data will be read
    int data_comp;          // number of components per element
    int data_stride;        // stride of 1 element, in bytes


#ifdef VULKAN_BACKEND
    VkFormat data_format;
    // XXX: usage?
#else
    GLenum data_comp_type;  // type of a single component: integer, float, ...
    GLenum usage;
#endif

    /* animatedbuffer */
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;

    int fd;

    /* private option that must be set before calling ngl_node_init() to enable
     * the generation of a GL buffer feed with the buffer data; mandatory for
     * buffers used as geometry, attributes or shader storage buffer objects */
    int generate_gl_buffer;

#ifdef VULKAN_BACKEND
    // TODO
    VkBuffer vkbuf;
    VkDeviceMemory vkmem;
#else
    GLuint buffer_id;
#endif
};

struct uniform {
    double scalar;
    float vector[4];
    float matrix[4*4];
    int ival;
    struct ngl_node *anim;
    struct ngl_node *transform;
};

struct rtt {
    struct ngl_node *child;
    struct ngl_node *color_texture;
    struct ngl_node *depth_texture;
    int samples;
    int width;
    int height;

#ifdef VULKAN_BACKEND
    // TODO
#else
    GLuint framebuffer_id;
    GLuint renderbuffer_id;
    GLuint stencilbuffer_id;

    GLuint framebuffer_ms_id;
    GLuint colorbuffer_ms_id;
    GLuint depthbuffer_ms_id;
#endif
};

struct program {
#ifdef VULKAN_BACKEND
    uint8_t *vert_data;
    int vert_data_size;
    uint8_t *frag_data;
    int frag_data_size;

    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkPipelineShaderStageCreateInfo shader_stage_create_info[2];

    int position_location_id;
    int uvcoord_location_id;
    int normal_location_id;
    int modelview_matrix_location_id;
    int projection_matrix_location_id;
    int normal_matrix_location_id;
#else
    const char *vertex;
    const char *fragment;

    GLuint program_id;
    GLint position_location_id;
    GLint uvcoord_location_id;
    GLint normal_location_id;
    GLint modelview_matrix_location_id;
    GLint projection_matrix_location_id;
    GLint normal_matrix_location_id;
#endif
};

struct computeprogram {
    const char *compute;

#ifdef VULKAN_BACKEND
    // TODO
#else
    GLuint program_id;
#endif
};

enum hwupload_fmt {
    NGLI_HWUPLOAD_FMT_NONE,
    NGLI_HWUPLOAD_FMT_COMMON,
    NGLI_HWUPLOAD_FMT_MEDIACODEC,
    NGLI_HWUPLOAD_FMT_MEDIACODEC_DR,
    NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_BGRA,
    NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_RGBA,
    NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12,
    NGLI_HWUPLOAD_FMT_VIDEOTOOLBOX_NV12_DR,
};

struct texture {

#ifdef VULKAN_BACKEND
    // TODO

    int width;
    int height;
    int depth;

    int wrap_s;
    int wrap_t;
    int wrap_r;

    struct ngl_node *data_src;
    int direct_rendering;
    int immutable;
#else
    GLenum target;
    GLint format;
    GLint internal_format;
    GLint type;
    int width;
    int height;
    int depth;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
    GLint wrap_r;
    struct ngl_node *data_src;
    GLenum access;
    int direct_rendering;
    int immutable;

    GLuint external_id;
    GLenum external_target;

    NGLI_ALIGNED_MAT(coordinates_matrix);
    GLuint id;
    GLuint local_id;
    GLenum local_target;
#endif

    enum hwupload_fmt upload_fmt;
    struct ngl_node *quad;
    struct ngl_node *program;
    struct ngl_node *render;
    struct ngl_node *textures[3];
    struct ngl_node *target_texture;
    struct ngl_node *rtt;

#ifdef TARGET_IPHONE
    CVOpenGLESTextureRef ios_textures[2];
#endif

    double data_src_ts;
};

#ifndef VULKAN_BACKEND
GLenum ngli_texture_get_sized_internal_format(struct glcontext *glcontext,
                                              GLenum internal_format,
                                              GLenum type);
#endif

int ngli_texture_update_local_texture(struct ngl_node *node,
                                      int width, int height, int depth,
                                      const uint8_t *data);

struct uniformprograminfo {
#ifdef VULKAN_BACKEND
    // TODO
#else
    GLint id;
    GLint size;
    GLenum type;
#endif
    char name[64];
};

struct textureprograminfo {
    int sampling_mode_id;
    int sampler_id;
#if defined(TARGET_ANDROID)
    int external_sampler_id;
#elif defined(TARGET_IPHONE)
    int y_sampler_id;
    int uv_sampler_id;
#endif
    int coord_matrix_id;
    int dimensions_id;
    int ts_id;
};

struct render {
    struct ngl_node *geometry;
    struct ngl_node *program;

    struct hmap *textures;
    struct textureprograminfo *textureprograminfos;
    int disable_1st_texture_unit;

    struct hmap *uniforms;
    struct uniformprograminfo *uniform_ids;
    int nb_uniform_ids;

    struct hmap *attributes;
    struct hmap *buffers;
#ifdef VULKAN_BACKEND
    int *attribute_ids;
    int *buffer_ids;

    int last_width;
    int last_height;

    int pipeline_id;

    VkVertexInputBindingDescription bind_descs[64]; // FIXME alloc
    VkVertexInputAttributeDescription attr_descs[64]; // FIXME alloc
    VkBuffer vkbuffers[64]; // FIXME alloc
    VkDeviceSize offsets[64]; // FIXME alloc
    int nb_binds;
#else
    GLint *attribute_ids;
    GLint *buffer_ids;

    GLuint vao_id;
#endif
};

struct compute {
    int nb_group_x;
    int nb_group_y;
    int nb_group_z;

    struct ngl_node *program;

    struct hmap *textures;
    struct textureprograminfo *textureprograminfos;

    struct hmap *uniforms;
    struct uniformprograminfo *uniform_ids;
    int nb_uniform_ids;

    struct hmap *attributes;
    struct hmap *buffers;

#ifdef VULKAN_BACKEND
    int *attribute_ids;
    int *buffer_ids;
#else
    GLint *attribute_ids;
    GLint *buffer_ids;
#endif
};

struct media {
    const char *filename;
    const char *sxplayer_min_level_str;
    struct ngl_node *anim;
    int audio_tex;
    int max_nb_packets;
    int max_nb_frames;
    int max_nb_sink;
    int max_pixels;

    int sxplayer_min_level;

    struct sxplayer_ctx *player;
    struct sxplayer_frame *frame;

#ifdef TARGET_ANDROID
    GLuint android_texture_id;
    GLenum android_texture_target;
    struct android_surface *android_surface;
    struct android_handlerthread *android_handlerthread;
#endif
};

struct timerangemode {
    double start_time;
    double render_time;
    int updated;
};

struct rotate {
    struct ngl_node *child;
    double angle;
    float axis[3];
    float anchor[3];
    struct ngl_node *anim;
};

struct transform {
    struct ngl_node *child;
    NGLI_ALIGNED_MAT(matrix);
};

struct translate {
    struct ngl_node *child;
    float vector[3];
    struct ngl_node *anim;
};

struct scale {
    struct ngl_node *child;
    float factors[3];
    float anchor[3];
    struct ngl_node *anim;
};

enum easing_id {
    EASING_LINEAR,
    EASING_QUADRATIC_IN,
    EASING_QUADRATIC_OUT,
    EASING_QUADRATIC_IN_OUT,
    EASING_QUADRATIC_OUT_IN,
    EASING_CUBIC_IN,
    EASING_CUBIC_OUT,
    EASING_CUBIC_IN_OUT,
    EASING_CUBIC_OUT_IN,
    EASING_QUARTIC_IN,
    EASING_QUARTIC_OUT,
    EASING_QUARTIC_IN_OUT,
    EASING_QUARTIC_OUT_IN,
    EASING_QUINTIC_IN,
    EASING_QUINTIC_OUT,
    EASING_QUINTIC_IN_OUT,
    EASING_QUINTIC_OUT_IN,
    EASING_SINUS_IN,
    EASING_SINUS_OUT,
    EASING_SINUS_IN_OUT,
    EASING_SINUS_OUT_IN,
    EASING_EXP_IN,
    EASING_EXP_OUT,
    EASING_EXP_IN_OUT,
    EASING_EXP_OUT_IN,
    EASING_CIRCULAR_IN,
    EASING_CIRCULAR_OUT,
    EASING_CIRCULAR_IN_OUT,
    EASING_CIRCULAR_OUT_IN,
    EASING_BOUNCE_IN,
    EASING_BOUNCE_OUT,
    EASING_ELASTIC_IN,
    EASING_ELASTIC_OUT,
    EASING_BACK_IN,
    EASING_BACK_OUT,
    EASING_BACK_IN_OUT,
    EASING_BACK_OUT_IN,
};

typedef double easing_type;
typedef easing_type (*easing_function)(easing_type, int, const easing_type *);

struct animation {
    struct ngl_node **animkf;
    int nb_animkf;
    int current_kf;
    int eval_current_kf;
    float values[4];
    double scalar;
};

struct animkeyframe {
    double time;
    float value[4];
    double scalar;
    uint8_t *data;
    int data_size;
    int easing;
    easing_function function;
    easing_function resolution;
    double *args;
    int nb_args;
};

struct hud_measuring {
    int64_t *times;
    int count;
    int pos;
    int64_t total_times;
};

struct hud_data_graph {
    int64_t *values;
    int count;
    int pos;
    int64_t min;
    int64_t max;
};

struct hud {
    struct ngl_node *child;
    int measure_window;
    int refresh_rate[2];
    char *export_filename;
    float bg_color[4];

    struct hud_measuring measures[6];
    struct hud_data_graph graph[6];
    uint32_t bg_color_u32;
    int fd_export;
    struct bstr *csv_line;
    uint8_t *data_buf;
    int data_w, data_h;
    double refresh_rate_interval;
    double last_refresh_time;
    int need_refresh;
    int64_t graph_min;
    int64_t graph_max;

#ifndef VULKAN_BACKEND
    GLuint query;
    void (*glGenQueries)(const struct glcontext *gl, GLsizei n, GLuint * ids);
    void (*glDeleteQueries)(const struct glcontext *gl, GLsizei n, const GLuint * ids);
    void (*glBeginQuery)(const struct glcontext *gl, GLenum target, GLuint id);
    void (*glEndQuery)(const struct glcontext *gl, GLenum target);
    void (*glGetQueryObjectui64v)(const struct glcontext *gl, GLuint id, GLenum pname, GLuint64 *params);
#endif
};

/**
 *   Operation        State result
 * -----------------------------------
 * I Init           STATE_INITIALIZED
 * P Prefetch       STATE_READY
 * D Update/Draw
 * R Release        STATE_IDLE
 * U Uninit         STATE_INITIALIZED
 *
 * Dependency callgraph:
 *
 *             .------------.
 *             v            |
 *     (I) <- [P] <- [D]   (R) <- [U]
 *      |                          ^
 *      `--------------------------'
 *
 * The starting state is [U].
 *
 * .--[ Legend ]-------
 * |
 * | X:      Operation X
 * | X -> Y: X call depends on Y state result
 * | [X]:    if X dependency is not met, the change state call will be made
 * | (X):    if X dependency is not met, it will noop
 * |
 * `-------------------
 *
 * Some examples:
 *  - calling prefetch() will always call init() if necessary
 *  - release() has a weak dependency to prefetch(), so it will noop if not in
 *    the READY state.
 *
 * Note: nodes implementation do NOT have to implement this logic, but they can
 * rely on these properties in their callback implementations.
 */
struct node_class {
    int id;
    const char *name;
    int (*init)(struct ngl_node *node);
    int (*visit)(struct ngl_node *node, int is_active, double t);
    int (*prefetch)(struct ngl_node *node);
    int (*update)(struct ngl_node *node, double t);
    void (*draw)(struct ngl_node *node);
    void (*release)(struct ngl_node *node);
    void (*uninit)(struct ngl_node *node);
    char *(*info_str)(const struct ngl_node *node);
    size_t priv_size;
    const struct node_param *params;
    const char *params_id;
    const char *file;
};

void ngli_node_print_specs(void);

int ngli_dispatch_cmd(struct ngl_ctx *s, cmd_func_type cmd_func, void *arg);

int ngli_node_init(struct ngl_node *node);
int ngli_node_visit(struct ngl_node *node, int is_active, double t);
int ngli_node_honor_release_prefetch(struct ngl_node *node, double t);
int ngli_node_update(struct ngl_node *node, double t);
int ngli_prepare_draw(struct ngl_ctx *s, double t);
void ngli_node_draw(struct ngl_node *node);

int ngli_node_attach_ctx(struct ngl_node *node, struct ngl_ctx *ctx);
void ngli_node_detach_ctx(struct ngl_node *node);

void ngli_node_transfer_matrices(struct ngl_node *dst, const struct ngl_node *src);
char *ngli_node_default_name(const char *class_name);
int ngli_is_default_name(const char *class_name, const char *str);
struct ngl_node *ngli_node_create_noconstructor(int type);
const struct node_param *ngli_node_param_find(const struct ngl_node *node, const char *key,
                                              uint8_t **base_ptrp);

#endif
