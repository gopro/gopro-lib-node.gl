#!/usr/bin/env python
#
# Copyright 2017 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import sys
from xml.etree import ElementTree as ET

cmds_optional = [
    # Framebuffer
    'glBlitFramebuffer',
    'glInvalidateFramebuffer',

    # Renderbuffer
    'glRenderbufferStorageMultisample',

    # Texture
    'glTexImage3D',
    'glTexStorage2D',
    'glTexStorage3D',
    'glTexSubImage3D',
    'glBindImageTexture',

    # Vertex Arrays
    'glBindVertexArray',
    'glDeleteVertexArrays',
    'glGenVertexArrays',

    # Barrier
    'glMemoryBarrier',

    #  Buffers
    'glBindBufferBase',
    'glBindBufferRange',

    # Compute shaders
    'glDispatchCompute',

    # Shaders
    'glGetProgramResourceLocation',
    'glGetProgramResourceIndex',
    'glGetProgramResourceiv',
    'glGetProgramInterfaceiv',
    'glGetProgramResourceName',

    # Polygon
    'glPolygonMode',

    # Internal format
    'glGetInternalformativ',

    # Query
    'glBeginQuery',
    'glEndQuery',
    'glGenQueries',
    'glDeleteQueries',
    'glQueryCounter',
    'glGetQueryObjectui64v',

    # Debug
    'glDebugMessageCallback',

    # Query EXT
    'glBeginQueryEXT',
    'glEndQueryEXT',
    'glGenQueriesEXT',
    'glDeleteQueriesEXT',
    'glQueryCounterEXT',
    'glGetQueryObjectui64vEXT',

    # Instancing
    'glDrawArraysInstanced',
    'glDrawElementsInstanced',
    'glVertexAttribDivisor',

    # Uniform Block Object
    'glGetUniformBlockIndex',
    'glUniformBlockBinding',
    'glGetActiveUniformBlockName',
    'glGetActiveUniformBlockiv',

    # EGL OES image
    'glEGLImageTargetTexture2DOES',

    # Sync object
    'glFenceSync',
    'glWaitSync',
    'glClientWaitSync',

    # Read/Draw Buffer
    'glReadBuffer',
    'glDrawBuffer',
    'glDrawBuffers',

    # Clear Buffer
    'glClearBufferfv',
    'glClearBufferfi',

    # UInt uniforms
    'glUniform1uiv',
    'glUniform2uiv',
    'glUniform3uiv',
    'glUniform4uiv',
]

cmds = [
    # Enable
    'glEnable',
    'glDisable',

    # Error
    'glGetError',

    # Get
    'glGetBooleanv',
    'glGetIntegeri_v',
    'glGetIntegerv',
    'glGetString',
    'glGetStringi',

    # Viewport
    'glViewport',
    'glScissor',

    # Color
    'glColorMask',

    # Depth
    'glDepthFunc',
    'glDepthMask',

    # Clear
    'glClear',
    'glClearColor',

    # Blending
    'glBlendColor',
    'glBlendEquation',
    'glBlendEquationSeparate',
    'glBlendFunc',
    'glBlendFuncSeparate',

    # Draw
    'glDrawArrays',
    'glDrawElements',

    # Texture
    'glActiveTexture',
    'glBindTexture',
    'glDeleteTextures',
    'glGenTextures',
    'glGenerateMipmap',
    'glPixelStorei',
    'glTexImage2D',
    'glTexParameteri',
    'glTexSubImage2D',

    # Framebuffer
    'glCheckFramebufferStatus',
    'glBindFramebuffer',
    'glDeleteFramebuffers',
    'glFramebufferRenderbuffer',
    'glFramebufferTexture2D',
    'glGenFramebuffers',
    'glReadPixels',

    # Buffer
    'glBindBuffer',
    'glBufferData',
    'glBufferSubData',
    'glDeleteBuffers',
    'glGenBuffers',

    # Render buffer
    'glBindRenderbuffer',
    'glDeleteRenderbuffers',
    'glGenRenderbuffers',
    'glGetRenderbufferParameteriv',
    'glRenderbufferStorage',

    # Shader
    'glCreateProgram',
    'glCreateShader',
    'glAttachShader',
    'glCompileShader',
    'glDeleteProgram',
    'glDeleteShader',
    'glDetachShader',
    'glGetAttachedShaders',
    'glGetProgramInfoLog',
    'glGetProgramiv',
    'glGetShaderInfoLog',
    'glGetShaderSource',
    'glGetShaderiv',
    'glLinkProgram',
    'glReleaseShaderCompiler',
    'glShaderBinary',
    'glShaderSource',
    'glUseProgram',

    # Shader Attributes
    'glGetAttribLocation',
    'glBindAttribLocation',
    'glEnableVertexAttribArray',
    'glDisableVertexAttribArray',
    'glVertexAttribPointer',
    'glGetActiveAttrib',
    'glGetActiveUniform',

    # Shader Uniforms
    'glGetUniformLocation',
    'glGetUniformiv',
    'glUniform1fv',
    'glUniform1i',
    'glUniform1iv',
    'glUniform2fv',
    'glUniform2iv',
    'glUniform3fv',
    'glUniform3iv',
    'glUniform4fv',
    'glUniform4iv',
    'glUniformMatrix2fv',
    'glUniformMatrix3fv',
    'glUniformMatrix4fv',

    # Stencil
    'glStencilFunc',
    'glStencilFuncSeparate',
    'glStencilMask',
    'glStencilMaskSeparate',
    'glStencilOp',
    'glStencilOpSeparate',

    # Face Culling
    'glCullFace',

    # Sync
    'glFlush',
    'glFinish',

] + cmds_optional

def get_proto_elems(xml_node):
    elems = []
    for text in xml_node.itertext():
        text = text.strip()
        if not text:
            continue
        elems.append(text)
    return elems

def gen(gl_xml, func_file, def_file, wrap_file):

    do_not_edit = '/* DO NOT EDIT - This file is autogenerated */\n'

    glwrappers = do_not_edit + '''
/* WARNING: this file must only be included once */

#include "config.h"

#if DEBUG_GL
# define check_error_code ngli_glcontext_check_gl_error
#else
# define check_error_code(gl, glfuncname) do { } while (0)
#endif
'''

    glfunctions = do_not_edit + '''
#ifndef NGL_GLFUNCS_H
#define NGL_GLFUNCS_H

#include "glincludes.h"

struct glfunctions {
'''

    gldefinitions = do_not_edit + '''
/* WARNING: this file must only be included once */

#include <stddef.h>

#include "glcontext.h"

#define M (1 << 0)

static const struct gldefinition {
    const char *name;
    size_t offset;
    int flags;
} gldefinitions[] = {
'''

    xml = ET.parse(gl_xml)
    root = xml.getroot()
    commands = root.find('commands')
    for cmd in commands:
        proto = cmd.find('proto')
        funcname = proto.find('name').text

        if funcname not in cmds:
            continue

        ptype = proto.find('ptype')
        funcret = ' '.join(get_proto_elems(proto)[:-1])

        ret_assign = ''
        ret_call = ''
        if funcret != 'void':
            ret_assign = '%s ret = ' % funcret
            ret_call = '    return ret;\n'

        func_args_specs = []
        func_args = []
        for param in cmd.findall('param'):
            func_args_specs.append(' '.join(get_proto_elems(param)))
            func_args.append(param.find('name').text)

        wrapper_args_specs = ['const struct glcontext *gl'] + func_args_specs

        data = {
                'func_ret': funcret,
                'func_name': funcname,
                'func_name_nogl': funcname[2:], # with "gl" stripped
                'wrapper_args_specs': ', '.join(wrapper_args_specs),
                'func_args_specs': ', '.join(func_args_specs),
                'ret_assign': ret_assign,
                'func_args': ', '.join(func_args),
                'ret_call': ret_call,
                'flags': '0' if funcname in cmds_optional else 'M',
        }

        glfunctions   += '    %(func_ret)s (NGLI_GL_APIENTRY *%(func_name_nogl)s)(%(func_args_specs)s);\n' % data
        gldefinitions += '    {"%(func_name)s", offsetof(struct glfunctions, %(func_name_nogl)s), %(flags)s},\n' % data
        if funcname == 'glGetError':
            glwrappers += '''
static inline GLenum ngli_glGetError(const struct glcontext *gl)
{
    return gl->funcs.GetError();
}
'''
        else:
            glwrappers    += '''
static inline %(func_ret)s ngli_%(func_name)s(%(wrapper_args_specs)s)
{
    %(ret_assign)sgl->funcs.%(func_name_nogl)s(%(func_args)s);
    check_error_code(gl, "%(func_name)s");
%(ret_call)s}
''' % data

        cmds.pop(cmds.index(funcname))
        if not cmds:
            break

    if cmds:
        print('WARNING: function(s) not found: ' + ', '.join(cmds))

    glfunctions   += '};\n\n#endif\n'
    gldefinitions += '};\n'

    with open(func_file, 'w', newline='\n') as f:
        f.write(glfunctions)
    with open(def_file, 'w', newline='\n') as f:
        f.write(gldefinitions)
    with open(wrap_file, 'w', newline='\n') as f:
        f.write(glwrappers)

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print('Usage: %s <gl.xml> <func.h> <def.h> <wrap.h>' % sys.argv[0])
        sys.exit(0)
    gen(*sys.argv[1:])
