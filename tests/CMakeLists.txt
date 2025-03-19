set(TARGET testvve)
set(SOURCE testvve.cpp)
set(HEADERS )


add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

target_link_libraries (${TARGET} PUBLIC ${VULKAN}/lib/SDL2.lib)
#target_link_libraries (${TARGET} PUBLIC ${sdl3_BINARY_DIR}/SDL3-static.lib)
#target_link_libraries (${TARGET} PUBLIC ${sdlmixer_BINARY_DIR}/SDL3_mixer.lib)

target_link_libraries(${TARGET} PUBLIC ${ASSIMP_ZLIB})
target_link_libraries (${TARGET} PUBLIC ${ASSIMP_LIB})

add_test(NAME testvvetest COMMAND testvve) # Command can be a target
