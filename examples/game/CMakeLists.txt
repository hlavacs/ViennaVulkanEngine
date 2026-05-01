add_executable(game
    game.cpp
)

target_link_libraries(game PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(game)
