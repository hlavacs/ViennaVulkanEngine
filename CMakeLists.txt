
cmake_minimum_required (VERSION 3.0.0)
project (Minimal)

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

include_directories (${CMAKE_HOME_DIRECTORY}/external/glm)
include_directories ($ENV{VULKAN_SDK}/include)
