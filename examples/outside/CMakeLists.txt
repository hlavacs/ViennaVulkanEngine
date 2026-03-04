cmake_minimum_required (VERSION 3.15.0)
project (
  "My Game"
  VERSION 2.0.0
  DESCRIPTION "My game"
  LANGUAGES CXX
)

set(TARGET game)
set(SOURCE game.cpp)
set(HEADERS )

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

set(VVE ${PROJECT_SOURCE_DIR}/../ViennaVulkanEngine)
set(DEPS ${VVE}/build/_deps)
set(FFMPEG ../ffmpeg)
set(FFMPEG_BIN_DIR ${FFMPEG}/bin)
file(GLOB FFMPEG_DLLS
  "${FFMPEG_BIN_DIR}/*.dll"
)

include_directories(${VVE}/include)
include_directories(${DEPS}/stb_image-src)
include_directories(${DEPS}/l2dfiledialog-src)
include_directories(${DEPS}/viennaentitycomponentsystem-src/include)
include_directories(${DEPS}/viennastrongtype-src/include)
include_directories(${DEPS}/viennatypelistlibrary-src/include)
include_directories(${DEPS}/sdl3-src/include)
include_directories(${DEPS}/sdlmixer-src/include)
include_directories(${DEPS}/assimp-src/include)
include_directories(${DEPS}/assimp-build/include)
include_directories(${DEPS}/imgui-src)
include_directories(${DEPS}/imgui-src/backends)
include_directories(${DEPS}/glm-src)
include_directories(${DEPS}/vkbootstrap-src/src)
include_directories(${FFMPEG}/include)

link_directories(${VVE}/build/src/$<CONFIG>)
link_directories(${DEPS}/assimp-build/lib/$<CONFIG>)
link_directories(${DEPS}/assimp-build/contrib/zlib/$<CONFIG>)
link_directories(${DEPS}/vkbootstrap-build/$<CONFIG>)
link_directories(${DEPS}/sdl3-build/$<CONFIG>)
link_directories(${DEPS}/sdlmixer-build/$<CONFIG>)

link_directories(${FFMPEG}/lib)


add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries(${TARGET} PUBLIC viennavulkanengined SDL3_test SDL3-static SDL3_mixer-static assimp-vc143-mtd zlibstaticd vk-bootstrapd volk avcodec avutil avformat swscale)

if(WIN32)
  target_link_libraries(${TARGET} PUBLIC
    winmm
    setupapi
    cfgmgr32
    version
  )
endif()


if(MSVC)
  add_custom_command(TARGET ${TARGET} PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${DEPS}/sdl3-build/$<CONFIG>/SDL3-static.pdb"
      "${CMAKE_BINARY_DIR}/$<CONFIG>/SDL3-static.pdb"
  )

  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${FFMPEG_DLLS}
      $<TARGET_FILE_DIR:${TARGET}>
  )
endif()


