add_executable(testscene
    testscene.cpp
)

target_link_libraries(testscene PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    vve_example_support
)

add_dependencies(testscene
    ViennaVulkanEngine
)

vve_set_output_dirs(testscene)
