cmake_minimum_required(VERSION 3.10)

project (ngfx)

file (GLOB_RECURSE NGFX_SOURCE_FILES src/*.cpp src/*.h)
if (GRAPHICS_BACKEND_VULKAN)
    set (WINDOW_BACKEND_GLFW 1)
    file (GLOB_RECURSE GRAPHICS_BACKEND_SOURCE_FILES porting/vulkan/*.cpp porting/vulkan/*.h)
    set (GRAPHICS_BACKEND_CFLAGS -DGRAPHICS_BACKEND_VULKAN)
    set (GRAPHICS_BACKEND_LIBS vulkan)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set (WINDOW_BACKEND_CFLAGS -DVK_USE_PLATFORM_XCB_KHR) #-DVK_USE_PLATFORM_XLIB_KHR
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set (WINDOW_BACKEND_CFLAGS -DVK_USE_PLATFORM_WIN32_KHR)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set (WINDOW_BACKEND_CFLAGS -DVK_USE_PLATFORM_METAL_EXT)
    endif()
elseif(GRAPHICS_BACKEND_DIRECT3D12)
    set (WINDOW_BACKEND_WINDOWS 1)
    file (GLOB_RECURSE GRAPHICS_BACKEND_SOURCE_FILES porting/d3d/*.cpp porting/d3d/*.h)
    set (GRAPHICS_BACKEND_CFLAGS -DGRAPHICS_BACKEND_DIRECT3D12 -DUNICODE)
    set (GRAPHICS_BACKEND_LIBS d3d12 dxgi d3dcompiler)
elseif(GRAPHICS_BACKEND_METAL)
    set (WINDOW_BACKEND_APPKIT 1)
    file (GLOB_RECURSE GRAPHICS_BACKEND_SOURCE_FILES porting/metal/*.cpp porting/metal/*.h porting/metal/*.mm)
    set (GRAPHICS_BACKEND_CFLAGS -DGRAPHICS_BACKEND_METAL)
    set (GRAPHICS_BACKEND_LIBS "-framework AppKit -framework MetalKit -framework Metal")
else()
    message (FATAL_ERROR "ERROR: graphics backend not set") 
endif()
if (WINDOW_BACKEND_GLFW)
    file (GLOB_RECURSE WINDOW_BACKEND_SOURCE_FILES porting/glfw/*.cpp porting/glfw/*.h)
    set (WINDOW_BACKEND_LIBS glfw)
    set (WINDOW_BACKEND_CFLAGS ${WINDOW_BACKEND_CFLAGS} -DWINDOW_BACKEND_GLFW)
elseif (WINDOW_BACKEND_WINDOWS)
    file (GLOB_RECURSE WINDOW_BACKEND_SOURCE_FILES porting/windows/*.cpp porting/windows/*.h)
    set (WINDOW_BACKEND_CFLAGS ${WINDOW_BACKEND_CFLAGS} -DWINDOW_BACKEND_WINDOWS)
elseif (WINDOW_BACKEND_APPKIT)
    file (GLOB_RECURSE APP_BACKEND_SOURCE_FILES porting/appkit/*)
    set (APP_BACKEND_SOURCE_FILES ${APP_BACKEND_SOURCE_FILES} PARENT_SCOPE)
    set (RESOURCE_FILES porting/appkit/Main.storyboard)
    set (WINDOW_BACKEND_CFLAGS ${WINDOW_BACKEND_CFLAGS} -DWINDOW_BACKEND_APPKIT)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NGFX_SOURCE_FILES} ${GRAPHICS_BACKEND_SOURCE_FILES} ${WINDOW_BACKEND_SOURCE_FILES})
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(NGFX_BUILD_TYPE STATIC)
else()
    set(NGFX_BUILD_TYPE SHARED)
endif()
add_library(ngfx ${NGFX_BUILD_TYPE} ${NGFX_SOURCE_FILES} ${GRAPHICS_BACKEND_SOURCE_FILES} ${WINDOW_BACKEND_SOURCE_FILES})
target_compile_definitions(ngfx PUBLIC -DGLM_ENABLE_EXPERIMENTAL -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 
    -DNGFX_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}" 
    ${GRAPHICS_BACKEND_CFLAGS} 
    ${WINDOW_BACKEND_CFLAGS}
)
target_link_libraries(ngfx ${GRAPHICS_BACKEND_LIBS} ${WINDOW_BACKEND_LIBS})
target_include_directories(ngfx PUBLIC . src
	${GLM_INCLUDE_DIRS}
	${STB_INCLUDE_DIRS}
)

if (GRAPHICS_BACKEND_DIRECT3D12)
	target_include_directories(ngfx PUBLIC . src ${D3DX12_INCLUDE_DIRS})
endif()

function(build_test name)
file (GLOB_RECURSE TEST_SOURCE_FILES test/${name}/*.cpp test/${name}/*.h test/${name}/*.mm)
if (GRAPHICS_BACKEND_METAL)
MESSAGE(STATUS "test: ${name} app backend src: ${APP_BACKEND_SOURCE_FILES}")
add_executable(${name} ${TEST_SOURCE_FILES} ${APP_BACKEND_SOURCE_FILES} ${NGFX_DIR}/porting/appkit/Main.storyboard)
set(RESOURCE_FILES ${NGFX_DIR}/porting/appkit/Info.plist ${NGFX_DIR}/porting/appkit/Main.storyboard)
set_target_properties(${name} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_FRAMEWORK_IDENTIFIER org.gopro.ngfx
    RESOURCE "${RESOURCE_FILES}"
    MACOSX_BUNDLE_INFO_PLIST ${NGFX_DIR}/porting/appkit/Info.plist
)
else()
add_executable(${name} ${TEST_SOURCE_FILES})
endif()
target_link_libraries(${name} ngfx)
endfunction()

function(build_tool name)
file (GLOB_RECURSE TOOL_SOURCE_FILES tools/${name}/*.cpp tools/${name}/*.h)
add_executable(${name} ${TOOL_SOURCE_FILES})
target_link_libraries(${name} ngfx ${GRAPHICS_BACKEND_LIBS} ${WINDOW_BACKEND_LIBS})
endfunction()

build_test(triangle)
build_test(texture)
build_test(matrixMultiply)
build_test(mesh)
build_tool(meshTool)

message(STATUS "graphics_backend_cflags: ${GRAPHICS_BACKEND_CFLAGS} window_backend_cflags: ${WINDOW_BACKEND_CFLAGS}")
set (NGFX_CFLAGS ${NGFX_CFLAGS} ${GRAPHICS_BACKEND_CFLAGS} ${WINDOW_BACKEND_CFLAGS} PARENT_SCOPE)
