cmake.exe -H. -Bcmake-build-debug -DCMAKE_BUILD_TYPE=RelWithDebInfo -G "Visual Studio 16 2019" -DGRAPHICS_BACKEND_DIRECT3D12=ON
cmake.exe --build cmake-build-debug --config RelWithDebInfo -j8
