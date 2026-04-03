add_executable(physics
    physics.cpp
)

target_compile_definitions(physics PRIVATE
    VVE_DEFAULT_ENGINE_IMPLEMENTATION=3
    VVE_DEFAULT_ECS_IMPLEMENTATION=3
)

target_link_libraries(physics PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(physics)
