cmake_minimum_required(VERSION 3.14)
project (pyshell)

file (GLOB_RECURSE PYSHELL_SRC_FILES *.c *.h)

add_executable(pyshell ${PYSHELL_SRC_FILES})
target_include_directories(pyshell PUBLIC ${PYTHON_INCLUDE_DIRS})
target_link_libraries(pyshell PUBLIC ${PYTHON_LIBRARIES})
target_link_directories(pyshell PUBLIC ${PYTHON_LIBRARY_DIRS})
