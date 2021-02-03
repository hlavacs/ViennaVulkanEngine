
cmake_minimum_required (VERSION 3.17.1)
project (VVE)

add_subdirectory (src)
add_subdirectory (examples/basic_test)

add_executable(basic_test basic_test.cpp)

