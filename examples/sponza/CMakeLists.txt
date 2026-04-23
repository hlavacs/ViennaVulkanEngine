add_executable(sponza
    sponza.cpp
)

target_compile_definitions(sponza PRIVATE
    VVE_DEFAULT_ENGINE_NAMESPACE=v3
)

target_link_libraries(sponza PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(sponza)
