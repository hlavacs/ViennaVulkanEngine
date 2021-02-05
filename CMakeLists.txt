
cmake_minimum_required (VERSION 3.0.0)
project (VVE)

if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE /O2)
  add_compile_options(/await)
  add_compile_options(/EHsc)
  set(CMAKE_VS_PLATFORM_NAME "x64")
endif(MSVC)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin)
set(INCLUDE ${CMAKE_HOME_DIRECTORY}/include)
include_directories (${INCLUDE})
include_directories ($ENV{VULKAN_SDK}/include)
include_directories ($ENV{VULKAN_SDK}/include/vulkan)

add_subdirectory (src)
add_subdirectory (examples/basic_test)

find_package(Doxygen)
if (DOXYGEN_FOUND)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "README.md")
  doxygen_add_docs(doxygen
    ${CMAKE_SOURCE_DIR}/README.md
    ${CMAKE_SOURCE_DIR}/include
	  ${CMAKE_SOURCE_DIR}/src
  )
endif(DOXYGEN_FOUND)
