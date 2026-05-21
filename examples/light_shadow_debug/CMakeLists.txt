add_executable(light_shadow_debug
    light_shadow_debug.cpp
)

target_link_libraries(light_shadow_debug PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(light_shadow_debug)

if(BUILD_TESTING AND VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "v4")
    add_test(NAME LightShadowDebugExample
        COMMAND light_shadow_debug --output "$<TARGET_FILE_DIR:light_shadow_debug>/light_shadow_debug.txt")
endif()
