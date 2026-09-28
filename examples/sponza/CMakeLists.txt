add_executable(sponza
    sponza.cpp
)

target_link_libraries(sponza PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    vve_example_support
)

add_dependencies(sponza
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(sponza)
