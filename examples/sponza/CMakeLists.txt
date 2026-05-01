add_executable(sponza
    sponza.cpp
)

target_link_libraries(sponza PRIVATE ViennaVulkanEngine::ViennaVulkanEngine)

vve_set_output_dirs(sponza)
