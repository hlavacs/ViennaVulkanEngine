add_executable(game
    game.cpp
)

target_link_libraries(game PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)
target_include_directories(game PRIVATE "${CMAKE_SOURCE_DIR}/include")

vve_set_output_dirs(game)
