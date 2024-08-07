
set(TARGET basic_test)

set(SOURCE basic_test.cpp)

set(HEADERS basic_test.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PUBLIC ${INCLUDE} ${EXTERN}/glm/glm)
target_include_directories (${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

target_link_libraries (${TARGET} PUBLIC ${SDL2}/lib/SDL2.lib)
