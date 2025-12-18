set(TARGET viennavulkanengine)

set(SOURCE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp

  VEEngine.cpp
  VEGUI.cpp
  VERenderer.cpp
  VERendererForward.cpp
  VERendererForward11.cpp
  VERendererDeferred.cpp
  VERendererDeferredCommon.cpp
  VERendererDeferred11.cpp
  VERendererDeferred13.cpp
  VERendererShadow11.cpp
  VERendererImgui.cpp
  VERendererVulkan.cpp
  VESceneManager.cpp
  VEAssetManager.cpp
  #VESoundManagerSDL2.cpp
  VESoundManagerSDL3.cpp
  VESystem.cpp
  VEWindow.cpp
  VEWindowSDL.cpp

  PathTracing/buffer.cpp
  PathTracing/command_manager_impl.cpp
  PathTracing/descriptor_sets.cpp
  PathTracing/descriptor_sets_RT.cpp
  PathTracing/descriptor_sets_targets.cpp
  PathTracing/helper_structs.cpp
  PathTracing/image.cpp
  PathTracing/material_manager.cpp
  PathTracing/object_manager.cpp
  PathTracing/texture_manager.cpp  
  PathTracing/vrt_renderer.cpp  
  )

set(HEADERS
  ${INCLUDE}/VEInclude.h
  ${INCLUDE}/VEEngine.h
  ${INCLUDE}/VEGUI.h
  ${INCLUDE}/VERenderer.h
  ${INCLUDE}/VERendererForward.h
  ${INCLUDE}/VERendererForward11.h
  ${INCLUDE}/VERendererDeferred.h
  ${INCLUDE}/VERendererDeferredCommon.h
  ${INCLUDE}/VERendererDeferred11.h
  ${INCLUDE}/VERendererDeferred13.h
  ${INCLUDE}/VERendererShadow11.h
  ${INCLUDE}/VERendererImgui.h
  ${INCLUDE}/VERendererVulkan.h
  ${INCLUDE}/VESceneManager.h
  ${INCLUDE}/VEAssetManager.h
  #${INCLUDE}/VESoundManagerSDL2.h
  ${INCLUDE}/VESoundManagerSDL3.h
  ${INCLUDE}/VESystem.h
  ${INCLUDE}/VEWindow.h
  ${INCLUDE}/VEWindowSDL.h

  ${INCLUDE}/PathTracing/buffer.h
  ${INCLUDE}/PathTracing/command_manager.h
  ${INCLUDE}/PathTracing/descriptor_sets.h
  ${INCLUDE}/PathTracing/descriptor_sets_RT.h
  ${INCLUDE}/PathTracing/descriptor_sets_targets.h
  ${INCLUDE}/PathTracing/helper_structs.h
  ${INCLUDE}/PathTracing/image.h
  ${INCLUDE}/PathTracing/light_manager.h
  ${INCLUDE}/PathTracing/material_manager.h
  ${INCLUDE}/PathTracing/object_manager.h
  ${INCLUDE}/PathTracing/pipline_rasterized.h
  ${INCLUDE}/PathTracing/pipline_raytraced.h
  ${INCLUDE}/PathTracing/render_target.h
  ${INCLUDE}/PathTracing/swapchain.h
  ${INCLUDE}/PathTracing/texture_manager.h
  ${INCLUDE}/PathTracing/vrt_renderer.h
)

add_library (${TARGET} STATIC ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

# Build in parallel under Visual Studio. Not enabled by default.
if(MSVC AND NOT USE_CLANG)
  target_compile_options(${TARGET} PRIVATE "/MP")
endif()

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})

target_link_libraries(${TARGET} SDL3::SDL3 SDL3_mixer::SDL3_mixer assimp vk-bootstrap volk)

