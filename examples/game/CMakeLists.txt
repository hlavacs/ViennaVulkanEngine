set(TARGET game)
set(SOURCE game.cpp)
set(HEADERS game.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)
target_link_libraries (${TARGET} PUBLIC ${VULKAN}/lib/SDL2.lib)
target_link_libraries(${TARGET} PUBLIC ${ASSIMP_ZLIB})
target_link_libraries (${TARGET} PUBLIC ${ASSIMP_LIB})