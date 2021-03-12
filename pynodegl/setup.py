#!/usr/bin/env python
#
# Copyright 2016 GoPro Inc.
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

from setuptools import setup, Command, Extension
from setuptools.command.build_ext import build_ext
import os


class LibNodeGLConfig:

    PKG_LIB_NAME = 'libnodegl'

    pkg_config_env = os.getenv('PKG_CONFIG')
    def __init__(self, pkg_config_bin= pkg_config_env if pkg_config_env else 'pkg-config'):
        import subprocess

        if subprocess.call([pkg_config_bin, '--exists', self.PKG_LIB_NAME]) != 0:
            raise Exception(f'{self.PKG_LIB_NAME} is required to build pynodegl')

        self.version       = subprocess.check_output([pkg_config_bin, '--modversion', self.PKG_LIB_NAME]).strip().decode()
        self.data_root_dir = subprocess.check_output([pkg_config_bin, '--variable=datarootdir', self.PKG_LIB_NAME]).strip().decode()
        pkgcfg_libs_cflags = subprocess.check_output([pkg_config_bin, '--libs', '--cflags', self.PKG_LIB_NAME]).decode()

        flags = pkgcfg_libs_cflags.split()
        self.include_dirs = [f[2:] for f in flags if f.startswith('-I')]
        self.library_dirs = [f[2:] for f in flags if f.startswith('-L')]
        self.libraries    = [f[2:] for f in flags if f.startswith('-l')]


_LIB_CFG = LibNodeGLConfig()


class CommandUtils:

    @staticmethod
    def _gen_definitions_pyx(specs):
        import yaml  # must NOT be on top of this file

        with open(specs) as f:
            specs = yaml.safe_load(f)

        def _get_vec_init_code(vectype, vecname):
            cvecname = f'{vecname}_c'
            if vectype.startswith('ivec'):
                n = int(vectype[4:])
                ctype = 'int'
            elif vectype.startswith('uivec'):
                n = int(vectype[5:])
                ctype = 'unsigned'
            elif vectype.startswith('vec'):
                n = int(vectype[3:])
                ctype = 'float'
            else:
                assert vectype == 'mat4'
                n = 16
                ctype = 'float'
            return cvecname, f'''
        cdef {ctype}[{n}] {cvecname}
        cdef int {vecname}_i
        if len({vecname}) != {n}:
            raise TypeError("%s parameter is expected to be vec%d but got %d values" % (
                            "{vecname}", {n}, len({vecname})))
        for {vecname}_i in range({n}):
            {cvecname}[{vecname}_i] = {vecname}[{vecname}_i]
'''

        content = 'from libc.stdlib cimport free\n'
        content += 'from libc.stdint cimport uintptr_t\n'
        content += 'from cpython cimport array\n'

        # Map C nodes identifiers (NGL_NODE_*)
        content += 'cdef extern from "nodegl.h":\n'
        nodes_decls = []
        for item in specs:
            node = list(item.keys())[0]
            if not node.startswith('_'):
                nodes_decls.append(f'cdef int NGL_NODE_{node.upper()}')
        nodes_decls.append(None)
        content += '\n'.join((f'    {d}') if d else '' for d in nodes_decls) + '\n'

        for item in specs:
            node = list(item.keys())[0]
            fields = item[node]

            type_id = f'NGL_NODE_{node.upper()}'
            construct_args = ['self']
            special_inits = ''
            extra_args = ''

            # Grab inheritance and fields information about the node
            if not fields:
                fields = []
            if isinstance(fields, str):
                parent_node = fields
                fields = []
            else:
                parent_node = '_Node'

            # For every arguments user-specified (not None), we will
            # call the corresponding set/add/update method.
            optional_args = ['self']
            optional_varnames = []
            for field in fields:
                field_name, field_type = field
                construct_args.append(f'{field_name}=None')
                optional_args.append(f'{field_name}=None')
                optional_varnames.append(field_name)
                is_list = field_type.endswith('List')
                is_dict = field_type.endswith('Dict')
                extra_args += f'''
        if {field_name} is not None:'''
                if is_list:
                    extra_args += f'''
            self.add_{field_name}(*{field_name})'''
                elif is_dict:
                    extra_args += f'''
            self.update_{field_name}({field_name})'''
                else:
                    dereference = 'vec' in field_type or field_type == 'mat4'
                    arg = '*' + field_name if dereference else field_name
                    extra_args += f'''
            self.set_{field_name}({arg})'''

            # Until the end of the inheritance node tree (the _Node), there is
            # no need to forward remaining unrecognized parameters.
            if node != '_Node':
                va_args = ('*args', '**kwargs')
                construct_args += va_args
                optional_args += va_args
                optional_varnames += va_args

            construct_args_str = ', '.join(construct_args)
            optional_args_str = ', '.join(optional_args)
            optional_varnames_str = ', '.join(optional_varnames)

            if node == '_Node':
                class_str = f'''
cdef class _Node:
    cdef ngl_node *ctx

    @property
    def cptr(self):
        return <uintptr_t>self.ctx

    def serialize(self):
        return _ret_pystr(ngl_node_serialize(self.ctx))

    def dot(self):
        return _ret_pystr(ngl_node_dot(self.ctx))

    def __dealloc__(self):
        ngl_node_unrefp(&self.ctx)

    def _update_dict(self, field_name, arg=None, **kwargs):
        cdef ngl_node *node
        data_dict = {{}}
        if arg is not None:
            if not isinstance(arg, dict):
                raise TypeError("%s must be of type dict" % field_name)
            data_dict.update(arg)
        data_dict.update(**kwargs)
        for key, val in data_dict.items():
            if not isinstance(key, str) or (val is not None and not isinstance(val, _Node)):
                raise TypeError("update_%s() takes a dictionary of <string, Node>" % field_name)
            node = (<_Node>val).ctx if val is not None else NULL
            ret = ngl_node_param_set(self.ctx, field_name, <const char *>key, node)
            if ret < 0:
                return ret
        return 0

    def _init_params({optional_args_str}):
{extra_args}
'''

                # Declare growing list helpers functions to _Node, to be used
                # by other nodes for their specific list-based parameters.
                for field_type in 'NodeList', 'doubleList':
                    base_field_type = field_type[:-len('List')]
                    if base_field_type == 'Node':
                        cfield = '(<_Node>item).ctx'
                        citem_type = 'ngl_node *'
                    else:
                        cfield = f'<{base_field_type}>item'
                        citem_type = base_field_type
                    class_str += f'''
    def _add_{field_type.lower()}(self, field_name, *elems):
        if hasattr(elems[0], '__iter__'):
            raise Exception("add_%s() takes elements as "
                            "positional arguments, not list" %
                            field_name)
        cdef int nb_elems = len(elems)
        elems_c = <{citem_type}*>calloc(len(elems), sizeof({citem_type}))
        if elems_c is NULL:
            raise MemoryError()
        cdef int i
        for i, item in enumerate(elems):
            elems_c[i] = {cfield}
        ret = ngl_node_param_add(self.ctx, field_name, nb_elems, elems_c)
        free(elems_c)
        return ret
'''
            else:
                class_str = f'''
cdef class {node}({parent_node}):
'''

                # This case is for nodes such as Buffer* or AnimatedBuffer* that
                # share a common set of parameters (respectively defined in _Buffer
                # and _AnimatedBuffer). These classes will inherit all the
                # parameters and their initializers from their parent class but
                # still need to be identified individually by instantiating a
                # specific C node (with ngl_node_create()).
                if parent_node != '_Node':
                    class_str += f'''
    def __init__({construct_args_str}):{special_inits}
        _set_node_ctx(self, {type_id})
        self._init_params({optional_varnames_str})
'''

                # Nodes starting with a _ (such as _Buffer or _AnimatedBuffer) are
                # intermediate fake nodes sharing the common set of parameters of
                # their children. These nodes do not instantiate a C node, they are
                # only meant to prevent duplication of all the parameter functions
                # for their children. The non-handled parameters provided in their
                # parameter init function need to be forwarded to their parent
                # (_Node).
                elif node.startswith('_'):
                    class_str += f'''
    def _init_params({optional_args_str}):{extra_args}
        {parent_node}._init_params(self, *args, **kwargs)
'''

                # Case for all the remaining nodes The __init__ function includes
                # argument pre-processing for standard C node instantiation, the C
                # node instantiation itself, and the forward of unhandled
                # parameters to the parent (_Node).
                else:
                    class_str += f'''
    def __init__({construct_args_str}):{special_inits}
        _set_node_ctx(self, {type_id})
        {parent_node}._init_params(self, *args, **kwargs)
{extra_args}
'''

            # Animated classes need a specific evaluate method that could not
            # be created through the parameters system.
            animated_nodes = dict(
                AnimatedFloat=1,
                AnimatedVec2=2,
                AnimatedVec3=3,
                AnimatedVec4=4,
                AnimatedQuat=4,
                VelocityFloat=1,
                VelocityVec2=2,
                VelocityVec3=3,
                VelocityVec4=4,
            )
            n = animated_nodes.get(node)
            if n:
                if n == 1:
                    retstr = 'vec[0]'
                else:
                    retstr = '({})'.format(', '.join(f'vec[{x}]' for x in range(n)))
                class_str += f'''
    def evaluate(self, t):
        cdef float[{n}] vec
        ngl_anim_evaluate(self.ctx, vec, t)
        return {retstr}
'''

            # Declare a set, add or update method for every optional field of
            # the node.
            for field in fields:
                field_name, field_type = field

                # Add method
                if field_type.endswith('List'):
                    field_name, field_type = field
                    class_str += f'''
    def add_{field_name}(self, *{field_name}):
        return self._add_{field_type.lower()}("{field_name}", *{field_name})
'''

                # Update method
                elif field_type.endswith('Dict'):
                    field_type = field_type[:-len('Dict')]
                    assert field_type == 'Node'
                    class_str += f'''
    def update_{field_name}(self, arg=None, **kwargs):
        return self._update_dict("{field_name}", arg, **kwargs)
'''

                # Set method for vectors and matrices
                elif 'vec' in field_type or field_type == 'mat4':
                    cparam, vec_init_code = _get_vec_init_code(field_type, field_name)
                    class_str += f'''
    def set_{field_name}(self, *{field_name}):{vec_init_code}
        return ngl_node_param_set(self.ctx, "{field_name}", {cparam})
'''

                # Set method for data
                elif field_type == 'data':
                    class_str += f'''
    def set_{field_name}(self, array.array {field_name}):
        return ngl_node_param_set(self.ctx,
                                  "{field_name}",
                                  <int>({field_name}.buffer_info()[1] * {field_name}.itemsize),
                                  <void *>({field_name}.data.as_voidptr))

'''

                # Set method for rationals
                elif field_type == 'rational':
                    class_str += f'''
    def set_{field_name}(self, tuple {field_name}):
        return ngl_node_param_set(self.ctx,
                                  "{field_name}",
                                  <int>{field_name}[0],
                                  <int>{field_name}[1]);
'''

                # Set method
                else:
                    ctype = field_type
                    cparam = field_name
                    if field_type in ('select', 'flags', 'string'):
                        ctype = 'const char *'
                    elif field_type == 'Node':
                        ctype = '_Node'
                        cparam += '.ctx'
                    elif field_type == 'bool':
                        ctype = 'bint'
                    elif field_type == 'uint':
                        ctype = 'unsigned'
                    class_str += f'''
    def set_{field_name}(self, {ctype} {field_name}):
        return ngl_node_param_set(self.ctx, "{field_name}", {cparam})
'''

            content += class_str + '\n'

        return content

    @staticmethod
    def write_definitions_pyx():
        import os.path as op
        specs_file = op.join(_LIB_CFG.data_root_dir, 'nodegl', 'nodes.specs')
        content = CommandUtils._gen_definitions_pyx(specs_file)
        with open('nodes_def.pyx', 'w') as output:
            output.write(content)


class BuildExtCommand(build_ext):

    def run(self):
        CommandUtils.write_definitions_pyx()
        build_ext.run(self)


class BuildSrcCommmand(Command):
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        from Cython.Compiler.Main import compile
        CommandUtils.write_definitions_pyx()
        compile('pynodegl.pyx', output_file='pynodegl.c')


setup(
    name='pynodegl',
    version=_LIB_CFG.version,
    setup_requires=[
        'setuptools>=18.0',
        'cython>=0.29.6',
        'pyyaml',
    ],
    cmdclass={
        'build_ext': BuildExtCommand,
        'build_src': BuildSrcCommmand,
    },
    ext_modules=[Extension("pynodegl",
                           sources=['pynodegl.pyx'],
                           include_dirs=_LIB_CFG.include_dirs,
                           libraries=_LIB_CFG.libraries,
                           library_dirs=_LIB_CFG.library_dirs)]
)
