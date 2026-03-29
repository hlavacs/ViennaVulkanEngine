add_executable(physics
    physics.cpp
)

target_link_libraries(physics PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(physics)
