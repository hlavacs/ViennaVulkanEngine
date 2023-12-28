set(TARGET testlib)

set(SOURCE testlib.cpp)

set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PUBLIC ../../include)
target_include_directories (${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

target_link_libraries (${TARGET} PUBLIC ${EXTERN}/glfw/lib-vc2010-64/glfw3.lib)
target_link_libraries (${TARGET} PUBLIC ${SDL2}/lib/SDL2.lib)

add_test(NAME testlibtest COMMAND testlib) # Command can be a target
