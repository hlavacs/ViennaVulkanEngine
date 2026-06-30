find_package(imgui CONFIG REQUIRED)

add_executable(game
    game.cpp
)

target_link_libraries(game PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
    imgui::imgui
)

add_dependencies(game
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(game)
