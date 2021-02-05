
set(TARGET basic_test)

set(SOURCE basic_test.cpp)

set(HEADERS basic_test.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC vve)
target_link_libraries (${TARGET} PUBLIC debug ${EXTERNAL}/imgui/Debug/imgui.lib)
target_link_libraries (${TARGET} PUBLIC optimized ${EXTERNAL}/imgui/Release/imgui.lib)
target_link_libraries (${TARGET} PUBLIC ${EXTERNAL}/glfw/lib-vc2010-64/glfw3.lib)
