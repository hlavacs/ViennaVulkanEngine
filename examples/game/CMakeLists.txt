set(TARGET game)
set(SOURCE game.cpp)
set(HEADERS )

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

#target_link_libraries (${TARGET} PUBLIC ${sdl2_BINARY_DIR}/${VULKAN_LIB}/${SDL2_LIB})
#target_link_libraries (${TARGET} PUBLIC ${sdl3_BINARY_DIR}/SDL3-static.lib)
#target_link_libraries (${TARGET} PUBLIC ${sdlmixer_BINARY_DIR}/SDL3_mixer.lib)
