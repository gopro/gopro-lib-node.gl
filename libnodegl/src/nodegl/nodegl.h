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

#ifndef NODEGL_H
#define NODEGL_H

#define NODEGL_VERSION_MAJOR 0
#define NODEGL_VERSION_MINOR 0
#define NODEGL_VERSION_MICRO 0

#define NODEGL_GET_VERSION(major, minor, micro) ((major)<<16 | (minor)<<8 | (micro))

#define NODEGL_VERSION_INT NODEGL_GET_VERSION(NODEGL_VERSION_MAJOR, \
                                              NODEGL_VERSION_MINOR, \
                                              NODEGL_VERSION_MICRO)

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Logging levels
 */
enum {
    NGL_LOG_VERBOSE,
    NGL_LOG_DEBUG,
    NGL_LOG_INFO,
    NGL_LOG_WARNING,
    NGL_LOG_ERROR,
};

/**
 * Logging callback prototype.
 *
 * @param arg       forwarded opaque user argument
 * @param level     log level (any of NGL_LOG_*)
 * @param filename  source filename from where the message originates
 * @param ln        line in the source filename
 * @param fn        calling function name
 * @param fmt       printf format string
 * @param vl        variable argument list object associated with the fmt format string
 */
typedef void (*ngl_log_callback_type)(void *arg, int level, const char *filename,
                                      int ln, const char *fn, const char *fmt, va_list vl);

/**
 * Set a global custom logging callback.
 *
 * @param arg       opaque user argument to be forwarded to the callback
 *                  (typically a user context)
 * @param callback  callback function to be called when logging a message
 */
void ngl_log_set_callback(void *arg, ngl_log_callback_type callback);

/**
 * Set the minimum global logging level.
 *
 * No message with its level inferior to the specified level will be logged
 * (with or without the callback set).
 *
 * @param level log level (any of NGL_LOG_*)
 */
void ngl_log_set_min_level(int level);

/**
 * Opaque structure identifying a node
 */
struct ngl_node;

#define NGLI_FOURCC(a,b,c,d) (((a))<<24 | (b)<<16 | (c)<<8 | (d))

/**
 * Node FOURCC identifiers
 */
#define NGL_NODE_ANIMATEDBUFFERFLOAT    NGLI_FOURCC('A','B','f','1')
#define NGL_NODE_ANIMATEDBUFFERVEC2     NGLI_FOURCC('A','B','f','2')
#define NGL_NODE_ANIMATEDBUFFERVEC3     NGLI_FOURCC('A','B','f','3')
#define NGL_NODE_ANIMATEDBUFFERVEC4     NGLI_FOURCC('A','B','f','4')
#define NGL_NODE_ANIMATEDTIME           NGLI_FOURCC('A','n','m','T')
#define NGL_NODE_ANIMATEDFLOAT          NGLI_FOURCC('A','n','m','1')
#define NGL_NODE_ANIMATEDVEC2           NGLI_FOURCC('A','n','m','2')
#define NGL_NODE_ANIMATEDVEC3           NGLI_FOURCC('A','n','m','3')
#define NGL_NODE_ANIMATEDVEC4           NGLI_FOURCC('A','n','m','4')
#define NGL_NODE_ANIMATEDQUAT           NGLI_FOURCC('A','n','m','Q')
#define NGL_NODE_ANIMKEYFRAMEBUFFER     NGLI_FOURCC('A','K','F','B')
#define NGL_NODE_ANIMKEYFRAMEFLOAT      NGLI_FOURCC('A','K','F','1')
#define NGL_NODE_ANIMKEYFRAMEVEC2       NGLI_FOURCC('A','K','F','2')
#define NGL_NODE_ANIMKEYFRAMEVEC3       NGLI_FOURCC('A','K','F','3')
#define NGL_NODE_ANIMKEYFRAMEVEC4       NGLI_FOURCC('A','K','F','4')
#define NGL_NODE_ANIMKEYFRAMEQUAT       NGLI_FOURCC('A','K','F','Q')
#define NGL_NODE_BLOCK                  NGLI_FOURCC('B','l','c','k')
#define NGL_NODE_BUFFERBYTE             NGLI_FOURCC('B','s','b','1')
#define NGL_NODE_BUFFERBVEC2            NGLI_FOURCC('B','s','b','2')
#define NGL_NODE_BUFFERBVEC3            NGLI_FOURCC('B','s','b','3')
#define NGL_NODE_BUFFERBVEC4            NGLI_FOURCC('B','s','b','4')
#define NGL_NODE_BUFFERINT              NGLI_FOURCC('B','s','i','1')
#define NGL_NODE_BUFFERINT64            NGLI_FOURCC('B','s','l','1')
#define NGL_NODE_BUFFERIVEC2            NGLI_FOURCC('B','s','i','2')
#define NGL_NODE_BUFFERIVEC3            NGLI_FOURCC('B','s','i','3')
#define NGL_NODE_BUFFERIVEC4            NGLI_FOURCC('B','s','i','4')
#define NGL_NODE_BUFFERSHORT            NGLI_FOURCC('B','s','s','1')
#define NGL_NODE_BUFFERSVEC2            NGLI_FOURCC('B','s','s','2')
#define NGL_NODE_BUFFERSVEC3            NGLI_FOURCC('B','s','s','3')
#define NGL_NODE_BUFFERSVEC4            NGLI_FOURCC('B','s','s','4')
#define NGL_NODE_BUFFERUBYTE            NGLI_FOURCC('B','u','b','1')
#define NGL_NODE_BUFFERUBVEC2           NGLI_FOURCC('B','u','b','2')
#define NGL_NODE_BUFFERUBVEC3           NGLI_FOURCC('B','u','b','3')
#define NGL_NODE_BUFFERUBVEC4           NGLI_FOURCC('B','u','b','4')
#define NGL_NODE_BUFFERUINT             NGLI_FOURCC('B','u','i','1')
#define NGL_NODE_BUFFERUIVEC2           NGLI_FOURCC('B','u','i','2')
#define NGL_NODE_BUFFERUIVEC3           NGLI_FOURCC('B','u','i','3')
#define NGL_NODE_BUFFERUIVEC4           NGLI_FOURCC('B','u','i','4')
#define NGL_NODE_BUFFERUSHORT           NGLI_FOURCC('B','u','s','1')
#define NGL_NODE_BUFFERUSVEC2           NGLI_FOURCC('B','u','s','2')
#define NGL_NODE_BUFFERUSVEC3           NGLI_FOURCC('B','u','s','3')
#define NGL_NODE_BUFFERUSVEC4           NGLI_FOURCC('B','u','s','4')
#define NGL_NODE_BUFFERFLOAT            NGLI_FOURCC('B','f','v','1')
#define NGL_NODE_BUFFERVEC2             NGLI_FOURCC('B','f','v','2')
#define NGL_NODE_BUFFERVEC3             NGLI_FOURCC('B','f','v','3')
#define NGL_NODE_BUFFERVEC4             NGLI_FOURCC('B','f','v','4')
#define NGL_NODE_BUFFERMAT4             NGLI_FOURCC('B','f','m','4')
#define NGL_NODE_CAMERA                 NGLI_FOURCC('C','m','r','a')
#define NGL_NODE_CIRCLE                 NGLI_FOURCC('C','r','c','l')
#define NGL_NODE_COMPUTE                NGLI_FOURCC('C','p','t',' ')
#define NGL_NODE_COMPUTEPROGRAM         NGLI_FOURCC('C','p','t','P')
#define NGL_NODE_GEOMETRY               NGLI_FOURCC('G','e','o','m')
#define NGL_NODE_GRAPHICCONFIG          NGLI_FOURCC('G','r','C','f')
#define NGL_NODE_GROUP                  NGLI_FOURCC('G','r','p',' ')
#define NGL_NODE_HUD                    NGLI_FOURCC('H','U','D',' ')
#define NGL_NODE_IDENTITY               NGLI_FOURCC('I','d',' ',' ')
#define NGL_NODE_IOINT                  NGLI_FOURCC('I','O','i','1')
#define NGL_NODE_IOIVEC2                NGLI_FOURCC('I','O','i','2')
#define NGL_NODE_IOIVEC3                NGLI_FOURCC('I','O','i','3')
#define NGL_NODE_IOIVEC4                NGLI_FOURCC('I','O','i','4')
#define NGL_NODE_IOUINT                 NGLI_FOURCC('I','O','u','1')
#define NGL_NODE_IOUIVEC2               NGLI_FOURCC('I','O','u','2')
#define NGL_NODE_IOUIVEC3               NGLI_FOURCC('I','O','u','3')
#define NGL_NODE_IOUIVEC4               NGLI_FOURCC('I','O','u','4')
#define NGL_NODE_IOFLOAT                NGLI_FOURCC('I','O','f','1')
#define NGL_NODE_IOVEC2                 NGLI_FOURCC('I','O','f','2')
#define NGL_NODE_IOVEC3                 NGLI_FOURCC('I','O','f','3')
#define NGL_NODE_IOVEC4                 NGLI_FOURCC('I','O','f','4')
#define NGL_NODE_IOMAT3                 NGLI_FOURCC('I','O','m','3')
#define NGL_NODE_IOMAT4                 NGLI_FOURCC('I','O','m','4')
#define NGL_NODE_IOBOOL                 NGLI_FOURCC('I','O','b','1')
#define NGL_NODE_MEDIA                  NGLI_FOURCC('M','d','i','a')
#define NGL_NODE_PROGRAM                NGLI_FOURCC('P','r','g','m')
#define NGL_NODE_QUAD                   NGLI_FOURCC('Q','u','a','d')
#define NGL_NODE_RENDER                 NGLI_FOURCC('R','n','d','r')
#define NGL_NODE_RENDERTOTEXTURE        NGLI_FOURCC('R','T','T',' ')
#define NGL_NODE_RESOURCEPROPS          NGLI_FOURCC('R','e','s','P')
#define NGL_NODE_ROTATE                 NGLI_FOURCC('T','R','o','t')
#define NGL_NODE_ROTATEQUAT             NGLI_FOURCC('T','R','o','Q')
#define NGL_NODE_SCALE                  NGLI_FOURCC('T','s','c','l')
#define NGL_NODE_STREAMEDINT            NGLI_FOURCC('S','t','i','1')
#define NGL_NODE_STREAMEDIVEC2          NGLI_FOURCC('S','t','i','2')
#define NGL_NODE_STREAMEDIVEC3          NGLI_FOURCC('S','t','i','3')
#define NGL_NODE_STREAMEDIVEC4          NGLI_FOURCC('S','t','i','4')
#define NGL_NODE_STREAMEDUINT           NGLI_FOURCC('S','t','u','1')
#define NGL_NODE_STREAMEDUIVEC2         NGLI_FOURCC('S','t','u','2')
#define NGL_NODE_STREAMEDUIVEC3         NGLI_FOURCC('S','t','u','3')
#define NGL_NODE_STREAMEDUIVEC4         NGLI_FOURCC('S','t','u','4')
#define NGL_NODE_STREAMEDFLOAT          NGLI_FOURCC('S','t','f','1')
#define NGL_NODE_STREAMEDVEC2           NGLI_FOURCC('S','t','f','2')
#define NGL_NODE_STREAMEDVEC3           NGLI_FOURCC('S','t','f','3')
#define NGL_NODE_STREAMEDVEC4           NGLI_FOURCC('S','t','f','4')
#define NGL_NODE_STREAMEDMAT4           NGLI_FOURCC('S','t','m','4')
#define NGL_NODE_STREAMEDBUFFERINT      NGLI_FOURCC('S','B','i','1')
#define NGL_NODE_STREAMEDBUFFERIVEC2    NGLI_FOURCC('S','B','i','2')
#define NGL_NODE_STREAMEDBUFFERIVEC3    NGLI_FOURCC('S','B','i','3')
#define NGL_NODE_STREAMEDBUFFERIVEC4    NGLI_FOURCC('S','B','i','4')
#define NGL_NODE_STREAMEDBUFFERUINT     NGLI_FOURCC('S','B','u','1')
#define NGL_NODE_STREAMEDBUFFERUIVEC2   NGLI_FOURCC('S','B','u','2')
#define NGL_NODE_STREAMEDBUFFERUIVEC3   NGLI_FOURCC('S','B','u','3')
#define NGL_NODE_STREAMEDBUFFERUIVEC4   NGLI_FOURCC('S','B','u','4')
#define NGL_NODE_STREAMEDBUFFERFLOAT    NGLI_FOURCC('S','B','f','1')
#define NGL_NODE_STREAMEDBUFFERVEC2     NGLI_FOURCC('S','B','f','2')
#define NGL_NODE_STREAMEDBUFFERVEC3     NGLI_FOURCC('S','B','f','3')
#define NGL_NODE_STREAMEDBUFFERVEC4     NGLI_FOURCC('S','B','f','4')
#define NGL_NODE_STREAMEDBUFFERMAT4     NGLI_FOURCC('S','B','m','4')
#define NGL_NODE_TEXT                   NGLI_FOURCC('T','e','x','t')
#define NGL_NODE_TEXTURE2D              NGLI_FOURCC('T','e','x','2')
#define NGL_NODE_TEXTURE3D              NGLI_FOURCC('T','e','x','3')
#define NGL_NODE_TEXTURECUBE            NGLI_FOURCC('T','e','x','C')
#define NGL_NODE_TIME                   NGLI_FOURCC('T','i','m','e')
#define NGL_NODE_TIMERANGEFILTER        NGLI_FOURCC('T','R','F','l')
#define NGL_NODE_TIMERANGEMODECONT      NGLI_FOURCC('T','R','M','C')
#define NGL_NODE_TIMERANGEMODENOOP      NGLI_FOURCC('T','R','M','N')
#define NGL_NODE_TIMERANGEMODEONCE      NGLI_FOURCC('T','R','M','1')
#define NGL_NODE_TRANSFORM              NGLI_FOURCC('T','r','f','m')
#define NGL_NODE_TRANSLATE              NGLI_FOURCC('T','m','o','v')
#define NGL_NODE_TRIANGLE               NGLI_FOURCC('T','r','g','l')
#define NGL_NODE_UNIFORMINT             NGLI_FOURCC('U','n','i','1')
#define NGL_NODE_UNIFORMIVEC2           NGLI_FOURCC('U','n','i','2')
#define NGL_NODE_UNIFORMIVEC3           NGLI_FOURCC('U','n','i','3')
#define NGL_NODE_UNIFORMIVEC4           NGLI_FOURCC('U','n','i','4')
#define NGL_NODE_UNIFORMUINT            NGLI_FOURCC('U','n','u','1')
#define NGL_NODE_UNIFORMUIVEC2          NGLI_FOURCC('U','n','u','2')
#define NGL_NODE_UNIFORMUIVEC3          NGLI_FOURCC('U','n','u','3')
#define NGL_NODE_UNIFORMUIVEC4          NGLI_FOURCC('U','n','u','4')
#define NGL_NODE_UNIFORMMAT4            NGLI_FOURCC('U','n','M','4')
#define NGL_NODE_UNIFORMFLOAT           NGLI_FOURCC('U','n','f','1')
#define NGL_NODE_UNIFORMVEC2            NGLI_FOURCC('U','n','f','2')
#define NGL_NODE_UNIFORMVEC3            NGLI_FOURCC('U','n','f','3')
#define NGL_NODE_UNIFORMVEC4            NGLI_FOURCC('U','n','f','4')
#define NGL_NODE_UNIFORMQUAT            NGLI_FOURCC('U','n','Q','t')
#define NGL_NODE_USERSWITCH             NGLI_FOURCC('U','S','c','h')

/**
 * Return error codes.
 *
 * All NGL_ERROR_* values shall remain < 0.
 */
#define NGL_ERROR_GENERIC           -1                                  /* Generic error */
#define NGL_ERROR_ACCESS            -NGLI_FOURCC('E','a','c','c')       /* Operation not allowed */
#define NGL_ERROR_BUG               -NGLI_FOURCC('E','b','u','g')       /* A buggy code path was triggered, please report if it happens */
#define NGL_ERROR_EXTERNAL          -NGLI_FOURCC('E','e','x','t')       /* An error occurred in an external dependency */
#define NGL_ERROR_INVALID_ARG       -NGLI_FOURCC('E','a','r','g')       /* Invalid user argument specified */
#define NGL_ERROR_INVALID_DATA      -NGLI_FOURCC('E','d','a','t')       /* Invalid input data */
#define NGL_ERROR_INVALID_USAGE     -NGLI_FOURCC('E','u','s','g')       /* Invalid public API usage */
#define NGL_ERROR_IO                -NGLI_FOURCC('E','i','o',' ')       /* Input/Output error */
#define NGL_ERROR_LIMIT_EXCEEDED    -NGLI_FOURCC('E','l','i','m')       /* Hardware or resource limit exceeded */
#define NGL_ERROR_MEMORY            -NGLI_FOURCC('E','m','e','m')       /* Memory/allocation error */
#define NGL_ERROR_NOT_FOUND         -NGLI_FOURCC('E','f','n','d')       /* Target not found */
#define NGL_ERROR_UNSUPPORTED       -NGLI_FOURCC('E','s','u','p')       /* Unsupported operation */

/**
 * Allocate a node.
 *
 * This function does not perform any OpenGL operation.
 *
 * The reference counter of the allocated node is set to 1.
 *
 * Must be destroyed using ngl_node_unrefp().
 *
 * @param type  identify the node (any of NGL_NODE_*)
 *
 * @return a new allocated node or NULL on error
 */
struct ngl_node *ngl_node_create(int type);

/**
 * Increment the reference counter of a given node by 1.
 *
 * This function is NOT thread-safe.
 *
 * This function does not perform any OpenGL operation.
 *
 * @param node  pointer to the node to reference count
 *
 * @return node with its reference counter incremented.
 */
struct ngl_node *ngl_node_ref(struct ngl_node *node);

/**
 * Decrement the reference counter of a given node by 1, and destroy its
 * content if the reference counter reaches 0. The passed node pointer will
 * also be set to NULL.
 *
 * @param nodep  pointer to the pointer to the target node
 */
void ngl_node_unrefp(struct ngl_node **nodep);

/**
 * Add entries to a list-based parameter of an allocated node.
 *
 * If the type of the parameter is node based, the reference counter of the
 * passed nodes will be incremented.
 *
 * @param node      pointer to the target node
 * @param key       string identifying the parameter
 * @param nb_elems  number of elements to append
 * @param elems     pointer to an array of values in parameter type
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_node_param_add(struct ngl_node *node, const char *key,
                       int nb_elems, void *elems);

/**
 * Set a parameter value of an allocated node.
 *
 * If the type of the parameter is node based, the reference counter of the
 * passed node will be incremented.
 *
 * @param node      pointer to the target node
 * @param key       string identifying the parameter
 * @param ...       the value in parameter type
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_node_param_set(struct ngl_node *node, const char *key, ...);

/**
 * Serialize in Graphviz format (.dot) a node graph.
 *
 * Must be destroyed using free().
 *
 * @return an allocated string in dot format or NULL on error
 *
 * @see ngl_dot()
 */
char *ngl_node_dot(const struct ngl_node *node);

/**
 * Serialize in node.gl format (.ngl).
 *
 * Must be destroyed using free().
 *
 * @return an allocated string in node.gl format or NULL on error
 */
char *ngl_node_serialize(const struct ngl_node *node);

/**
 * De-serialize a scene.
 *
 * @param s  string in node.gl serialized format.
 *
 * Must be destroyed using ngl_node_unrefp().
 *
 * @return a pointer to the de-serialized node graph or NULL on error
 */
struct ngl_node *ngl_node_deserialize(const char *s);

/**
 * Platform-specific identifiers
 */
enum {
    NGL_PLATFORM_AUTO,
    NGL_PLATFORM_XLIB,
    NGL_PLATFORM_ANDROID,
    NGL_PLATFORM_MACOS,
    NGL_PLATFORM_IOS,
    NGL_PLATFORM_WINDOWS,
    NGL_PLATFORM_WAYLAND,
};

/**
 * Rendering backends
 */
enum {
    NGL_BACKEND_AUTO,
    NGL_BACKEND_OPENGL,
    NGL_BACKEND_OPENGLES,
    NGL_BACKEND_VULKAN,
    NGL_BACKEND_NGFX
};

/**
 * node.gl configuration
 */
struct ngl_config {
    int platform;  /* Platform-specific identifier (any of NGL_PLATFORM_*) */

    int backend;   /* Rendering backend (any of NGL_BACKEND_*) */

    uintptr_t display; /* A native display handle */

    uintptr_t window;  /* A native window handle */

    uintptr_t handle;  /* A native OpenGL context handle */

    int swap_interval; /* Specifies the minimum number of video frames that are
                          displayed before a buffer swap will occur. -1 can be
                          used to use the default system implementation value.
                          This option is only honored on Linux, macOS, and
                          Android (iOS does not provide swap interval control).
                          */

    int offscreen; /* Whether the rendering should happen offscreen or not */

    int width;     /* Graphic context width, mandatory for offscreen rendering */

    int height;    /* Graphic context height, mandatory for offscreen rendering */

    int viewport[4]; /* Viewport defined as x, y, width and height */

    int samples;   /* Number of samples used for multisample anti-aliasing */

    int set_surface_pts; /* Whether pts should be set to the surface or not (Android only).
                            Unsupported with offscreen rendering. */

    float clear_color[4]; /* Clear color (red, green, blue, alpha) */

    uint8_t *capture_buffer; /* RGBA offscreen capture buffer. If allocated,
                                its size must be at least width * height * 4
                                bytes. */
};

/**
 * Opaque structure identifying a node.gl context
 */
struct ngl_ctx;

/**
 * Allocate a new node.gl context.
 *
 * Must be destroyed using ngl_freep().
 *
 * This function does not perform any OpenGL operation.
 *
 * @return a pointer to the context, or NULL on error
 */
struct ngl_ctx *ngl_create(void);

/**
 * Configure the node.gl context.
 *
 * This function must be called before any ngl_draw() call.
 *
 * This function must be called on the UI/main thread on iOS/macOS.
 *
 * If the context has already been configured, calling ngl_configure() will
 * perform a hard-reconfiguration meaning it will de-allocate the ressources of
 * any associated scene, reconfigure the rendering backend and finally
 * re-allocate the ressources of any previously associated scene.
 *
 * @param s        pointer to a node.gl context
 * @param config   pointer to a node.gl configuration structure (cannot be NULL)
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_configure(struct ngl_ctx *s, struct ngl_config *config);

/**
 * Update the swap chain buffers size.
 *
 * This function must be called on the UI/main thread on iOS/macOS.
 *
 * @param width new width of the swap chain buffers
 * @param height new height of the swap chain buffers
 * @param viewport new viewport of the current render target,
 *                 a NULL pointer will make the new viewport match the
 *                 dimensions of the swap chain buffers
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_resize(struct ngl_ctx *s, int width, int height, const int *viewport);

/**
 * Associate a scene with a node.gl context.
 *
 * The reference counter of the root node will be incremented and all its node
 * will be associated with the specified node.gl context.
 *
 * The nodes can be associated with only one node.gl context.
 *
 * If any scene was previously associated with the context, it is detached from
 * it and its reference counter decremented.
 *
 * To only detach the currently associated scene, scene=NULL can be used.
 *
 * @param s      pointer to the configured node.gl context
 * @param scene  pointer to the scene
 *
 * @note node.gl context must to be configured before calling this function.
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_set_scene(struct ngl_ctx *s, struct ngl_node *scene);

/**
 * Draw at the specified time.
 *
 * @param s     pointer to the configured node.gl context
 * @param t     target draw time in seconds
 *
 * @note ngl_draw() will only perform a clear if no scene is set.
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_draw(struct ngl_ctx *s, double t);

/**
 * Serialize the current scene in Graphviz format (.dot) a node graph at the
 * specified time. Non active nodes will be grayed.
 *
 * Must be destroyed using free().
 *
 * @return an allocated string in dot format or NULL on error
 *
 * @see ngl_node_dot()
 */
char *ngl_dot(struct ngl_ctx *s, double t);

/**
 * Destroy a node.gl context. The passed context pointer will also be set to
 * NULL.
 *
 * @param ss    pointer to the pointer to the node.gl context
 */
void ngl_freep(struct ngl_ctx **ss);

/**
 * Evaluate an animation at a given time t.
 *
 * @param anim  the animation node can be any of AnimatedFloat, AnimatedVec2,
 *              AnimatedVec3, or AnimatedVec4
 * @param dst   pointer to the destination for the interpolated value(s), needs
 *              to hold enough space depending on the type of anim
 *              (AnimatedFloat is float[1], AnimatedVec2 is float[2],
 *              AnimatedVec3 is float[3], AnimatedVec4 is float[4],
 *              AnimatedQuat is float[4])
 * @param t     the target time at which to interpolate the value(s)
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_anim_evaluate(struct ngl_node *anim, void *dst, double t);

/**
 * Evaluate an easing at a given time t
 *
 * @param name      the easing name
 * @param args      a list of arguments some easings may use, can be NULL
 * @param nb_args   number of arguments in args
 * @param offsets   starting and ending offset of the truncation of the easing, can be NULL or point to two doubles
 * @param t         the target time
 * @param v         pointer for the resulting value
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_easing_evaluate(const char *name, double *args, int nb_args,
                        double *offsets, double t, double *v);

/**
 * Solve an easing for a given value t
 *
 * @param name      the easing name
 * @param args      a list of arguments some easings may use, can be NULL
 * @param nb_args   number of arguments in args
 * @param offsets   starting and ending offset of the truncation of the easing, can be NULL or point to two doubles
 * @param v         the target value
 * @param t         pointer for the resulting time
 *
 * @note Not all easings have a resolution function available
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_easing_solve(const char *name, double *args, int nb_args,
                     double *offsets, double v, double *t);

/**
 * Android
 */

/**
 * Set a Java virtual machine that will be used to retrieve the JNI
 * environment.
 *
 * @param vm    pointer to the Java virtual machine
 *
 * @return 0 on success, NGL_ERROR_* (< 0) on error
 */
int ngl_jni_set_java_vm(void *vm);

/**
 * Get the Java virtual machine pointer that has been set with
 * ngl_jni_set_java_vm().
 *
 * @return a pointer to the Java virtual machine or NULL if none has been set
 */
void *ngl_jni_get_java_vm(void);

/**
 * Set the Android application context.
 *
 * @param application_context   JNI global reference of the Android application
 *                              context
 */
int ngl_android_set_application_context(void *application_context);

/**
 * Get the Android application context that has been set with
 * ngl_android_set_application_context().
 *
 * @return a pointer to the JNI global reference of the Android application
 *         context or NULL if none has been set
 */
void *ngl_android_get_application_context(void);

#ifdef __cplusplus
}
#endif

#endif
