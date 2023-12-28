set(TARGET basic_test)

set(SOURCE basic_test.cpp)

set(HEADERS basic_test.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)

target_link_libraries (${TARGET} PUBLIC imgui)
target_link_libraries (${TARGET} PUBLIC ${EXTERN}/glfw/lib-vc2010-64/glfw3.lib)
target_link_libraries (${TARGET} PUBLIC ${EXTERN}/sdl2/lib/x64/SDL2.lib)

#add_test(NAME testlibtest COMMAND testlib) # Command can be a target
