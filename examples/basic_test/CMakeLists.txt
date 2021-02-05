
add_executable(basic_test basic_test.cpp)

target_link_libraries (basic_test PUBLIC vve)

target_include_directories (basic_test PUBLIC ${CMAKE_HOME_DIRECTORY}/include ${VULKAN_SDK}/include)

target_compile_features(basic_test PUBLIC cxx_std_20)

