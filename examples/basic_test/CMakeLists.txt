
set(TARGET basic_test)

set(SOURCE basic_test.cpp)

set(HEADERS basic_test.h)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries (${TARGET} PUBLIC vve)
