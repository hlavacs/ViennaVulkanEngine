add_executable(light_shadow_debug
    light_shadow_debug.cpp
)

target_link_libraries(light_shadow_debug PRIVATE
    ViennaVulkanEngineSimple
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(light_shadow_debug
    ViennaVulkanEngineSimple
    vve_simple_shaders
)

vve_set_output_dirs(light_shadow_debug)

if(BUILD_TESTING AND VVE_ENGINE_IMPLEMENTATION_NAMESPACE STREQUAL "v5")
    add_test(NAME LightShadowDebugExample
        COMMAND light_shadow_debug --skip-gpu-asserts --output "$<TARGET_FILE_DIR:light_shadow_debug>/light_shadow_debug.txt")
endif()
