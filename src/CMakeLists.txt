
add_library (vve vve.cpp)

target_include_directories (vve PUBLIC ${CMAKE_HOME_DIRECTORY}/include)

target_compile_features(vve PUBLIC cxx_std_20)

