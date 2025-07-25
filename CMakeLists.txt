cmake_minimum_required (VERSION 3.8.0)
project (
  "ViennaVulkanEngine"
  VERSION 2.0.0
  DESCRIPTION "A game engine using the Vulkan API"
  LANGUAGES CXX
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  	add_compile_options(/D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(/D_CRT_SECURE_NO_WARNINGS) #for assimp
	add_compile_options(/EHsc /DEBUG /Zi /Istb) #for assimp
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
	add_compile_options(-fexperimental-library)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
endif()


# enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# engine include directory 
set(INCLUDE ${PROJECT_SOURCE_DIR}/include)
include_directories(${INCLUDE})

# Vulkan SDK
find_package(Vulkan REQUIRED)
set(VULKAN_SDK $ENV{VULKAN_SDK})

if (WIN32)
	set(VULKAN_INCLUDE Include)
	set(VULKAN_LIB Lib)
elseif (APPLE)
	set(VULKAN_INCLUDE include)
    set(VULKAN_LIB lib)
elseif (UNIX AND NOT APPLE)
	set(VULKAN_INCLUDE include)
	set(VULKAN_LIB lib)
endif()

include_directories(${VULKAN_SDK}/${VULKAN_INCLUDE})
include_directories(${VULKAN_SDK}/${VULKAN_INCLUDE}/volk)
include_directories(${VULKAN_SDK}/${VULKAN_INCLUDE}/vma)
link_directories(${VULKAN_SDK}/lib)

# Fetch GitHub projects and set variables for include directories
include(FetchContent) # enable Fetch Content

#--------------------------------- Fetch stb image ---------------------------------#

FetchContent_Declare(stb_image
                     GIT_REPOSITORY https://github.com/nothings/stb.git
                     GIT_TAG master
)
FetchContent_MakeAvailable(stb_image)
include_directories (${stb_image_SOURCE_DIR})

#--------------------------------- Fetch file dialog ---------------------------------#

FetchContent_Declare(l2dfiledialog
                     GIT_REPOSITORY https://github.com/Limeoats/L2DFileDialog.git
                     GIT_TAG master
)
FetchContent_MakeAvailable(l2dfiledialog)
include_directories (${l2dfiledialog_SOURCE_DIR}/L2DFileDialog/src)

#--------------------------------- Fetch Vienna Entity Component System ---------------------------------#

FetchContent_Declare(viennaentitycomponentsystem
                     GIT_REPOSITORY https://github.com/hlavacs/ViennaEntityComponentSystem.git
                     GIT_TAG main
)
FetchContent_MakeAvailable(viennaentitycomponentsystem)
FetchContent_MakeAvailable(viennastrongtype)
FetchContent_MakeAvailable(viennatypelistlibrary)
include_directories (${viennaentitycomponentsystem_SOURCE_DIR}/include)
include_directories (${viennastrongtype_SOURCE_DIR}/include)
include_directories (${viennatypelistlibrary_SOURCE_DIR}/include)

#--------------------------------- Fetch Vienna Vulkan Helper ---------------------------------#

#FetchContent_Declare(viennavulkanhelper
#                     GIT_REPOSITORY https://github.com/hlavacs/ViennaVulkanHelper.git
#                     GIT_TAG main
#)
#FetchContent_MakeAvailable(viennavulkanhelper)
#include_directories (${viennavulkanhelper_SOURCE_DIR}/include)

#--------------------------------- Fetch SDL3 ---------------------------------#

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
FetchContent_Declare(sdl3
					GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                    GIT_TAG main
)
FetchContent_MakeAvailable(sdl3)
include_directories (${sdl3_SOURCE_DIR}/include)
include_directories (${sdl3_SOURCE_DIR}/include/SDL3)

#--------------------------------- Fetch SDL Mixer ---------------------------------#

set(SDL3_DIR ${SDL3_BINARY_DIR})
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(sdlmixer
                    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
                    GIT_TAG b3a6fa8
)
FetchContent_MakeAvailable(sdlmixer)
include_directories (${sdlmixer_SOURCE_DIR}/include)

#--------------------------------- Fetch Assimp ---------------------------------#

FetchContent_Declare(assimp
                     GIT_REPOSITORY https://github.com/assimp/assimp.git 
                     GIT_TAG master
)
set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)
include_directories (${assimp_SOURCE_DIR}/include)

message("COMPILER: ${CMAKE_CXX_COMPILER_ID}")
message("BUILD TYPE: ${CMAKE_BUILD_TYPE}")

#--------------------------------- Fetch ImGui ---------------------------------#

FetchContent_Declare(imgui
                     GIT_REPOSITORY https://github.com/ocornut/imgui.git 
                     GIT_TAG master)
FetchContent_MakeAvailable(Imgui)
include_directories (${imgui_SOURCE_DIR})
include_directories(${imgui_SOURCE_DIR}/backends)

#--------------------------------- Fetch glm ---------------------------------#

FetchContent_Declare(glm
                     GIT_REPOSITORY https://github.com/g-truc/glm.git 
                     GIT_TAG master)
FetchContent_MakeAvailable(glm)
include_directories(${glm_SOURCE_DIR}/src)

#--------------------------------- Fetch vk-bootstrap ---------------------------------#

FetchContent_Declare(vkbootstrap
                     GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git 
                     GIT_TAG main)
FetchContent_MakeAvailable(vkbootstrap)
include_directories(${vkbootstrap_SOURCE_DIR}/src)

#--------------------------------- Summary ---------------------------------#

message(STATUS "VECS: ${viennaentitycomponentsystem_SOURCE_DIR}")
message(STATUS "VTLL: ${viennatypelistlibrary_SOURCE_DIR}")
message(STATUS "VSTY: ${viennastrongtype_SOURCE_DIR}")
message(STATUS "STB IMAGE: ${stb_image_SOURCE_DIR}")
message(STATUS "FILE DIALOG: ${l2dfiledialog_SOURCE_DIR}")
message(STATUS "IMGUI: ${imgui_SOURCE_DIR}")
message(STATUS "SDL3 SRC: ${sdl3_SOURCE_DIR}")
message(STATUS "SDL3 BINARY: ${sdl3_BINARY_DIR}")
message(STATUS "SDL Mixer SRC: ${sdlmixer_SOURCE_DIR}")
message(STATUS "ASSIMP SRC: ${assimp_SOURCE_DIR}")
message(STATUS "ASSIMP BIN: ${assimp_BINARY_DIR}")
message(STATUS "ZLIB SRC: ${zlib_SOURCE_DIR}")
message(STATUS "ZLIB BIN: ${zlib_BINARY_DIR}")
message(STATUS "VKBOOTSTRAP: ${vkbootstrap_SOURCE_DIR}")
message(STATUS "GLM: ${glm_SOURCE_DIR}")

# Add directories for compiling
add_subdirectory(src)
add_subdirectory(examples)

# Testing
include(CTest)
add_subdirectory(tests)

# set the game as the default startup project in VS
if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT game)
endif()

# Doxygen
find_package(Doxygen)

if(DOXYGEN_FOUND)

	set(DOXYGEN_DIRS
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/include
		${viennaentitycomponentsystem_SOURCE_DIR}/include
		${viennatypelistlibrary_SOURCE_DIR}/include
		${viennastrongtype_SOURCE_DIR}/include
	)

	string(REPLACE ";" " " DOXYGEN_DIRS_STR "${DOXYGEN_DIRS} ")  
	set(DOXYGEN_INPUT_DIR ${DOXYGEN_DIRS_STR})
	set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")
	set(DOXYGEN_EXCLUDE_DIR "${viennaentitycomponentsystem_SOURCE_DIR}/include/old")
	set(DOXYGEN_CONFIG_FILE "${CMAKE_BINARY_DIR}/Doxyfile")

	configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${DOXYGEN_CONFIG_FILE} @ONLY)

	doxygen_add_docs(docs
		${DOXYGEN_INPUT_DIR}
		COMMENT "Generate MyProject documentation"
	)
endif()
