
add_library (vve vve.cpp)

target_include_directories (vve PUBLIC ${CMAKE_HOME_DIRECTORY}/include ${VULKAN_SDK}/include)

target_compile_features(vve PUBLIC cxx_std_20)

target_link_libraries(vve LINK_INTERFACE_LIBRARIES $(VULKAN_SDK)/Lib/vulkan-1.lib)

