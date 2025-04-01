set(TARGET helper)
set(SOURCE helper.cpp)
set(HEADERS helper.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC viennavulkanengine)


