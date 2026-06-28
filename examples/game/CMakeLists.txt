add_executable(game
    game.cpp
)

target_link_libraries(game PRIVATE
    ViennaVulkanEngineSimple
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(game
    ViennaVulkanEngineSimple
    vve_simple_shaders
)

vve_set_output_dirs(game)
