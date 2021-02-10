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
  'libnodegl',
  'c',
   default_options: ['c_std=c99'],
   license: 'Apache',
   meson_version: '>= 0.54.0',
   version: '0.0.0',
)

conf_data = configuration_data()
version_array = meson.project_version().split('.')
conf_data.set('version_major', version_array[0])
conf_data.set('version_minor', version_array[1])
conf_data.set('version_micro', version_array[2])

host_system = host_machine.system()
cpu_family = host_machine.cpu_family()
cc = meson.get_compiler('c')

# meson currently (0.56.0) doesn't distinguish between macOS and iOS (the
# "darwin" identifier is shared), so we need to test explicitely.
# We are also checking for IPHONE and not IOS but since the latter is actually
# more specific (doesn't include tvOS and watchOS) we may want to consider
# switching to it at some point.
# See https://github.com/mesonbuild/meson/issues/7944 for more information
iphone_check = '''#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#error not iphone
#endif'''
if host_system == 'darwin' and cc.compiles(iphone_check, name: 'iPhone target')
  host_system = 'iphone'
endif

install_rpath = get_option('rpath') ? get_option('prefix') / get_option('libdir') : ''

debug_opts = get_option('debug_opts')
conf_data.set10('TARGET_' + host_system.to_upper(), true)
conf_data.set10('ARCH_' + cpu_family.to_upper(), true)
conf_data.set10('CONFIG_SMALL', get_option('small'))
conf_data.set10('DEBUG_GL', 'gl' in debug_opts)
conf_data.set10('DEBUG_MEM', 'mem' in debug_opts)
conf_data.set10('DEBUG_SCENE', 'scene' in debug_opts)

if host_system == 'windows'
  if cc.get_id() == 'msvc'
    conf_data.set10('TARGET_MSVC', true)
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
  else
    conf_data.set10('TARGET_MINGW_W64', true)
  endif
endif

# This trim prefix is used to make __FILE__ starts from the source dir with
# out-of-tree builds.
# Adjusted from https://github.com/mesonbuild/meson/issues/7485
trim_prefix = run_command([
  find_program('python'), '-c', 'import sys,os;print(os.path.relpath(*sys.argv[1:3]))',
  meson.current_source_dir(),
  meson.build_root(),
]).stdout().strip()

project_args = cc.get_supported_arguments([
  '-Werror=missing-prototypes',
  '-fmacro-prefix-map=@0@/='.format(trim_prefix),
])
add_project_arguments(project_args, language: 'c')

# Also apply to local generator targets such as gen_doc
# See https://github.com/mesonbuild/meson/issues/7940
add_project_arguments(project_args, language: 'c', native: true)


#
# Library main configuration
#

lib_version = '0.0.0'
lib_src = files(
  'animation.c',
  'api.c',
  'block.c',
  'bstr.c',
  'buffer.c',
  'colorconv.c',
  'darray.c',
  'deserialize.c',
  'dot.c',
  'drawutils.c',
  'format.c',
  'gctx.c',
  'hmap.c',
  'hud.c',
  'hwconv.c',
  'hwupload.c',
  'hwupload_common.c',
  'image.c',
  'log.c',
  'math_utils.c',
  'memory.c',
  'node_animatedbuffer.c',
  'node_animated.c',
  'node_animkeyframe.c',
  'node_block.c',
  'node_buffer.c',
  'node_camera.c',
  'node_circle.c',
  'node_compute.c',
  'node_computeprogram.c',
  'node_geometry.c',
  'node_graphicconfig.c',
  'node_group.c',
  'node_identity.c',
  'node_io.c',
  'node_media.c',
  'node_program.c',
  'node_quad.c',
  'node_render.c',
  'node_resourceprops.c',
  'node_rotate.c',
  'node_rotatequat.c',
  'node_rtt.c',
  'node_scale.c',
  'node_skew.c',
  'node_streamed.c',
  'node_streamedbuffer.c',
  'node_text.c',
  'node_texture.c',
  'node_time.c',
  'node_timerangefilter.c',
  'node_timerangemodes.c',
  'node_transform.c',
  'node_translate.c',
  'node_triangle.c',
  'node_uniform.c',
  'node_userswitch.c',
  'nodes.c',
  'params.c',
  'pass.c',
  'pgcache.c',
  'pgcraft.c',
  'pipeline.c',
  'precision.c',
  'program.c',
  'rendertarget.c',
  'rnode.c',
  'serialize.c',
  'texture.c',
  'transforms.c',
  'utils.c',
)

if host_machine.cpu_family() == 'aarch64'
  lib_src += files('asm_aarch64.S')
endif

hosts_cfg = {
  'linux': {
    'deps': ['x11'],
  },
  'darwin': {
    'frameworks': ['CoreVideo', 'CoreFoundation', 'AppKit', 'IOSurface'],
  },
  'android': {
    'deps': ['libavcodec'],
    'libs': ['android'],
    'src': files(
      'jni_utils.c',
      'android_ctx.c',
      'android_utils.c',
      'android_looper.c',
      'android_surface.c',
      'android_handler.c',
      'android_handlerthread.c',
      'android_imagereader.c',
    ),
  },
  'iphone': {
    'frameworks': ['CoreMedia'],
  },
}

lib_deps = [
  cc.find_library('m', required: false),
  dependency('libsxplayer', version: '>= 9.7.0'),
  dependency('threads'),
]

host_cfg = hosts_cfg.get(host_system, {})
foreach dep : host_cfg.get('deps', [])
  lib_deps += dependency(dep)
endforeach
foreach dep : host_cfg.get('libs', [])
  lib_deps += cc.find_library(dep)
endforeach
if 'frameworks' in host_cfg
  lib_deps += dependency('appleframeworks', modules: host_cfg.get('frameworks'))
endif
lib_src += host_cfg.get('src', [])


#
# VAAPI dependency
#

libva_version = '>= 1.1.0'
vaapi_enabled = false

opt_wayland = get_option('wayland')
dep_wayland_client = dependency('wayland-client', required: opt_wayland)
dep_wayland_egl    = dependency('wayland-egl',    required: opt_wayland)
if dep_wayland_client.found() and dep_wayland_egl.found()
  lib_deps += [dep_wayland_client, dep_wayland_egl]
  conf_data.set10('HAVE_WAYLAND', true)
endif

opt_vaapi_x11 = get_option('vaapi-x11')
dep_libva_x11 = dependency('libva-x11', version: libva_version, required: opt_vaapi_x11)
dep_libva_x11_drm = dependency('libva-drm', version: libva_version, required: opt_vaapi_x11)
if dep_libva_x11.found() and dep_libva_x11_drm.found()
  vaapi_enabled = true
  lib_deps += [dep_libva_x11, dep_libva_x11_drm]
  conf_data.set10('HAVE_VAAPI_X11', true)
endif

opt_vaapi_wayland = get_option('vaapi-wayland')
dep_libva_wayland = dependency('libva-wayland', version: libva_version, required: opt_vaapi_wayland)
dep_libva_wayland_drm = dependency('libva-drm', version: libva_version, required: opt_vaapi_wayland)
if dep_libva_wayland.found() and dep_libva_wayland_drm.found()
  vaapi_enabled = true
  lib_deps += [dep_libva_wayland, dep_libva_wayland_drm]
  conf_data.set10('HAVE_VAAPI_WAYLAND', true)
endif

if vaapi_enabled
  lib_src += files('vaapi_ctx.c')
  conf_data.set10('HAVE_VAAPI', true)
endif


#
# Graphic backend dependencies
#

gbackends_cfg = {
  'gl': {
    'src': files(
      'backends/gl/buffer_gl.c',
      'backends/gl/format_gl.c',
      'backends/gl/gctx_gl.c',
      'backends/gl/glcontext.c',
      'backends/gl/glstate.c',
      'backends/gl/pipeline_gl.c',
      'backends/gl/program_gl.c',
      'backends/gl/rendertarget_gl.c',
      'backends/gl/texture_gl.c',
      'backends/gl/topology_gl.c',
      'backends/gl/type_gl.c',
    ),
    'cfg': 'BACKEND_GL',
    'hosts_cfg': {
      'linux': {
        'src': files('backends/gl/glcontext_egl.c'),
        'cfg': 'HAVE_GLPLATFORM_EGL',
        'deps': ['gl', 'egl'],
      },
      'darwin': {
        'src': files('backends/gl/glcontext_nsgl.m', 'backends/gl/hwupload_videotoolbox_darwin_gl.c'),
        'cfg': 'HAVE_GLPLATFORM_NSGL',
        'frameworks': ['OpenGL'],
      },
      'android': {
        'src': files('backends/gl/glcontext_egl.c', 'backends/gl/hwupload_mediacodec_gl.c'),
        'cfg': 'HAVE_GLPLATFORM_EGL',
        'libs': ['EGL'],
      },
      'iphone': {
        'src': files('backends/gl/glcontext_eagl.m', 'backends/gl/hwupload_videotoolbox_ios_gl.c'),
        'cfg': 'HAVE_GLPLATFORM_EAGL',
      },
      'windows': {
        'src': files('backends/gl/glcontext_wgl.c'),
        'cfg': 'HAVE_GLPLATFORM_WGL',
        'libs': ['OpenGL32', 'gdi32'],
      },
    },
  },
}

if host_system in ['darwin', 'iphone']
  add_languages('objc')
endif

foreach gbackend_name, gbackend_cfg : gbackends_cfg
  opt_gbackend = get_option('gbackend-' + gbackend_name)

  host_backend_cfg = gbackend_cfg.get('hosts_cfg').get(host_system, {})
  gbackend_deps = []
  foreach dep : host_backend_cfg.get('deps', [])
    gbackend_deps += dependency(dep, required: opt_gbackend)
  endforeach
  foreach dep : host_backend_cfg.get('libs', [])
    gbackend_deps += cc.find_library(dep, required: opt_gbackend)
  endforeach
  if 'frameworks' in host_backend_cfg
    gbackend_deps += dependency('appleframeworks', modules: host_backend_cfg.get('frameworks'), required: opt_gbackend)
  endif

  all_dep_found = true
  foreach dep : gbackend_deps
    all_dep_found = all_dep_found and dep.found()
  endforeach
  message('backend: @0@ enabled: @1@'.format(gbackend_name, all_dep_found.to_string()))
  if all_dep_found

    if gbackend_name == 'gl' and vaapi_enabled
      lib_src += files('backends/gl/hwupload_vaapi_gl.c')
    endif

    lib_src += gbackend_cfg.get('src')
    lib_src += host_backend_cfg.get('src')
    lib_deps += gbackend_deps
    conf_data.set10(gbackend_cfg.get('cfg'), true)
    conf_data.set10(host_backend_cfg.get('cfg'), true)
  endif
endforeach


#
# Library
#

configure_file(output: 'config.h', configuration: conf_data)

lib_c_args = []
if get_option('default_library') == 'shared'
  lib_c_args += '-DBUILD_NODEGL_SHARED_LIB'
endif

libnodegl = library(
  'nodegl',
  lib_src,
  dependencies: lib_deps,
  install: true,
  install_rpath: install_rpath,
  version: meson.project_version(),
  c_args: lib_c_args,
  gnu_symbol_visibility: 'hidden',
)
lib_header = configure_file(
  input: files('nodegl.h.in'),
  output: 'nodegl.h',
  configuration: conf_data
)
install_headers(lib_header)

pkg = import('pkgconfig')
pkg_extra_cflags = []

if get_option('default_library') == 'static'
  pkg_extra_cflags += '-DUSE_NODEGL_STATIC_LIB'
endif

pkg.generate(
  libnodegl,
  name: 'libnodegl',  # not specifying the name would fallback on "nodegl.pc" instead of "libnodegl.pc"
  description: 'Node/Graph based OpenGL engine',
  extra_cflags: pkg_extra_cflags,
  variables: ['datarootdir=${prefix}/share'],
)


#
# Specifications
#

copyfile = find_program('scripts/copyfile.py')
specs_filename = 'nodes.specs'
dest_datadir = get_option('datadir') / 'nodegl'
install_data(files(specs_filename), install_dir: dest_datadir)
install_data(files('android/java/OnFrameAvailableListener.java'), install_dir: dest_datadir / 'java/org/nodegl')

# XXX: should we use an intermediate static_library() to share the objects
# between the library and these tools?
# https://git.archlinux.org/pacman.git/tree/meson.build?id=4533c6a8e0f39c7707e671b7f9687607b46f1417#n310
# seem to imply some extract_all_objects(recursive: true)
gen_specs = executable(
  'gen_specs',
  lib_src + files('gen_specs.c'),
  dependencies: lib_deps,
  build_by_default: false,
  native: true,
)
specs_file = custom_target(
  specs_filename,
  command: gen_specs,
  capture: true,
  output: specs_filename,
)
run_target(
  'updatespecs',
  command: [copyfile, specs_file, meson.current_source_dir()],
)


#
# Doc
#

if not get_option('small')
  gen_doc = executable(
    'gen_doc',
    lib_src + files('gen_doc.c'),
    dependencies: lib_deps,
    build_by_default: false,
    native: true,
  )
  doc_file = custom_target(
    'libnodegl.md',
    command: gen_doc,
    capture: true,
    output: 'libnodegl.md',
  )
  run_target(
    'updatedoc',
    command: [copyfile, doc_file, meson.current_source_dir() / 'doc'],
  )
endif


#
# OpenGL specifications
#

gl_xml = custom_target(
  'gl.xml',
  command: [find_program('curl'), 'https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/master/xml/gl.xml', '-o', '@OUTPUT@'],
  capture: false,
  output: 'gl.xml',
)
gl_generated_files = custom_target(
  'gl_generated_files',
  command: [find_program('gen-gl-wrappers.py'), '@INPUT@', '@OUTPUT@'],
  capture: false,
  input: gl_xml,
  output: ['glfunctions.h', 'gldefinitions_data.h', 'glwrappers.h'],
)
run_target(
  'updateglwrappers',
  command: [
    copyfile,
    # FIXME: In 0.56.0, we get "WARNING: custom_target 'gl_generated_files' has
    # more than one output!  Using the first one.", so we need to do this
    # instead of just using gl_generated_files. It also requires the "depends:"
    # entry below.
    # See https://github.com/mesonbuild/meson/issues/7891 for more information
    gl_generated_files[0].full_path(),
    gl_generated_files[1].full_path(),
    gl_generated_files[2].full_path(),
    meson.current_source_dir() / 'backends/gl'
  ],
  depends: gl_generated_files,
)


#
# Tests
#

test_progs = {
  'Assembly': {
    'exe': 'test_asm',
    'src': files('test_asm.c', 'math_utils.c'),
  },
  'Color convertion': {
    'exe': 'test_colorconv',
    'src': files('test_colorconv.c', 'colorconv.c', 'log.c', 'memory.c'),
  },
  'Dynamic array': {
    'exe': 'test_darray',
    'src': files('test_darray.c', 'darray.c', 'memory.c'),
  },
  'Draw utils': {
    'exe': 'test_draw',
    'src': files('test_draw.c', 'drawutils.c', 'memory.c'),
    'args': ['ngl-test.ppm']
  },
  'Hash map': {
    'exe': 'test_hmap',
    'src': files('test_hmap.c', 'bstr.c', 'log.c', 'utils.c', 'memory.c'),
  },
  'Utils': {
    'exe': 'test_utils',
    'src': files('test_utils.c', 'bstr.c', 'log.c', 'utils.c', 'memory.c'),
  },
}

if get_option('tests')
  foreach test_key, test_data : test_progs
    exe = executable(
      test_data.get('exe'),
      test_data.get('src'),
      dependencies: lib_deps,
      build_by_default: false,
      install: false,
    )
    test(test_key, exe, args: test_data.get('args', []))
  endforeach
endif
