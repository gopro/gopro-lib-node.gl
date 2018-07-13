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

#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#ifdef VULKAN_BACKEND
#include <vulkan/vulkan.h>
#else
#include "glfunctions.h"
#endif
#include "nodegl.h"

#define NGLI_FEATURE_VERTEX_ARRAY_OBJECT          (1 << 0)
#define NGLI_FEATURE_TEXTURE_3D                   (1 << 1)
#define NGLI_FEATURE_TEXTURE_STORAGE              (1 << 2)
#define NGLI_FEATURE_COMPUTE_SHADER               (1 << 3)
#define NGLI_FEATURE_PROGRAM_INTERFACE_QUERY      (1 << 4)
#define NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE      (1 << 5)
#define NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT (1 << 6)
#define NGLI_FEATURE_FRAMEBUFFER_OBJECT           (1 << 7)
#define NGLI_FEATURE_INTERNALFORMAT_QUERY         (1 << 8)
#define NGLI_FEATURE_PACKED_DEPTH_STENCIL         (1 << 9)
#define NGLI_FEATURE_TIMER_QUERY                  (1 << 10)
#define NGLI_FEATURE_EXT_DISJOINT_TIMER_QUERY     (1 << 11)

#define NGLI_FEATURE_COMPUTE_SHADER_ALL (NGLI_FEATURE_COMPUTE_SHADER           | \
                                         NGLI_FEATURE_PROGRAM_INTERFACE_QUERY  | \
                                         NGLI_FEATURE_SHADER_IMAGE_LOAD_STORE  | \
                                         NGLI_FEATURE_SHADER_STORAGE_BUFFER_OBJECT)

#ifdef VULKAN_BACKEND
struct vk_swapchain_support {
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR *formats;
    uint32_t nb_formats;
    VkPresentModeKHR *present_modes;
    uint32_t nb_present_modes;
};
#else
struct glcontext_class;
#endif

struct glcontext {
#ifdef VULKAN_BACKEND
    struct ngl_config config;

    VkDevice device;
    VkExtent2D extent;
    VkRenderPass render_pass;

    VkInstance instance;
    VkDebugReportCallbackEXT report_callback;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceMemoryProperties phydev_mem_props;
    int queue_family_graphics_id;
    int queue_family_present_id;
    VkSurfaceKHR surface;
    struct vk_swapchain_support swapchain_support;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkSwapchainKHR swapchain;
    VkImage *images;
    uint32_t nb_images;
    VkImageView *image_views;
    uint32_t nb_image_views;
    VkFramebuffer *framebuffers;
    int nb_framebuffers;
    VkCommandPool command_pool;
    VkSemaphore *sem_img_avail;
    VkSemaphore *sem_render_finished;
    VkFence *fences;
    VkStructureType surface_create_type;

    VkPipeline graphic_pipelines[64]; // XXX
    int nb_graphic_pipelines;
    //int nb_allocated_graphic_pipelines;

    VkPipelineLayout pipeline_layouts[64]; // XXX
    int nb_pipeline_layouts;
    //int nb_allocated_pipeline_layouts

    VkCommandBuffer *command_buffers;
    int nb_command_buffers;

    uint32_t img_index;

    int nb_in_flight_frames;
    int current_frame;
#else
    /* GL context */
    const struct glcontext_class *class;
    void *priv_data;

    /* User options */
    int platform;
    int backend;
    int wrapped;
    int offscreen;
    int width;
    int height;
    int samples;
    int set_surface_pts;
    float clear_color[4]; // FIXME: update it

    /* GL api */
    int version;

    /* GL features */
    int features;
    int max_texture_image_units;
    int max_compute_work_group_counts[3];

    GLenum gl_1comp;
    GLenum gl_2comp;

    /* GL functions */
    struct glfunctions funcs;
#endif
};

struct glcontext_class {
    int (*init)(struct glcontext *glcontext, uintptr_t display, uintptr_t window, uintptr_t handle);
    int (*create)(struct glcontext *glcontext, uintptr_t other);
    int (*resize)(struct glcontext *glcontext, int width, int height);
    int (*make_current)(struct glcontext *glcontext, int current);
    void (*swap_buffers)(struct glcontext *glcontext);
    int (*set_swap_interval)(struct glcontext *glcontext, int interval);
    void (*set_surface_pts)(struct glcontext *glcontext, double t);
    void* (*get_texture_cache)(struct glcontext *glcontext);
    void* (*get_proc_address)(struct glcontext *glcontext, const char *name);
    void (*uninit)(struct glcontext *glcontext);
    size_t priv_size;
};

struct glcontext *ngli_glcontext_new(const struct ngl_config *config);
int ngli_glcontext_load_extensions(struct glcontext *glcontext);
int ngli_glcontext_make_current(struct glcontext *glcontext, int current);
void ngli_glcontext_swap_buffers(struct glcontext *glcontext);
int ngli_glcontext_set_swap_interval(struct glcontext *glcontext, int interval);
void ngli_glcontext_set_surface_pts(struct glcontext *glcontext, double t);
int ngli_glcontext_resize(struct glcontext *glcontext, int width, int height);
void *ngli_glcontext_get_proc_address(struct glcontext *glcontext, const char *name);
void *ngli_glcontext_get_texture_cache(struct glcontext *glcontext);
void ngli_glcontext_freep(struct glcontext **glcontext);
int ngli_glcontext_check_extension(const char *extension, const char *extensions);
int ngli_glcontext_check_gl_error(const struct glcontext *glcontext, const char *context);

#ifdef VULKAN_BACKEND
// TODO
#else
#include "glwrappers.h"
#endif

#endif /* GLCONTEXT_H */
