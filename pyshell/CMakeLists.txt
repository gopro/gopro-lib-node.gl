cmake_minimum_required(VERSION 3.14)
project (pyshell)

file (GLOB_RECURSE PYSHELL_SRC_FILES *.c *.h)

add_executable(pyshell ${PYSHELL_SRC_FILES})
target_include_directories(pyshell PUBLIC ${Python3_INCLUDE_DIRS})
target_link_libraries(pyshell PUBLIC ${Python3_LIBRARIES})
target_link_directories(pyshell PUBLIC ${Python3_LIBRARY_DIRS})
