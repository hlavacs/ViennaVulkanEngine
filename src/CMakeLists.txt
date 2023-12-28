set(TARGET viennavulkanengine)

set(SOURCE
  VEEngine.cpp
  VEImgui.cpp
  )

set(HEADERS
  ${INCLUDE}/VEEngine.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)
target_include_directories(${TARGET} PUBLIC ${SDL2}/include/SDL2)
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/extern/imgui)
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/extern/glm)

target_link_libraries(${TARGET} LINK_INTERFACE_LIBRARIES imgui)

target_link_libraries(${TARGET} LINK_INTERFACE_LIBRARIES $(VULKAN_SDK)/Lib/vulkan-1.lib)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Header Files"
  FILES ${HEADER_LIST})
