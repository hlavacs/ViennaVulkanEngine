set(TARGET testvve)

set(SOURCE testvve.cpp)

set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_include_directories (${TARGET} PUBLIC ${INCLUDE})
target_include_directories (${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

target_link_libraries (${TARGET} PUBLIC ${SDL2}/lib/SDL2.lib)

add_test(NAME testvvetest COMMAND testvve) # Command can be a target
