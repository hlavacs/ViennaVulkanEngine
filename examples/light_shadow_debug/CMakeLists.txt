add_executable(light_shadow_debug
    light_shadow_debug.cpp
)

target_link_libraries(light_shadow_debug PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(light_shadow_debug)
