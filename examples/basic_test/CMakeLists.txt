
add_executable(basic_test basic_test.cpp)

target_link_libraries (basic_test LINK_PUBLIC vve)

target_compile_features(basic_test PUBLIC cxx_std_20)
