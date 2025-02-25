set(TARGET helper)
set(SOURCE helper.cpp)
set(HEADERS helper.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PUBLIC ${INCLUDE} ${GLM}/glm)
target_include_directories (${TARGET} PUBLIC ${VULKAN}/include)
target_include_directories(${TARGET} PUBLIC ${VULKAN}/Include/SDL2)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)
target_link_libraries (${TARGET} PUBLIC ${VULKAN}/lib/SDL2.lib)
