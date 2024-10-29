set(TARGET viennavulkanengine)

set(SOURCE
  ${IMGUI}/imgui.cpp
  ${IMGUI}/imgui_demo.cpp
  ${IMGUI}/imgui_draw.cpp
  ${IMGUI}/backends/imgui_impl_sdl2.cpp
  ${IMGUI}/backends/imgui_impl_vulkan.cpp
  ${IMGUI}/imgui_tables.cpp
  ${IMGUI}/imgui_widgets.cpp

  VEEngine.cpp
  VERenderer.cpp
  VERendererForward.cpp
  VERendererImgui.cpp
  VERendererVulkan.cpp
  VESceneManager.cpp
  VESystem.cpp
  VEWindow.cpp
  VEWindowSDL.cpp
  VHDevice.cpp
  )

set(HEADERS
  ${INCLUDE}/VEInclude.h
  ${INCLUDE}/VEEngine.h
  ${INCLUDE}/VERenderer.h
  ${INCLUDE}/VERendererForward.h
  ${INCLUDE}/VERendererImgui.h
  ${INCLUDE}/VERendererVulkan.h
  ${INCLUDE}/VESceneManager.h
  ${INCLUDE}/VESystem.h
  ${INCLUDE}/VEWindow.h
  ${INCLUDE}/VEWindowSDL.h
  ${INCLUDE}/VHInclude.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)
target_include_directories(${TARGET} PUBLIC ${SDL2}/include/SDL2)
target_include_directories(${TARGET} PUBLIC ${SDL2}/include/Volk)
target_include_directories(${TARGET} PUBLIC ${INCLUDE})
target_include_directories(${TARGET} PUBLIC ${IMGUI})
target_include_directories(${TARGET} PUBLIC ${IMGUI}/backends)
target_include_directories(${TARGET} PUBLIC ${GLM}/glm)
target_include_directories(${TARGET} PUBLIC ${VECS}/include)
target_include_directories(${TARGET} PUBLIC ${VTLL}/include)

target_link_libraries(${TARGET} LINK_INTERFACE_LIBRARIES $(VULKAN_SDK)/Lib/vulkan-1.lib)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
