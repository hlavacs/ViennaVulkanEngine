add_executable(physics
    physics.cpp
)

target_link_libraries(physics PRIVATE
    ViennaVulkanEngineSimple
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(physics
    ViennaVulkanEngineSimple
    vve_simple_shaders
)

vve_set_output_dirs(physics)
