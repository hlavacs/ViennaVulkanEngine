add_executable(game
    game.cpp
)

target_link_libraries(game PRIVATE
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(game
    ViennaVulkanEngine
    vve_simple_shaders
)

vve_set_output_dirs(game)
