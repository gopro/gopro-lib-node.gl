#
# Copyright 2020 GoPro Inc.
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

project(
  'ngl-tools',
  'c',
   default_options: ['c_std=c99', 'b_vscrt=md'],
   license: 'Apache',
   meson_version: '>= 0.53.0',
)

host_system = host_machine.system()
cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([
    '-Werror=missing-prototypes',
  ]),
  language: 'c',
)

c_args = []

if cc.get_id() == 'msvc'
  c_args += ['-DSDL_MAIN_HANDLED']
  msvc_args = [
      # Ignore several spurious warnings
      '/wd4018', # comparaison between signed and unsigned numbers
      '/wd4146', # unary minus operator applied to unsigned type when its result is still unsigned (INT_MIN)
      '/wd4244', # conversion from 'type1' to 'type2', resulting in a possible loss of data
      '/wd4305', # value is converted to a smaller type in an initialization, resulting in a loss of information.
      # Enable some warnings to match GCC/Clang
      '/w14062', # enumerator 'identifier' in switch of enum 'enumeration' is not handled
      '/w14101', # 'identifier' : unreferenced local variable
      '/w14189', # 'identifier' : local variable is initialized but not referenced
  ]
  add_project_arguments(msvc_args, language: 'c')
endif


#
# Main dependencies
#
tool_deps = [
  cc.find_library('m', required: false),
  dependency('libnodegl'),
]
sxplayer_dep = dependency('libsxplayer')  # for media probing
threads_dep = dependency('threads', required: false)

python_mod = import('python')
python_ins = python_mod.find_installation('python3')
python_dep = python_ins.dependency(embed: true, required: get_option('python'))


#
# Player based dependencies
#
wsi_src_system = {
  'linux': files('wsi_linux.c'),
  'darwin': files('wsi_cocoa.m'),
  'windows': files('wsi_windows.c'),
}
wsi_src = files('wsi.c') + wsi_src_system.get(host_system, [])
sdl_dep = dependency('sdl2', required: get_option('sdl'))
wsi_deps = [sdl_dep]
if host_system == 'darwin'
  wsi_deps += [dependency('appleframeworks', modules: 'AppKit', required: false)]
endif
if host_system == 'darwin'
  add_languages('objc')
endif


#
# Network dependencies
#
net_deps = []
if host_system == 'windows'
  net_deps += [cc.find_library('ws2_32')]
endif


#
# Tools specifications
#
tools_specs = {
  'ngl-desktop': {
    'src': files('ngl-desktop.c', 'ipc.c', 'player.c', 'opts.c') + wsi_src,
    'deps': net_deps + wsi_deps + [threads_dep],
  },
  'ngl-ipc': {
    'src': files('ngl-ipc.c', 'ipc.c', 'opts.c'),
    'deps': net_deps,
  },
  'ngl-player': {
    'src': files('ngl-player.c', 'player.c', 'opts.c') + wsi_src,
    'deps': wsi_deps + [sxplayer_dep],
  },
  'ngl-probe': {
    'src': files('ngl-probe.c', 'opts.c'),
    'deps': [],
  },
  'ngl-python': {
    'src': files('ngl-python.c', 'player.c', 'python_utils.c', 'opts.c') + wsi_src,
    'deps': wsi_deps + [python_dep],
  },
  'ngl-render': {
    'src': files('ngl-render.c', 'opts.c') + wsi_src,
    'deps': wsi_deps,
  },
  'ngl-serialize': {
    'src': files('ngl-serialize.c', 'python_utils.c'),
    'deps': [python_dep],
  },
}

foreach tool_name, tool_info : tools_specs
  all_dep_found = true
  foreach dep : tool_info.get('deps')
    all_dep_found = all_dep_found and dep.found()
  endforeach

  if all_dep_found
    exe = executable(
      tool_name,
      tool_info.get('src') + files('common.c'),
      dependencies: tool_deps + tool_info.get('deps'),
      install: true,
      install_rpath: get_option('rpath') ? get_option('prefix') / 'lib' : '',
      c_args: c_args,
    )
  endif
endforeach
