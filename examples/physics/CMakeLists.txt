add_executable(physics
    physics.cpp
)

target_link_libraries(physics PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)
target_include_directories(physics PRIVATE "${CMAKE_SOURCE_DIR}/include")

vve_set_output_dirs(physics)
