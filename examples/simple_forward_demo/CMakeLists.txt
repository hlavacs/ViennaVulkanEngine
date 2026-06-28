add_executable(simple_forward_demo
    simple_forward_demo.cpp
)

target_link_libraries(simple_forward_demo PRIVATE
    ViennaVulkanEngineSimple
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(simple_forward_demo
    ViennaVulkanEngineSimple
    vve_simple_shaders
)

vve_set_output_dirs(simple_forward_demo)
