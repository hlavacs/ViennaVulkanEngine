set(TARGET viennavulkanengine)

set(SOURCE
  VEEngine.cpp
  VEImgui.cpp
  )

set(HEADERS
  ${INCLUDE}/ViennaVulkanEngine/VEEngine.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC ../include/ViennaVulkanEngine)
target_include_directories(${TARGET} PUBLIC ../extern/imgui)
target_include_directories(${TARGET} PUBLIC ../extern/sdl2/include)
target_include_directories(${TARGET} PUBLIC ../extern/glfw/include)
target_include_directories(${TARGET} PUBLIC ../extern/glm)

target_link_libraries(${TARGET} LINK_INTERFACE_LIBRARIES $(VULKAN_SDK)/Lib/vulkan-1.lib)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
