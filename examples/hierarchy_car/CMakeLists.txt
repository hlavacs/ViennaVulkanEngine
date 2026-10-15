add_executable(hierarchy_car
    hierarchy_car.cpp
)

target_link_libraries(hierarchy_car PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    vve_example_support
)

add_dependencies(hierarchy_car
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(hierarchy_car)
