cmake_minimum_required (VERSION 3.5.0)
project (
  "ViennaVulkanEngine"
  VERSION 2.0.0
  DESCRIPTION "A game engine using the Vulkan API"
  LANGUAGES CXX
)

if (MSVC)
    add_compile_options(/D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
else()
    add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
endif()

# enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ) #cmake scripts for finding packages

# Doxygen
find_package(Doxygen)
if(Doxygen_FOUND)
  message(STATUS "Doxygen found, building docs")
  #add_subdirectory(docs)
else()
  message(STATUS "Doxygen not found, not building docs")
endif()

# Vulkan
find_package(Vulkan REQUIRED)
set(VULKAN $ENV{VULKAN_SDK})


# set variables for directories
set(INCLUDE ${PROJECT_SOURCE_DIR}/include)  # engine include directory

set(EXTERN ${PROJECT_SOURCE_DIR}/extern)    # external libraries
set(IMGUI ${EXTERN}/imgui)    # external libraries
set(TINYGLTF ${EXTERN}/tinygltf)    # external libraries 
set(VECS ${EXTERN}/ViennaEntityComponentSystem)    # external libraries 
set(VTLL ${EXTERN}/ViennaEntityComponentSystem/extern/ViennaTypeListLibrary)    # external libraries 


# Add directories for compiling
add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(examples)

# Testing

include(CTest)

add_subdirectory(tests)
