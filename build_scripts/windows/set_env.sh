#!/usr/bin/bash

if [[ ! -v VCPKG_DIR ]]; then
  export VCPKG_DIR='C:\vcpkg'
fi
if [[ ! -v VULKAN_SDK ]]; then
  export VULKAN_SDK=$(wslpath -w /mnt/c/VulkanSDK/*)
fi
CURRENT_DIR=`wslpath -wa .`
export PKG_CONFIG=$CURRENT_DIR'\venv\Scripts\pkgconf.exe'
export TARGET_OS=Windows
export DEBUG=yes
export DEBUG_GPU_CAPTURE=yes

export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
export PKG_CONFIG_DONT_DEFINE_PREFIX=1
export PKG_CONFIG_PATH=$VCPKG_DIR'\installed\x64-windows\lib\pkgconfig;'$CURRENT_DIR'\venv\Lib\pkgconfig'

export WSLENV=VULKAN_SDK/w:PKG_CONFIG/w:PKG_CONFIG_PATH/w:PKG_CONFIG_ALLOW_SYSTEM_LIBS/w:PKG_CONFIG_ALLOW_SYSTEM_CFLAGS/w:PKG_CONFIG_DONT_DEFINE_PREFIX/w:VCVARS64/w
