find_package(imgui CONFIG REQUIRED)

add_executable(testscene
    testscene.cpp
)

target_link_libraries(testscene PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    imgui::imgui
)

add_dependencies(testscene
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(testscene)
