add_executable(game
    game.cpp
)

target_compile_definitions(game PRIVATE
    VVE_DEFAULT_ENGINE_IMPLEMENTATION=3
    VVE_DEFAULT_ECS_IMPLEMENTATION=3
)

target_link_libraries(game PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(game)
