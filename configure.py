#!/usr/bin/env python3
#
# Copyright 2021 GoPro Inc.
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

import argparse
import glob
import hashlib
import logging
import os
import os.path as op
import pathlib
import platform
import shlex
import shutil
import sysconfig
import tarfile
import urllib.request
import venv
import zipfile
from multiprocessing import Pool, cpu_count
from subprocess import run
import ssl


_ROOTDIR = op.abspath(op.dirname(__file__))
_SYSTEM = 'MinGW' if sysconfig.get_platform() == 'mingw' else platform.system()
_RENDERDOC_ID = f'renderdoc_{_SYSTEM}'
_EXTERNAL_DEPS = dict(
    sxplayer=dict(
        version='9.9.0',
        url='https://github.com/Stupeflix/sxplayer/archive/v@VERSION@.tar.gz',
        dst_file='sxplayer-@VERSION@.tar.gz',
        sha256='a4baff3b70cc0b519128474d078ee7ec1099e880f869968b3d32693bdb5260de',
    ),
    pkgconf=dict(
        version='1.7.4',
        url='https://distfiles.dereferenced.org/pkgconf/pkgconf-@VERSION@.tar.xz',
        sha256='d73f32c248a4591139a6b17777c80d4deab6b414ec2b3d21d0a24be348c476ab',
    ),
    renderdoc_Windows=dict(
        version='1.14',
        url=f'https://renderdoc.org/stable/@VERSION@/RenderDoc_@VERSION@_64.zip',
        sha256='d91bac6702e832543f4bc7a8ee260e8e2176d450170fb2fc0b111c6a889ce3d5',
    ),
    renderdoc_Linux=dict(
        version='1.14',
        url=f'https://renderdoc.org/stable/@VERSION@/renderdoc_@VERSION@.tar.gz',
        sha256='8ae12b5c0ab28dc6fd60e0a1a8e52dbb49b8745b48c08ff27c70139f05a6e545',
    ),
    shaderc=dict(
        version='2020.5',
        dst_file='shaderc-@VERSION@.tar.gz',
        url='https://github.com/google/shaderc/archive/v@VERSION@.tar.gz',
        sha256='e96d8cb208b796cecb9e6cce437c7d1116343158ef3ea26277eb13b62cf56834',
    ),
    moltenvk=dict(
        version='1.1.3',
        dst_file='moltenvk-@VERSION@.tar.gz',
        url='https://github.com/KhronosGroup/MoltenVK/archive/v@VERSION@.tar.gz',
        sha256='c20758bc19a46060aaf6e0949b47d29824b70b9ec0e22fb73a3feeef4c73a0ef',
    ),
    ngfx=dict(
        url='https://github.com/gopro/ngfx.git',
        branch='develop',
    ),
)


def _get_external_deps(args):
    deps = ['sxplayer']
    if _SYSTEM == 'Windows':
        deps += ['pkgconf', 'shaderc']
    if 'gpu_capture' in args.debug_opts:
        if _SYSTEM  in {'Windows', 'Linux'}:
            deps.append(_RENDERDOC_ID)
    if _SYSTEM == 'Darwin':
        deps += ['shaderc', 'moltenvk']
    if args.enable_ngfx_backend:
        deps += ['ngfx']
    return {dep: _EXTERNAL_DEPS[dep] for dep in deps}


def _guess_base_dir(dirs):
    smallest_dir = sorted(dirs, key=lambda x: len(x))[0]
    return pathlib.Path(smallest_dir).parts[0]


def _file_chk(path, chksum_hexdigest):
    chksum = hashlib.sha256()
    with open(path, 'rb') as f:
        while True:
            buf = f.read(8196)
            if not buf:
                break
            chksum.update(buf)
    match = chksum.hexdigest() == chksum_hexdigest
    if not match:
        logging.warning('%s: mismatching check sum', path)
    return match


def _download_extract(dep_item):
    logging.basicConfig(level='INFO')  # Needed for every process on Windows

    name, dep = dep_item

    version = dep['version'] if 'version' in dep else ''
    url = dep['url'].replace('@VERSION@', version)
    chksum = dep.get('sha256')
    is_git_repo = url.endswith('.git')
    if is_git_repo:
        git_branch = dep['branch'] if 'branch' in dep else 'main'
    dst_file = dep.get('dst_file', op.basename(url)).replace('@VERSION@', version)
    if dst_file.endswith('.git'):
        dst_file = dst_file[:-4]
    dst_base = op.join(_ROOTDIR, 'external')
    dst_path = op.join(dst_base, dst_file)
    os.makedirs(dst_base, exist_ok=True)

    # Download
    if is_git_repo:
        if not op.exists(dst_path):
            logging.info('cloning %s', url)
            run(['git', 'clone', url, '-b', git_branch, dst_path], check=True)
    else:
        if not op.exists(dst_path) or not _file_chk(dst_path, dep['sha256']):
            logging.info('downloading %s to %s', url, dst_file)
            # Temp workaround for pkgconf ssl verify error
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            with urllib.request.urlopen(url, context=ctx) as u, open(dst_path, 'wb') as f:
                f.write(u.read())

    # Extract
    # FIXME
    if is_git_repo:
        pass
    elif tarfile.is_tarfile(dst_path):
        with tarfile.open(dst_path) as tar:
            dirs = {f.name for f in tar.getmembers() if f.isdir()}
            extract_dir = op.join(dst_base, _guess_base_dir(dirs))
            if not op.exists(extract_dir):
                logging.info('extracting %s', dst_file)
                tar.extractall(dst_base)

    elif zipfile.is_zipfile(dst_path):
        with zipfile.ZipFile(dst_path) as zip_:
            dirs = {op.dirname(f) for f in zip_.namelist()}
            extract_dir = op.join(dst_base, _guess_base_dir(dirs))
            if not op.exists(extract_dir):
                logging.info('extracting %s', dst_file)
                zip_.extractall(dst_base)
    else:
        assert False

    # Remove previous link if needed
    # FIXME
    target = op.join(dst_base, name)
    if is_git_repo:
        pass
    else:
        rel_extract_dir = op.basename(extract_dir)
        if op.islink(target) and os.readlink(target) != rel_extract_dir:
            logging.info('unlink %s target', target)
            os.unlink(target)
        elif op.exists(target) and not op.islink(target):
            logging.info('remove previous %s copy', target)
            shutil.rmtree(target)

    # Link (or copy)
    # FIXME
    if is_git_repo:
        pass
    elif not op.exists(target):
        logging.info('symlink %s -> %s', target, rel_extract_dir)
        try:
            os.symlink(rel_extract_dir, target)
        except OSError:
            # This typically happens on Windows when Developer Mode is not
            # available/enabled
            logging.info('unable to symlink, fallback on copy (%s -> %s)', extract_dir, target)
            shutil.copytree(extract_dir, target)

    return name, target


def _fetch_externals(args):
    dependencies = _get_external_deps(args)
    with Pool() as p:
        return dict(p.map(_download_extract, dependencies.items()))


def _block(name, prerequisites=None):
    def real_decorator(block_func):
        block_func.name = name
        block_func.prerequisites = prerequisites if prerequisites else []
        return block_func
    return real_decorator

def _get_cmake_setup_options(cfg, build_type = '$(CMAKE_BUILD_TYPE)', generator = '$(CMAKE_GENERATOR)', prefix = '$(PREFIX)', external_dir = '$(EXTERNAL_DIR)'):
    opts = f'-DCMAKE_BUILD_TYPE={build_type} -G\"{generator}\" -DCMAKE_INSTALL_PREFIX=\"{prefix}\" -DEXTERNAL_DIR=\"{external_dir}\"'
    if _SYSTEM == 'Windows':
        opts += f' -DCMAKE_INSTALL_INCLUDEDIR=Include -DCMAKE_INSTALL_LIBDIR=Lib -DCMAKE_INSTALL_BINDIR=Scripts'
        # Always use MultiThreadedDLL (/MD), not MultiThreadedDebugDLL (/MDd)
        # Some external libraries are only available in Release mode, not in Debug mode
        # MSVC toolchain doesn't allow mixing libraries built in /MD mode with libraries built in /MDd mode
        opts += f' -DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreadedDLL'
        # Set Windows SDK Version
        opts += f' -DCMAKE_SYSTEM_VERSION=$(CMAKE_SYSTEM_VERSION)'
        # Set VCPKG Directory
        opts += f' -DVCPKG_DIR=$(VCPKG_DIR)'
    return opts

def _get_cmake_compile_options(cfg, build_type = '$(CMAKE_BUILD_TYPE)', num_threads = cpu_count() + 1):
    opts = f'--config {build_type} -j{num_threads}'
    if cfg.args.verbose:
        opts += '-v'
    return opts

def _get_cmake_install_options(cfg, build_type = '$(CMAKE_BUILD_TYPE)'):
    return f'--config {build_type}'

def _meson_compile_install_cmd(component):
    builddir = op.join('$(BUILDDIR)', component)
    return ['$(MESON) ' + _cmd_join(action, '-C', builddir) for action in ('compile', 'install')]


@_block('pkgconf-setup')
def _pkgconf_setup(cfg):
    return ['$(MESON_SETUP) ' + _cmd_join('-Dtests=false', cfg.externals['pkgconf'], op.join('$(BUILDDIR)', 'pkgconf'))]


@_block('pkg-conf-install', [_pkgconf_setup])
def _pkgconf_install(cfg):
    if _SYSTEM != 'Windows':
        return []
    ret = _meson_compile_install_cmd('pkgconf')
    pkgconf_exe = op.join(cfg.bin_path, 'pkgconf.exe')
    pkgconfig_exe = op.join(cfg.bin_path, 'pkg-config.exe')
    return ret + [f'copy {pkgconf_exe} {pkgconfig_exe}']


@_block('sxplayer-setup')
def _sxplayer_setup(cfg):
    return ['$(MESON_SETUP) ' + _cmd_join(cfg.externals['sxplayer'], op.join('$(BUILDDIR)', 'sxplayer'))]


@_block('sxplayer-install', [_sxplayer_setup])
def _sxplayer_install(cfg):
    return _meson_compile_install_cmd('sxplayer')


@_block('renderdoc-install')
def _renderdoc_install(cfg):
    renderdoc_dll = op.join(cfg.externals[_RENDERDOC_ID], 'renderdoc.dll')
    return [f'copy {renderdoc_dll} {cfg.bin_path}']

@_block('shaderc-install')
def _shaderc_install(cfg):
    shaderc_cmake_setup_options = _get_cmake_setup_options(cfg, build_type='Release', generator='Ninja')
    shaderc_cmake_compile_options = _get_cmake_compile_options(cfg, build_type='Release')
    shaderc_cmake_install_options = _get_cmake_install_options(cfg, build_type='Release')
    if _SYSTEM == 'Darwin':
        shaderc_lib_filename = 'libshaderc_shared.1.dylib'
    cmd = []
    if _SYSTEM in ['Darwin', 'Windows']:
        cmd += [ 'cd external/shaderc && python ./utils/git-sync-deps' ]
        cmd += [
            f'$(CMAKE) -S external/shaderc -B $(BUILDDIR)/shaderc {shaderc_cmake_setup_options} -DSHADERC_SKIP_TESTS=ON',
            f'$(CMAKE) --build $(BUILDDIR)/shaderc {shaderc_cmake_compile_options}',
            f'$(CMAKE) --install $(BUILDDIR)/shaderc {shaderc_cmake_install_options}'
        ]
        if _SYSTEM == 'Darwin':
            cmd += [ f'install_name_tool -id @rpath/{shaderc_lib_filename} $(PREFIX)/lib/{shaderc_lib_filename}' ]
    return cmd

@_block('ngl-debug-tools-install')
def _ngl_debug_tools_install(cfg):
    ngl_debug_tools_cmake_setup_options = _get_cmake_setup_options(cfg)
    ngl_debug_tools_cmake_compile_options = _get_cmake_compile_options(cfg)
    ngl_debug_tools_cmake_install_options = _get_cmake_install_options(cfg)
    cmd = [
        f'$(CMAKE) -S ngl-debug-tools -B $(BUILDDIR)/ngl-debug-tools {ngl_debug_tools_cmake_setup_options}',
        f'$(CMAKE) --build $(BUILDDIR)/ngl-debug-tools --verbose {ngl_debug_tools_cmake_compile_options}',
        f'$(CMAKE) --install $(BUILDDIR)/ngl-debug-tools {ngl_debug_tools_cmake_install_options}'
    ]
    return cmd

@_block('ngfx-install', [_shaderc_install])
def _ngfx_install(cfg):
    ngfx_deps = ['nlohmann-json', 'stb']
    if _SYSTEM == 'Windows':
        ngfx_deps += ['glm', 'd3dx12', 'glfw']
    ngfx_deps_str = ' '.join(s for s in ngfx_deps)
    if _SYSTEM == 'Windows':
        ngfx_install_deps_cmd = f'echo& set OS={_SYSTEM}& set PKGS={ngfx_deps_str}& python external/ngfx/build_scripts/install_deps.py&'
    else:
        ngfx_install_deps_cmd = f'export OS={_SYSTEM} && export PKGS="{ngfx_deps_str}" && python external/ngfx/build_scripts/install_deps.py'
    cmd = [ ngfx_install_deps_cmd ]
    ngfx_cmake_setup_options = _get_cmake_setup_options(cfg)
    ngfx_cmake_compile_options = _get_cmake_compile_options(cfg)
    ngfx_cmake_install_options = _get_cmake_install_options(cfg)
    cmd += [
        f'$(CMAKE) -S external/ngfx -B $(BUILDDIR)/ngfx {ngfx_cmake_setup_options} -D$(NGFX_GRAPHICS_BACKEND)=ON',
        f'$(CMAKE) --build $(BUILDDIR)/ngfx --verbose {ngfx_cmake_compile_options}',
        f'$(CMAKE) --install $(BUILDDIR)/ngfx {ngfx_cmake_install_options}'
    ]
    if cfg.ngfx_graphics_backend == 'NGFX_GRAPHICS_BACKEND_DIRECT3D12':
        cmd += [ '$(PREFIX)\\Scripts\\ngfx_compile_shaders_dx12.exe d3dBlitOp' ]
    return cmd

@_block('nodegl-setup', [_sxplayer_install])
def _nodegl_setup(cfg):
    nodegl_setup_opts = ['--default-library', 'shared']
    nodegl_debug_opts = []
    if cfg.args.debug_opts:
        debug_opts = ','.join(cfg.args.debug_opts)
        nodegl_debug_opts += [f'-Ddebug_opts={debug_opts}']
        if 'gpu_capture' in cfg.args.debug_opts:
            if _SYSTEM in ('Linux', 'Windows'):
                renderdoc_dir = cfg.externals[_RENDERDOC_ID]
                nodegl_debug_opts += [f'-Drenderdoc_dir={renderdoc_dir}']
        nodegl_setup_opts += nodegl_debug_opts

    if cfg.args.enable_ngfx_backend:
        nodegl_setup_opts += [f'-Denable_ngfx_backend=true']
        nodegl_setup_opts += [f'-Dngfx_graphics_backend=$(NGFX_GRAPHICS_BACKEND)']
    else:
        nodegl_setup_opts += [f'-Denable_ngfx_backend=false']


    return ['$(MESON_SETUP) ' + _cmd_join(*nodegl_setup_opts, 'libnodegl', op.join('$(BUILDDIR)', 'libnodegl'))]


@_block('nodegl-install', [_nodegl_setup])
def _nodegl_install(cfg):
    return _meson_compile_install_cmd('libnodegl')


@_block('pynodegl-deps-install', [_nodegl_install])
def _pynodegl_deps_install(cfg):
    return ['$(PIP) ' + _cmd_join('install', '-r', op.join('.', 'pynodegl', 'requirements.txt'))]


@_block('pynodegl-install', [_pynodegl_deps_install])
def _pynodegl_install(cfg):
    ret = ['$(PIP) ' + _cmd_join('-v', 'install', '-e', op.join('.', 'pynodegl'))]
    if _SYSTEM == 'Windows':
        dlls = op.join(cfg.prefix, 'Scripts', '*.dll')
        ret += [f'xcopy /Y {dlls} pynodegl\\.']
    else:
        rpath = op.join(cfg.prefix, 'lib')
        ldflags = f'-Wl,-rpath,{rpath}'
        ret[0] = f'LDFLAGS={ldflags} {ret[0]}'
    return ret


@_block('pynodegl-utils-deps-install', [_pynodegl_install])
def _pynodegl_utils_deps_install(cfg):
    #
    # Requirements not installed on MinGW because:
    # - PySide2 can't be pulled (required to be installed by the user outside the
    #   Python virtual env)
    # - Pillow fails to find zlib (required to be installed by the user outside the
    #   Python virtual env)
    #
    if _SYSTEM == 'MinGW':
        return ['@']  # noop
    return ['$(PIP) ' + _cmd_join('install', '-r', op.join('.', 'pynodegl-utils', 'requirements.txt'))]


@_block('pynodegl-utils-install', [_pynodegl_utils_deps_install])
def _pynodegl_utils_install(cfg):
    return ['$(PIP) ' + _cmd_join('-v', 'install', '-e', op.join('.', 'pynodegl-utils'))]


@_block('ngl-tools-setup', [_nodegl_install])
def _ngl_tools_setup(cfg):
    return ['$(MESON_SETUP) ' + _cmd_join('ngl-tools', op.join('$(BUILDDIR)', 'ngl-tools'))]


@_block('ngl-tools-install', [_ngl_tools_setup])
def _ngl_tools_install(cfg):
    return _meson_compile_install_cmd('ngl-tools')


def _nodegl_run_target_cmd(cfg, target):
    builddir = op.join('$(BUILDDIR)', 'libnodegl')
    return ['$(MESON) ' + _cmd_join('compile', '-C', builddir, target)]


@_block('nodegl-updatedoc', [_nodegl_install])
def _nodegl_updatedoc(cfg):
    return _nodegl_run_target_cmd(cfg, 'updatedoc')


@_block('nodegl-updatespecs', [_nodegl_install])
def _nodegl_updatespecs(cfg):
    return _nodegl_run_target_cmd(cfg, 'updatespecs')


@_block('nodegl-updateglwrappers', [_nodegl_install])
def _nodegl_updateglwrappers(cfg):
    return _nodegl_run_target_cmd(cfg, 'updateglwrappers')


@_block('all', [_ngl_tools_install, _pynodegl_utils_install])
def _all(cfg):
    echo = ['', 'Build completed.', '', 'You can now enter the venv with:']
    if _SYSTEM == 'Windows':
        echo.append(op.join(cfg.bin_path, 'Activate.ps1'))
        return [f'@echo.{e}' for e in echo]
    else:
        echo.append(' ' * 4 + '. ' + op.join(cfg.bin_path, 'activate'))
        return [f'@echo "    {e}"' for e in echo]


@_block('tests-setup', [_ngl_tools_install, _pynodegl_utils_install])
def _tests_setup(cfg):
    return ['$(MESON_SETUP_TESTS) ' + _cmd_join('tests', op.join('$(BUILDDIR)', 'tests'))]


@_block('nodegl-tests', [_nodegl_install])
def _nodegl_tests(cfg):
    return [_cmd_join('meson', 'test', '-C', op.join('$(BUILDDIR)', 'libnodegl'))]


def _rm(f):
    return f'(if exist {f} del /q {f})' if _SYSTEM == 'Windows' else f'$(RM) {f}'

def _rd(d):
    return f'(if exist {d} rd /s /q {d})' if _SYSTEM == 'Windows' else f'$(RM) -r {d}'


@_block('clean-py')
def _clean_py(cfg):
    return [
        _rm(op.join('pynodegl', 'nodes_def.pyx')),
        _rm(op.join('pynodegl', 'pynodegl.c')),
        _rm(op.join('pynodegl', 'pynodegl.*.so')),
        _rm(op.join('pynodegl', 'pynodegl.*.pyd')),
        _rd(op.join('pynodegl', 'build')),
        _rd(op.join('pynodegl', 'pynodegl.egg-info')),
        _rd(op.join('pynodegl', '.eggs')),
        _rd(op.join('pynodegl-utils', 'pynodegl_utils.egg-info')),
        _rd(op.join('pynodegl-utils', '.eggs')),
    ]


@_block('clean', [_clean_py])
def _clean(cfg):
    return [
        _rd(op.join('$(BUILDDIR)', 'libnodegl')),
        _rd(op.join('$(BUILDDIR)', 'moltenvk')),
        _rd(op.join('$(BUILDDIR)', 'ngl-tools')),
        _rd(op.join('$(BUILDDIR)', 'pkgconf')),  # XXX backport
        _rd(op.join('$(BUILDDIR)', 'shaderc')),
        _rd(op.join('$(BUILDDIR)', 'sxplayer')),
        _rd(op.join('$(BUILDDIR)', 'tests')),
    ]


def _coverage(cfg, output):
    # We don't use `meson coverage` here because of
    # https://github.com/mesonbuild/meson/issues/7895
    return [_cmd_join('ninja', '-C', op.join('$(BUILDDIR)', 'libnodegl'), f'coverage-{output}')]


@_block('coverage-html')
def _coverage_html(cfg):
    return _coverage(cfg, 'html')


@_block('coverage-xml')
def _coverage_xml(cfg):
    return _coverage(cfg, 'xml')


@_block('tests', [_nodegl_tests, _tests_setup])
def _tests(cfg):
    return [_cmd_join('meson', 'test', '-C', op.join('$(BUILDDIR)', 'tests'))]


@_block('shaderc-setup')
def _shaderc_setup(cfg):
    srcdir = cfg.externals['shaderc']
    builddir = op.join('builddir', 'shaderc')
    return [
        f'cd {srcdir} && ./utils/git-sync-deps',
        f'$(CMAKE_SETUP) -S {srcdir} -B {builddir}',
    ]

@_block('shaderc-install', [_shaderc_setup])
def _shaderc_install(cfg):
    lib_filename = 'libshaderc_shared.1.dylib'
    builddir = op.join('builddir', 'shaderc')
    return [
        f'cmake --build {builddir} --target install',
        f'install_name_tool -id @rpath/{lib_filename} {cfg.prefix}/lib/{lib_filename}',
    ]


@_block('moltenvk-setup')
def _moltenvk_setup(cfg):
    srcdir = cfg.externals['moltenvk']
    builddir = op.join('builddir', 'moltenvk')
    return [
        f'cd {srcdir} && ./fetchDependencies -v --macos',
    ]


# Note: somehow xcodebuild sets name @rpath/libMoltenVK.dylib automatically
# (according to otool -l) so we don't have to do anything special
@_block('moltenvk-install', [_moltenvk_setup])
def _moltenvk_install(cfg):
    srcdir = cfg.externals['moltenvk']   # XXX working in the sources directly might not be a good idea
    return [
        f'make -C {srcdir} macos',
        f'cp -v {srcdir}/Package/Latest/MoltenVK/dylib/macOS/libMoltenVK.dylib {cfg.prefix}/lib',
        f'cp -vr {srcdir}/Package/Latest/MoltenVK/include/. {cfg.prefix}/include',
    ]


def _quote(s):
    if not s or ' ' in s:
        return f'"{s}"'
    assert "'" not in s
    assert '"' not in s
    return s


def _cmd_join(*cmds):
    if _SYSTEM == 'Windows':
        return ' '.join(_quote(cmd) for cmd in cmds)
    return shlex.join(cmds)


def _get_make_vars(cfg):
    debug = cfg.args.coverage or cfg.args.buildtype == 'debug'

    # We don't want Python to fallback on one found in the PATH so we explicit
    # it to the one in the venv.
    python = op.join(cfg.bin_path, 'python')

    #
    # MAKEFLAGS= is a workaround (not working on Windows due to incompatible Make
    # syntax) for the issue described here:
    # https://github.com/ninja-build/ninja/issues/1139#issuecomment-724061270
    #
    # Note: this will invoke the meson in the venv, unless we're on MinGW where
    # it will fallback on the system one. This is due to the extended PATH
    # mechanism.
    #
    meson = 'MAKEFLAGS= meson' if _SYSTEM != 'Windows' else 'meson'
    cmake = 'cmake'
    builddir = os.getenv("BUILDDIR", 'builddir')
    buildtype = 'debug' if cfg.args.coverage or cfg.args.buildtype == 'debug' else 'release'
    meson_setup = [
        'setup',
        '--prefix', '$(PREFIX)',
        '-Drpath=true',
        '--pkg-config-path', '$(PKG_CONFIG_PATH)',
        '--buildtype', buildtype,
    ]
    if cfg.args.coverage:
        meson_setup += ['-Db_coverage=true']
    if _SYSTEM != 'MinGW':
        meson_setup += ['-Db_lto=true']

    cmake_builddtype = 'Debug' if debug else 'Release'
    cmake_setup = [
        f'-DCMAKE_INSTALL_PREFIX={cfg.prefix}',
        f'-DCMAKE_BUILD_TYPE={cmake_builddtype}',
        '-GNinja',
    ]

    if _SYSTEM == 'Windows':
        meson_setup += ['--bindir=Scripts', '--libdir=Lib', '--includedir=Include']
    elif op.isfile('/etc/debian_version'):
        # Workaround Debian/Ubuntu bug; see https://github.com/mesonbuild/meson/issues/5925
        meson_setup += ['--libdir=lib']

    if _SYSTEM == 'Windows':
        # Use Multithreaded DLL runtime library for debug and release configurations
        # Third party libs are compiled in release mode
        # Visual Studio toolchain requires all libraries to use the same runtime library
        meson_setup += ['-Db_vscrt=md']

    ret = dict(
        BUILDDIR = builddir,
        PREFIX = cfg.prefix, #.replace('\\','/'),
        PIP = _cmd_join(python, '-m', 'pip'),
        MESON = meson,
        MESON_BACKEND = {'Windows': 'vs', 'Linux': 'ninja', 'Darwin': 'ninja', 'MinGW': 'ninja'}[_SYSTEM],
        MESON_SETUP = '$(MESON) ' + _cmd_join(*meson_setup, '--backend=$(MESON_BACKEND)'),
        MESON_SETUP_TESTS = '$(MESON) ' + _cmd_join(*meson_setup, '--backend=ninja'),
        CMAKE = cmake,
        CMAKE_GENERATOR = cfg.cmake_generator,
        CMAKE_BUILD_TYPE = cfg.cmake_build_type,
    )

    # FIXME: ...
    ret['MESON_SETUP'] = '$(MESON) ' + _cmd_join(*meson_setup, f'--backend={cfg.args.build_backend}')
    # Our tests/meson.build logic is not well supported with the VS backend so
    # we need to fallback on Ninja
    ret['MESON_SETUP_TESTS'] = '$(MESON) ' + _cmd_join(*meson_setup, '--backend=ninja')
    ret['CMAKE_SETUP'] = 'cmake ' + _cmd_join(*cmake_setup)

    if _SYSTEM == 'Windows':
        ret['CMAKE_SYSTEM_VERSION'] = cfg.cmake_system_version
        ret['VCPKG_DIR'] = cfg.args.vcpkg_dir

    if cfg.args.enable_ngfx_backend:
        ret['NGFX_GRAPHICS_BACKEND'] = cfg.ngfx_graphics_backend

    return ret


def _get_makefile_rec(cfg, blocks, declared):
    ret = ''
    for block in blocks:
        if block.name in declared:
            continue
        declared |= {block.name}
        req_names = ' '.join(r.name for r in block.prerequisites)
        req = f' {req_names}' if req_names else ''
        commands = '\n'.join('\t' + cmd for cmd in block(cfg))
        ret += f'{block.name}:{req}\n{commands}\n'
        ret += _get_makefile_rec(cfg, block.prerequisites, declared)
    return ret


def _get_makefile(cfg, blocks):
    env = cfg.get_env()
    env_vars = {k: _quote(v) for k, v in env.items()}
    if _SYSTEM == 'Windows':
        #
        # Environment variables are altered if and only if they already exists
        # in the environment. While this is (usually) true for PATH, it isn't
        # for the others we're trying to declare. This "if [set...]" trick is
        # to circumvent this issue.
        #
        # See https://stackoverflow.com/questions/38381422/how-do-i-set-an-environment-variables-with-nmake
        #
        vars_export = '\n'.join(f'{k} = {v}' for k, v in env_vars.items()) + '\n'
        vars_export_cond = ' || '.join(f'[set {k}=$({k})]' for k in env_vars.keys())
        vars_export += f'!if {vars_export_cond}\n!endif\n'
    else:
        # We must immediate assign with ":=" since sometimes values contain
        # reference to themselves (typically PATH)
        vars_export = '\n'.join(f'export {k} := {v}' for k, v in env_vars.items()) + '\n'

    make_vars = _get_make_vars(cfg)
    ret = '\n'.join(f'{k} = {v}' for k, v in make_vars.items()) + '\n' + vars_export

    declared = set()
    ret += _get_makefile_rec(cfg, blocks, declared)
    ret += '.PHONY: ' + ' '.join(declared) + '\n'
    return ret


class _EnvBuilder(venv.EnvBuilder):

    def __init__(self):
        super().__init__(system_site_packages=_SYSTEM == 'MinGW', with_pip=True, prompt='nodegl')

    def post_setup(self, context):
        if _SYSTEM == 'MinGW':
            return
        pip_install = [context.env_exe, '-m', 'pip', 'install']
        pip_install += ['meson', 'ninja']
        logging.info('install build dependencies: %s', _cmd_join(*pip_install))
        run(pip_install, check=True)


def _build_venv(args):
    if op.exists(args.venv_path):
        logging.warning('Python virtual env already exists at %s', args.venv_path)
        return
    logging.info('creating Python virtualenv: %s', args.venv_path)
    _EnvBuilder().create(args.venv_path)


class _Config:

    def __init__(self, args):
        self.args = args
        self.prefix = op.abspath(args.venv_path)

        # On MinGW we need the path translated from C:\ to /c/, because when
        # part of the PATH, the ':' separator will break
        if _SYSTEM == 'MinGW':
            self.prefix = run(['cygpath', '-u', self.prefix], capture_output=True, text=True).stdout.strip()

        self.bin_name = 'Scripts' if _SYSTEM == 'Windows' else 'bin'
        self.bin_path = op.join(self.prefix, self.bin_name)
        if _SYSTEM == 'Windows':
            vcpkg_pc_path = op.join(args.vcpkg_dir, 'installed', 'x64-windows', 'lib', 'pkgconfig')
            self.pkg_config_path = os.pathsep.join((vcpkg_pc_path, op.join(self.prefix, 'Lib', 'pkgconfig'))) #.replace('\\','/')
        else:
            self.pkg_config_path = op.join(self.prefix, 'lib', 'pkgconfig')
        self.externals = _fetch_externals(args)

        if args.enable_ngfx_backend:
            self.ngfx_graphics_backend = os.getenv('NGFX_GRAPHICS_BACKEND',
                'NGFX_GRAPHICS_BACKEND_' + {'Windows': 'DIRECT3D12', 'Linux': 'VULKAN', 'Darwin': 'METAL'}[_SYSTEM])

        self.cmake_generator = os.getenv("CMAKE_GENERATOR", {'Windows': 'Visual Studio 16 2019', 'Linux': 'Ninja', 'Darwin': 'Xcode', 'MinGW': 'Ninja'}[_SYSTEM])

        if _SYSTEM == 'Windows':
            # Set Windows SDK Version
            self.cmake_system_version = os.getenv('CMAKE_SYSTEM_VERSION', '10.0.18362.0')

        if args.buildtype == 'debug':
            self.cmake_build_type = 'Debug'
        else:
            self.cmake_build_type = 'Release'

        if _SYSTEM == 'Windows':
            _sxplayer_setup.prerequisites.append(_pkgconf_install)
            _ngfx_install.prerequisites.insert(0, _pkgconf_install)
            if 'gpu_capture' in args.debug_opts:
                _nodegl_setup.prerequisites.append(_renderdoc_install)

            vcpkg_bin = op.join(args.vcpkg_dir, 'installed', 'x64-windows', 'bin')
            for f in glob.glob(op.join(vcpkg_bin, '*.dll')):
                logging.info('copy %s to venv/Scripts', f)
                shutil.copy2(f, op.join('venv', 'Scripts'))

            shaderc_dll = op.join(args.vcpkg_dir, 'installed', 'x64-windows', 'tools', 'shaderc_shared.dll')
            if op.exists(shaderc_dll):
                shutil.copy2(shaderc_dll, op.join('venv', 'Scripts'))

        if args.enable_ngfx_backend:
            _nodegl_setup.prerequisites.append(_ngfx_install)

    def get_env(self):
        sep = ':' if _SYSTEM == 'MinGW' else os.pathsep
        env = {}
        env['PATH'] = sep.join((self.bin_path, '$(PATH)'))
        env['EXTERNAL_DIR'] = op.join(_ROOTDIR, 'external')
        env['PKG_CONFIG_PATH'] = self.pkg_config_path
        if _SYSTEM == 'Windows':
            env['PKG_CONFIG'] = op.join(self.args.venv_path, 'Scripts', 'pkgconf.exe')
            env['PKG_CONFIG_ALLOW_SYSTEM_LIBS'] = '1'
            env['PKG_CONFIG_ALLOW_SYSTEM_CFLAGS'] = '1'
        return env

def _run():
    default_build_backend = 'ninja' if _SYSTEM != 'Windows' else 'vs'
    parser = argparse.ArgumentParser(
        prog='ngl-env',
        description='Create and manage a standalone node.gl virtual environement',
    )
    parser.add_argument('-p', '--venv-path', default=op.join(_ROOTDIR, 'venv'),
                        help='Virtual environment directory')
    parser.add_argument('--buildtype', choices=('release', 'debug'), default='release',
                        help='Build type')
    parser.add_argument('--coverage', action='store_true',
                        help='Code coverage')
    parser.add_argument('-d', '--debug-opts', nargs='+', default=[],
                        choices=('gl', 'vk', 'mem', 'scene', 'gpu_capture'),
                        help='Debug options')
    parser.add_argument('--build-backend', choices=('ninja', 'vs'), default=default_build_backend,
                        help='Build backend to use')
    parser.add_argument('-v','--verbose', action='store_true',
                        help='Enable verbose output')
    if _SYSTEM == 'Windows':
        parser.add_argument('--vcpkg-dir', default=r'C:\vcpkg',
                            help='Vcpkg directory')
    parser.add_argument('--enable-ngfx-backend', action='store_true',
                        help='Enable NGFX Backend')

    args = parser.parse_args()

    logging.basicConfig(level='INFO')

    _build_venv(args)

    dst_makefile = op.join(_ROOTDIR, 'Makefile')
    logging.info('writing %s', dst_makefile)
    cfg = _Config(args)
    blocks = [
        _all, _tests, _clean,
        _nodegl_updatedoc, _nodegl_updatespecs, _nodegl_updateglwrappers,
        _ngl_debug_tools_install,
    ]
    if _SYSTEM == 'Darwin':
        blocks += [_shaderc_install, _moltenvk_install]
    if args.coverage:
        blocks += [_coverage_html, _coverage_xml]

    makefile = _get_makefile(cfg, blocks)
    with open(dst_makefile, 'w') as f:
        f.write(makefile)

if __name__ == '__main__':
    _run()
