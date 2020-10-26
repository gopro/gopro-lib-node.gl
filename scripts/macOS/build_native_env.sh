#set -x
cmake -H. -Bcmake-build-debug -DCMAKE_BUILD_TYPE=Debug -G "Xcode" -DNGFX_GRAPHICS_BACKEND_METAL=ON
cmake --build cmake-build-debug --config Debug -j16
