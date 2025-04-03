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
  VERendererForward11.cpp
  VERendererShadow11.cpp
  VERendererImgui.cpp
  VERendererVulkan.cpp
  VESceneManager.cpp
  VEAssetManager.cpp
  VESoundManager.cpp
  VESystem.cpp
  VEWindow.cpp
  VEWindowSDL.cpp
  VHBuffer.cpp
  VHImage.cpp
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
  ${INCLUDE}/VERendererForward11.h
  ${INCLUDE}/VERendererShadow11.h
  ${INCLUDE}/VERendererImgui.h
  ${INCLUDE}/VERendererVulkan.h
  ${INCLUDE}/VESceneManager.h
  ${INCLUDE}/VEAssetManager.h
  ${INCLUDE}/VESoundManager.h
  ${INCLUDE}/VESystem.h
  ${INCLUDE}/VEWindow.h
  ${INCLUDE}/VEWindowSDL.h
  ${INCLUDE}/VHInclude.h
  ${INCLUDE}/VHBuffer.h
  ${INCLUDE}/VHImage.h
  ${INCLUDE}/VHVulkan.h
  ${INCLUDE}/VHDevice.h
  ${INCLUDE}/VHRender.h
  ${INCLUDE}/VHRender.h
  ${INCLUDE}/VHCommand.h
  ${INCLUDE}/VHSync.h
)

add_library (${TARGET} STATIC ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})

target_link_libraries(${TARGET} SDL2 assimp vk-bootstrap volk)

