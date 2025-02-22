cmake_minimum_required (VERSION 3.5.0)
project (
  "ViennaVulkanEngine"
  VERSION 2.0.0
  DESCRIPTION "A game engine using the Vulkan API"
  LANGUAGES CXX
)

#set(CMAKE_BUILD_TYPE Debug) #use for MSVC debug

if (MSVC)
  add_compile_options(/DIMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(/D_CRT_SECURE_NO_WARNINGS) #for assimp
  add_compile_options(/DASSIMP_BUILD_ZLIB=ON) #for assimp
else()
  add_compile_options(-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
	add_compile_options(-DASSIMP_BUILD_ZLIB=ON) #for assimp
endif()

# enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Doxygen
#find_package(Doxygen)
#if(Doxygen_FOUND)
#  message(STATUS "Doxygen found, building docs")
  #add_subdirectory(docs)
#else()
#  message(STATUS "Doxygen not found, not building docs")
#endif()

# Vulkan
find_package(Vulkan REQUIRED)
set(VULKAN $ENV{VULKAN_SDK})

# set variables for directories
set(BUILD ${PROJECT_SOURCE_DIR}/build)  # engine include directory
set(INCLUDE ${PROJECT_SOURCE_DIR}/include)  # engine include directory
set(EXTERN ${PROJECT_SOURCE_DIR}/extern)    # external libraries


include(FetchContent) # once in the project to include the module

FetchContent_Declare(viennastrongtype
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaStrongType.git 
                     GIT_TAG main
)
FetchContent_MakeAvailable(viennastrongtype)

FetchContent_Declare(viennatypelistlibrary
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaTypeListLibrary.git 
                     GIT_TAG main
)
FetchContent_MakeAvailable(viennatypelistlibrary)

FetchContent_Declare(viennaentitycomponentsystem
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaEntityComponentSystem.git
                     GIT_TAG main
)
FetchContent_MakeAvailable(viennaentitycomponentsystem)

FetchContent_Declare(assimp
                     GIT_REPOSITORY https://github.com/assimp/assimp.git 
                     GIT_TAG v5.3.1
)

set(BUILD_SHARED_LIBS OFF)
#set(ASSIMP_BUILD_DOCS ON)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Assimp)

set(zlib_BINARY_DIR ${assimp_BINARY_DIR}/contrib/zlib)    # external libraries 

FetchContent_Declare(imgui
                     GIT_REPOSITORY https://github.com/ocornut/imgui.git 
                     GIT_TAG v1.89.9
                     GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(Imgui)

set(VECS_SOURCE_DIR   ${viennaentitycomponentsystem_SOURCE_DIR})    # external libraries 
set(VTLL_SOURCE_DIR   ${viennatypelistlibrary_SOURCE_DIR})    # external libraries 
set(VSTY_SOURCE_DIR   ${viennastrongtype_SOURCE_DIR})    # external libraries 

message(STATUS "IMGUI: ${imgui_SOURCE_DIR}")
message(STATUS "ASSIMP: ${assimp_SOURCE_DIR}")
message(STATUS "ASSIMP: ${assimp_BINARY_DIR}")
message(STATUS "ZLIB: ${zlib_SOURCE_DIR}")
message(STATUS "ZLIB: ${zlib_BINARY_DIR}")
message(STATUS "VECS: ${VECS_SOURCE_DIR}")
message(STATUS "VTLL: ${VTLL_SOURCE_DIR}")
message(STATUS "VSTY: ${VSTY_SOURCE_DIR}")


# Include Assimp
#add_subdirectory(${ASSIMP} ${ASSIMP} EXCLUDE_FROM_ALL)

# Add directories for compiling
#add_subdirectory(extern)
add_subdirectory(src)
#add_subdirectory(examples)

# Testing
#include(CTest)
#add_subdirectory(tests)
