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

  list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

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

find_package(assimp REQUIRED)
if(assimp_FOUND)
  message(STATUS "Assimp found at ${ASSIMP_ROOT_DIR}")
else()
  message(STATUS "Assimp not found - clone it to the parent directory and build it yourself!")
endif()
#set(ASSIMP ${PROJECT_SOURCE_DIR}/../assimp)   # you have to clone assimp and build it yourself

set(INCLUDE ${PROJECT_SOURCE_DIR}/include)
set(EXTERN ${PROJECT_SOURCE_DIR}/extern)

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(examples)

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MODERN_CMAKE_BUILD_TESTING)
   AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
