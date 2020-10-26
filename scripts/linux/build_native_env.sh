#set -x
cmake -H. -Bcmake-build-debug -DCMAKE_BUILD_TYPE=Debug -G "CodeBlocks - Ninja" -DNGFX_GRAPHICS_BACKEND_VULKAN=ON
cmake --build cmake-build-debug --config Debug -j16
