add_executable(V4Tests V4Tests.cpp)
target_link_libraries(V4Tests PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)
add_test(NAME V4Tests COMMAND V4Tests)
vve_set_output_dirs(V4Tests)
