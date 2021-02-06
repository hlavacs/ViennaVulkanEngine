
cmake_minimum_required (VERSION 3.0.0)
project (VVE)

if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE /O2)
  add_compile_options(/await)
  set(CMAKE_VS_PLATFORM_NAME "x64")
  add_compile_options(/EHsc)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  #clang++ *.cpp -g -std=c++20 -stdlib=libc++ -pthread -fcoroutines-ts -o a.out
  add_compile_options(-stdlib=libc++)
  add_compile_options(-pthread)
  add_compile_options(-fcoroutines-ts)
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin)
set(INCLUDE ${CMAKE_HOME_DIRECTORY}/include)
set(EXTERNAL ${CMAKE_HOME_DIRECTORY}/external)

include_directories (${INCLUDE})
include_directories (${CMAKE_HOME_DIRECTORY}/external/glfw/include)
include_directories (${CMAKE_HOME_DIRECTORY}/external/imgui)
include_directories (${CMAKE_HOME_DIRECTORY}/external/glm)
include_directories ($ENV{VULKAN_SDK}/include)

add_subdirectory (src)
add_subdirectory (external/imgui)
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
