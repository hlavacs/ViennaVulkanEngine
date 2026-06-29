add_executable(physics
    physics.cpp
)

target_link_libraries(physics PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(physics
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(physics)
