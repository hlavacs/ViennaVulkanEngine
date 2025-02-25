set(TARGET viennavulkanengine)

set(SOURCE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp

  VEEngine.cpp
  VEGUI.cpp
  VERenderer.cpp
  VERendererForward.cpp
  VERendererImgui.cpp
  VERendererVulkan.cpp
  VESceneManager.cpp
  VEAssetManager.cpp
  VESystem.cpp
  VEWindow.cpp
  VEWindowSDL.cpp
  VHBUffer.cpp
  VHVulkan.cpp
  VHDevice.cpp
  VHRender.cpp
  VHCommand.cpp
  VHSync.cpp
  )

set(HEADERS
  ${INCLUDE}/VEInclude.h
  ${INCLUDE}/VEEngine.h
  ${INCLUDE}/VEGUI.h
  ${INCLUDE}/VERenderer.h
  ${INCLUDE}/VERendererForward.h
  ${INCLUDE}/VERendererImgui.h
  ${INCLUDE}/VERendererVulkan.h
  ${INCLUDE}/VESceneManager.h
  ${INCLUDE}/VEAssetManager.h
  ${INCLUDE}/VESystem.h
  ${INCLUDE}/VEWindow.h
  ${INCLUDE}/VEWindowSDL.h
  ${INCLUDE}/VHInclude.h
  ${INCLUDE}/VHBuffer.h
  ${INCLUDE}/VHVulkan.h
  ${INCLUDE}/VHDevice.h
  ${INCLUDE}/VHRender.h
  ${INCLUDE}/VHRender.h
  ${INCLUDE}/VHCommand.h
  ${INCLUDE}/VHSync.h
  
)

add_library (${TARGET} STATIC ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/SDL2)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/Volk)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/glm)
target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/Include/vma)
target_include_directories(${TARGET} PUBLIC ${INCLUDE})
#target_include_directories(${TARGET} PUBLIC ${EXTERN})
target_include_directories(${TARGET} PUBLIC ${stb_image_SOURCE_DIR})
target_include_directories(${TARGET} PUBLIC ${assimp_SOURCE_DIR})
target_include_directories(${TARGET} PUBLIC ${assimp_SOURCE_DIR}/include)
target_include_directories(${TARGET} PUBLIC ${assimp_BINARY_DIR}/include)
target_include_directories(${TARGET} PUBLIC ${imgui_SOURCE_DIR})
target_include_directories(${TARGET} PUBLIC ${imgui_SOURCE_DIR}/backends)
target_include_directories(${TARGET} PUBLIC ${GLM}/glm)
target_include_directories(${TARGET} PUBLIC ${VECS_SOURCE_DIR}/include)
target_include_directories(${TARGET} PUBLIC ${VTLL_SOURCE_DIR}/include)
target_include_directories(${TARGET} PUBLIC ${VSTY_SOURCE_DIR}/include)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
