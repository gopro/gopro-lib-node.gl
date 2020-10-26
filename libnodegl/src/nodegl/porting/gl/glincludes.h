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

#ifndef GLINCLUDES_H
#define GLINCLUDES_H

#if __APPLE__
# include <TargetConditionals.h>
# if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
#  define NGL_GLES2_COMPAT_INCLUDES 1
#  define NGL_CS_COMPAT_INCLUDES 1
# elif TARGET_OS_MAC
#  include <OpenGL/gl3.h>
#  include <OpenGL/glext.h>
#  define NGL_CS_COMPAT_INCLUDES 1
#  define NGL_OGL3_COMPAT_INCLUDES 1
# endif
#endif

#if __ANDROID__
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>
# define NGL_GLES2_COMPAT_INCLUDES 1
# define NGL_CS_COMPAT_INCLUDES 1
# define GL_BGRA GL_BGRA_EXT
#endif

#if __linux__ && !__ANDROID__
# define GL_GLEXT_PROTOTYPES
# include <GL/glcorearb.h>
# include <GL/glext.h>
# define NGL_OGL3_COMPAT_INCLUDES 1
#endif

#if _WIN32
#include <windows.h>
#include <GL/glew.h>
#include <GL/gl.h>
//#include <GL/glcorearb.h>
//#include <GL/glext.h>
# define NGL_OGL3_COMPAT_INCLUDES 1
#endif

#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

#if NGL_OGL3_COMPAT_INCLUDES
# define GL_LUMINANCE                          0x1909
# define GL_LUMINANCE_ALPHA                    0x190A
# define GL_TEXTURE_EXTERNAL_OES               0x8D65
# define GL_SAMPLER_EXTERNAL_OES               0x8D66
# define GL_TEXTURE_BINDING_EXTERNAL_OES       0x8D67
# define GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT        0x8BE7
#endif

#if NGL_GLES2_COMPAT_INCLUDES
# define GL_UNPACK_ROW_LENGTH                  0x0CF2
# define GL_MAX_DRAW_BUFFERS                   0x8824
# define GL_MAX_SAMPLES                        0x8D57
# define GL_MAX_COLOR_ATTACHMENTS              0x8CDF
# define GL_SYNC_GPU_COMMANDS_COMPLETE         0x9117
# define GL_TIMEOUT_IGNORED                    0xFFFFFFFFFFFFFFFFull
# define GL_TEXTURE_RECTANGLE                  0x84F5
# define GL_STENCIL_INDEX                      0x1901
# define GL_STENCIL_INDEX8                     0x8D48
# define GL_UNSIGNED_INT_VEC2                  0x8DC6
# define GL_UNSIGNED_INT_VEC3                  0x8DC7
# define GL_UNSIGNED_INT_VEC4                  0x8DC8
# define GL_SAMPLER_2D_RECT                    0x8B63
# define GL_SAMPLER_3D                         0x8B5F
# define GL_SAMPLER_CUBE                       0x8B60
# define GL_TEXTURE_EXTERNAL_OES               0x8D65
# define GL_SAMPLER_EXTERNAL_OES               0x8D66
# define GL_TEXTURE_BINDING_EXTERNAL_OES       0x8D67
# define GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT        0x8BE7
# define GL_MAJOR_VERSION                      0x821B
# define GL_MINOR_VERSION                      0x821C
# define GL_NUM_EXTENSIONS                     0x821D
# define GL_HALF_FLOAT                         0x140B
# define GL_RED                                0x1903
# define GL_RED_INTEGER                        0x8D94
# define GL_RG                                 0x8227
# define GL_RG_INTEGER                         0x8228
# define GL_RGB_INTEGER                        0x8D98
# define GL_RGBA_INTEGER                       0x8D99
# define GL_R8                                 0x8229
# define GL_R16                                0x822A
# define GL_RG8                                0x822B
# define GL_RG16                               0x822C
# define GL_R16F                               0x822D
# define GL_R32F                               0x822E
# define GL_RG16F                              0x822F
# define GL_RG32F                              0x8230
# define GL_R8I                                0x8231
# define GL_R8UI                               0x8232
# define GL_R16I                               0x8233
# define GL_R16UI                              0x8234
# define GL_R32I                               0x8235
# define GL_R32UI                              0x8236
# define GL_RG8I                               0x8237
# define GL_RG8UI                              0x8238
# define GL_RG16I                              0x8239
# define GL_RG16UI                             0x823A
# define GL_RG32I                              0x823B
# define GL_RG32UI                             0x823C
# define GL_RGB8                               0x8051
# define GL_RGB16                              0x8054
# define GL_RGBA8                              0x8058
# define GL_RGBA16                             0x805B
# define GL_RGBA32UI                           0x8D70
# define GL_RGB32UI                            0x8D71
# define GL_RGBA16UI                           0x8D76
# define GL_RGB16UI                            0x8D77
# define GL_RGBA8UI                            0x8D7C
# define GL_RGB8UI                             0x8D7D
# define GL_RGBA32I                            0x8D82
# define GL_RGB32I                             0x8D83
# define GL_RGBA16I                            0x8D88
# define GL_RGB16I                             0x8D89
# define GL_RGBA8I                             0x8D8E
# define GL_RGB8I                              0x8D8F
# define GL_R8_SNORM                           0x8F94
# define GL_RG8_SNORM                          0x8F95
# define GL_RGB8_SNORM                         0x8F96
# define GL_RGBA8_SNORM                        0x8F97
# define GL_RGBA32F                            0x8814
# define GL_RGB32F                             0x8815
# define GL_RGBA16F                            0x881A
# define GL_SRGB8                              0x8C41
# define GL_SRGB8_ALPHA8                       0x8C43
# define GL_BGRA_INTEGER                       0x8D9B
# define GL_R16_SNORM                          0x8F98
# define GL_RG16_SNORM                         0x8F99
# define GL_RGB16_SNORM                        0x8F9A
# define GL_RGBA16_SNORM                       0x8F9B
# define GL_RGB16F                             0x881B
# define GL_QUERY_RESULT                       0x8866
# define GL_QUERY_RESULT_AVAILABLE             0x8867
# define GL_DEPTH_COMPONENT24                  0x81A6
# define GL_DEPTH_COMPONENT32F                 0x8CAC
# define GL_DEPTH32F_STENCIL8                  0x8CAD
# define GL_FLOAT_32_UNSIGNED_INT_24_8_REV     0x8DAD
# define GL_DEPTH_STENCIL                      0x84F9
# define GL_UNSIGNED_INT_24_8                  0x84FA
# define GL_DEPTH24_STENCIL8                   0x88F0
# define GL_READ_ONLY                          0x88B8
# define GL_WRITE_ONLY                         0x88B9
# define GL_READ_WRITE                         0x88BA
# define GL_TIME_ELAPSED                       0x88BF
# define GL_STREAM_READ                        0x88E1
# define GL_STREAM_COPY                        0x88E2
# define GL_STATIC_READ                        0x88E5
# define GL_STATIC_COPY                        0x88E6
# define GL_DYNAMIC_READ                       0x88E9
# define GL_DYNAMIC_COPY                       0x88EA
# define GL_INVALID_INDEX                      0xFFFFFFFFU
# define GL_POLYGON_MODE                       0x0B40
# define GL_FILL                               0x1B02
# define GL_TEXTURE_3D                         0x806F
# define GL_TEXTURE_WRAP_R                     0x8072
# define GL_MIN                                0x8007
# define GL_MAX                                0x8008
# define GL_DRAW_FRAMEBUFFER_BINDING           0x8CA6
# define GL_READ_FRAMEBUFFER                   0x8CA8
# define GL_DRAW_FRAMEBUFFER                   0x8CA9
# define GL_READ_FRAMEBUFFER_BINDING           0x8CAA
# define GL_DEPTH_STENCIL_ATTACHMENT           0x821A
# define GL_ACTIVE_RESOURCES                   0x92F5
# define GL_ACTIVE_UNIFORM_BLOCKS              0x8A36
# define GL_UNIFORM_BUFFER                     0x8A11
# define GL_UNIFORM_BLOCK_BINDING              0x8A3F
# define GL_MAX_UNIFORM_BLOCK_SIZE             0x8A30
# define GL_TEXTURE_CUBE_MAP                   0x8513
# define GL_TEXTURE_BINDING_CUBE_MAP           0x8514
# define GL_TEXTURE_CUBE_MAP_POSITIVE_X        0x8515
# define GL_TEXTURE_CUBE_MAP_NEGATIVE_X        0x8516
# define GL_TEXTURE_CUBE_MAP_POSITIVE_Y        0x8517
# define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y        0x8518
# define GL_TEXTURE_CUBE_MAP_POSITIVE_Z        0x8519
# define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z        0x851A
# define GL_TEXTURE_CUBE_MAP_SEAMLESS          0x884F
#endif

#if NGL_CS_COMPAT_INCLUDES
# define GL_COMPUTE_SHADER                     0x91B9
# define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS 0x90EB
# define GL_MAX_COMPUTE_WORK_GROUP_COUNT       0x91BE
# define GL_MAX_COMPUTE_WORK_GROUP_SIZE        0x91BF
# define GL_COMPUTE_WORK_GROUP_SIZE            0x8267
# define GL_SHADER_STORAGE_BUFFER              0x90D2
# define GL_SHADER_STORAGE_BUFFER_BINDING      0x90D3
# define GL_SHADER_STORAGE_BUFFER_START        0x90D4
# define GL_SHADER_STORAGE_BUFFER_SIZE         0x90D5
# define GL_SHADER_STORAGE_BLOCK               0x92E6
# define GL_BUFFER_BINDING                     0x9302
# define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT    0x00000001
# define GL_ELEMENT_ARRAY_BARRIER_BIT          0x00000002
# define GL_UNIFORM_BARRIER_BIT                0x00000004
# define GL_TEXTURE_FETCH_BARRIER_BIT          0x00000008
# define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT    0x00000020
# define GL_COMMAND_BARRIER_BIT                0x00000040
# define GL_PIXEL_BUFFER_BARRIER_BIT           0x00000080
# define GL_TEXTURE_UPDATE_BARRIER_BIT         0x00000100
# define GL_BUFFER_UPDATE_BARRIER_BIT          0x00000200
# define GL_FRAMEBUFFER_BARRIER_BIT            0x00000400
# define GL_TRANSFORM_FEEDBACK_BARRIER_BIT     0x00000800
# define GL_ATOMIC_COUNTER_BARRIER_BIT         0x00001000
# define GL_ALL_BARRIER_BITS                   0xFFFFFFFF
# define GL_IMAGE_2D                           0x904D
# define GL_ACTIVE_RESOURCES                   0x92F5
#endif

#endif /* GLINCLUDES_H */
