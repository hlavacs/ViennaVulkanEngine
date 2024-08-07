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
  VEImgui.cpp
  )

set(HEADERS
  ${INCLUDE}/VEEngine.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)
target_include_directories(${TARGET} PUBLIC ${SDL2}/include/SDL2)
target_include_directories(${TARGET} PUBLIC ${INCLUDE})
target_include_directories(${TARGET} PUBLIC ${IMGUI})
target_include_directories(${TARGET} PUBLIC ${IMGUI}/backends)
target_include_directories(${TARGET} PUBLIC ${GLM}/glm)

target_link_libraries(${TARGET} LINK_INTERFACE_LIBRARIES $(VULKAN_SDK)/Lib/vulkan-1.lib)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
