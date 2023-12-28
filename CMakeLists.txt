cmake_minimum_required (VERSION 3.5.0)
project (
  ViennaVulkanEngine
  VERSION 0.1
  DESCRIPTION "A game engine using the Vulkan API"
  LANGUAGES CXX)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  set(CMAKE_CXX_EXTENSIONS OFF)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  include(CTest)

  find_package(Doxygen)
  if(Doxygen_FOUND)
    message(STATUS "Doxygen found, building docs")
    add_subdirectory(docs)
  else()
    message(STATUS "Doxygen not found, not building docs")
  endif()
endif()

find_package(Vulkan REQUIRED)
set(SDL2 $ENV{VULKAN_SDK})
set(INCLUDE ${CMAKE_HOME_DIRECTORY}/include)
set(EXTERN ${CMAKE_HOME_DIRECTORY}/extern)

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(examples)

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MODERN_CMAKE_BUILD_TESTING)
   AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
