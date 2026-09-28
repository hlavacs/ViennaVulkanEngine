find_package(imgui CONFIG REQUIRED)

add_executable(postprocessing
    postprocessing.cpp
)

target_link_libraries(postprocessing PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    vve_example_support
    imgui::imgui
)

add_dependencies(postprocessing
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(postprocessing)
