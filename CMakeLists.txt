cmake_minimum_required(VERSION 3.14)
project (gopro-nodegl)

set(CMAKE_CXX_STANDARD 20)
set (CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set (BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build-debug)
set (EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

set(CMAKE_FIND_DEBUG_MODE TRUE)
find_package (Python3 COMPONENTS Interpreter Development)
find_package (OpenGL COMPONENTS OpenGL)
    
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

set (PTHREADS_DIR ${EXTERNAL_DIR}/win64/pthreads-w32-2-9-1-release/Pre-built.2)
set (PTHREADS_INCLUDE_DIRS ${PTHREADS_DIR}/include)
set (PTHREADS_CFLAGS -DHAVE_STRUCT_TIMESPEC)
set (PTHREADS_LIBRARY_DIRS ${PTHREADS_DIR}/lib/x64)
set (PTHREADS_LIBRARIES pthreadVC2)

set (FFMPEG_DIR ${EXTERNAL_DIR}/win64/ffmpeg-20200831-4a11a6f-win64-dev)
set (FFMPEG_INCLUDE_DIRS ${FFMPEG_DIR}/include)
set (FFMPEG_LIBRARY_DIRS ${FFMPEG_DIR}/lib)
set (FFMPEG_LIBRARIES avcodec avdevice avformat avfilter avutil)

set (GLEW_DIR ${EXTERNAL_DIR}/win64/glew-2.1.0-win32/glew-2.1.0)
set (GLEW_INCLUDE_DIRS ${GLEW_DIR}/include)
set (GLEW_LIBRARY_DIRS ${GLEW_DIR}/lib/Release/x64)
set (GLEW_LIBRARIES glew32)

set (GLFW_DIR ${EXTERNAL_DIR}/win64/glfw-3.3.2.bin.WIN64)
set (GLFW_INCLUDE_DIRS ${GLFW_DIR}/include)
set (GLFW_LIBRARY_DIRS ${GLFW_DIR}/lib-vc2019)
set (GLFW_LIBRARIES glfw3)

set (SXPLAYER_DIR ${EXTERNAL_DIR}/sxplayer-9.5.1)
set (SXPLAYER_INCLUDE_DIRS ${SXPLAYER_DIR})
set (SXPLAYER_LIBRARY_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build-debug/external/sxplayer-9.5.1/Debug)
set (SXPLAYER_LIBRARIES sxplayer avformat avfilter avcodec avutil pthreadVC2)

set (VULKANSDK_DIR C:/VulkanSDK/1.2.131.2)
set (VULKANSDK_INCLUDE_DIRS ${VULKANSDK_DIR}/Include)
set (VULKANSDK_LIBRARY_DIRS ${VULKANSDK_DIR}/Lib)
set (VULKANSDK_LIBRARIES vulkan-1)

set (SHADERC_DIR ${VULKANSDK_DIR})
set (SHADERC_INCLUDE_DIRS ${VULKANSDK_INCLUDE_DIRS})
set (SHADERC_LIBRARY_DIRS ${VULKANSDK_LIBRARY_DIRS})
set (SHADERC_LIBRARIES shaderc_combined)

set (GLM_INCLUDE_DIRS ${EXTERNAL_DIR}/win64/glm-0.9.9.8/glm)

set (D3DX12_INCLUDE_DIRS ${EXTERNAL_DIR}/win64/d3dx12)

else ()
find_package (PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED libavcodec libavdevice libavformat libavfilter libavutil)
pkg_check_modules(GLEW REQUIRED glew)
pkg_check_modules(GLFW REQUIRED glfw3)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    pkg_check_modules(VULKANSDK REQUIRED vulkan)
    pkg_check_modules(SHADERC REQUIRED shaderc_combined)
    set (GL_PACKAGES gl egl)
    pkg_check_modules(GL REQUIRED ${GL_PACKAGES})
    pkg_check_modules(GLM REQUIRED glm)
else()
    set (GLM_INCLUDE_DIRS /usr/local/include)
endif()

set (SXPLAYER_DIR ${EXTERNAL_DIR}/sxplayer-9.5.1)
set (SXPLAYER_INCLUDE_DIRS ${SXPLAYER_DIR})
set (SXPLAYER_LIBRARY_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build-debug/external/sxplayer-9.5.1)
set (SXPLAYER_LIBRARIES sxplayer avformat avfilter avcodec avutil pthread)

endif()

set (NGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ngfx)
set (NGFX_INCLUDE_DIRS ${NGFX_DIR} ${NGFX_DIR}/src)
set (NGFX_CFLAGS -DGLM_ENABLE_EXPERIMENTAL)
set (NGFX_LIBRARIES ngfx)

set (SHADER_TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shader-tools)
set (SHADER_TOOLS_INCLUDE_DIRS ${SHADER_TOOLS_DIR} ${SHADER_TOOLS_DIR}/src)
set (SHADER_TOOLS_LIBRARIES shader_tools)

set (JSON_DIR ${EXTERNAL_DIR}/json)
set (JSON_INCLUDE_DIRS ${JSON_DIR}/include)

#optional: enable GPU capture
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows" OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
option (ENABLE_RENDERDOC_CAPTURE "Enable RenderDoc Capture" ON)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
option (ENABLE_METAL_CAPTURE "Enable Metal Capture" ON)
endif()
if (ENABLE_RENDERDOC_CAPTURE)
    MESSAGE(STATUS "Enabling RenderDoc Capture")
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set (RENDERDOC_DIR C:/Program\ Files/RenderDoc)
        set (RENDERDOC_INCLUDE_DIRS ${RENDERDOC_DIR})
        set (RENDERDOC_LIBRARY_DIRS ${RENDERDOC_DIR})
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set (RENDERDOC_DIR /home/$ENV{USER}/Downloads/renderdoc_1.7)
        set (RENDERDOC_INCLUDE_DIRS ${RENDERDOC_DIR}/include)
        set (RENDERDOC_LIBRARY_DIRS ${RENDERDOC_DIR}/lib)
    endif()
endif()

set (STB_INCLUDE_DIRS ${EXTERNAL_DIR}/stb)

option (GRAPHICS_BACKEND_METAL "build metal backend" OFF)
option (GRAPHICS_BACKEND_DIRECT3D12 "build directx12 backend" OFF)
option (GRAPHICS_BACKEND_VULKAN "build vulkan backend" OFF)
if (NOT GRAPHICS_BACKEND_METAL AND NOT GRAPHICS_BACKEND_DIRECT3D12 AND NOT GRAPHICS_BACKEND_VULKAN)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set (GRAPHICS_BACKEND_DIRECT3D12 ON)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set (GRAPHICS_BACKEND_METAL ON)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set (GRAPHICS_BACKEND_VULKAN ON)
    endif()
endif()

if (CMAKE_GENERATOR MATCHES "Visual Studio" AND ${CMAKE_BUILD_TYPE} MATCHES "RelWithDebInfo")
MESSAGE(STATUS "Disabling Compiler Optimization")
string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
MESSAGE(STATUS "Enabling Parallel Build")
string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " /MP")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " /MP")
MESSAGE(STATUS "Enable Debug Assertions")
string(REPLACE "/DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

add_subdirectory(ngfx)
add_subdirectory(external/sxplayer-9.5.1)
add_subdirectory(libnodegl)
add_subdirectory(pyshell)
add_subdirectory(shader-tools)

function (define_custom_command name cmd working_dir)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${name}.task
  COMMAND ${cmd}
  #COMMAND touch "${CMAKE_BINARY_DIR}/${name}.task"
  WORKING_DIRECTORY ${working_dir}
  COMMENT "${name}"
  VERBATIM
)
endfunction()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    define_custom_command(build_python_env
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/linux/build_python_env.sh
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(build_python_env DEPENDS ${CMAKE_BINARY_DIR}/build_python_env.task)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    define_custom_command(build_pynodegl
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/win64/build_pynodegl.sh
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_target(build_pynodegl DEPENDS ${CMAKE_BINARY_DIR}/build_pynodegl.task)
    define_custom_command(build_python_env
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/win64/build_python_env.sh
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_target(build_python_env DEPENDS ${CMAKE_BINARY_DIR}/build_python_env.task)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    define_custom_command(build_python_env
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/macOS/build_python_env.sh
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_target(build_python_env DEPENDS ${CMAKE_BINARY_DIR}/build_python_env.task)
endif()
