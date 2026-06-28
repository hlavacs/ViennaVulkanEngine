add_executable(sponza
    sponza.cpp
)

target_link_libraries(sponza PRIVATE
    ViennaVulkanEngineSimple
    ViennaVulkanEngine::ViennaVulkanEngine
)

add_dependencies(sponza
    ViennaVulkanEngineSimple
    vve_simple_shaders
)

vve_set_output_dirs(sponza)
