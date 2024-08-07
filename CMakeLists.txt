cmake_minimum_required (VERSION 3.5.0)
project (
  "ViennaVulkanEngine"
  VERSION 2.0.0
  DESCRIPTION "A game engine using the Vulkan API"
  LANGUAGES CXX
)

# enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ) #cmake scripts for finding packages

# Doxygen
find_package(Doxygen)
if(Doxygen_FOUND)
  message(STATUS "Doxygen found, building docs")
  add_subdirectory(docs)
else()
  message(STATUS "Doxygen not found, not building docs")
endif()

# Vulkan
find_package(Vulkan REQUIRED)
set(SDL2 $ENV{VULKAN_SDK})


# set variables for directories
set(INCLUDE ${PROJECT_SOURCE_DIR}/include)  # engine include directory
set(EXTERN ${PROJECT_SOURCE_DIR}/extern)    # external libraries
set(IMGUI ${EXTERN}/imgui)    # external libraries
set(GLM ${EXTERN}/glm)    # external libraries


# Add directories for compiling
add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(examples)

# Testing

include(CTest)

#cmake_policy(SET CMP0135 NEW) #force time stamping of downloads to avoid warnings

#include(FetchContent)
#FetchContent_Declare(
#  googletest
#  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
#)
# For Windows: Prevent overriding the parent project's compiler/linker settings
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#FetchContent_MakeAvailable(googletest)

#set(CMAKE_CXX_EXTENSIONS OFF)
#set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MODERN_CMAKE_BUILD_TESTING)
   AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
