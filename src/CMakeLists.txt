add_library(ViennaVulkanEngine SHARED)
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_23)
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_ENGINE_BUILD)
target_compile_options(ViennaVulkanEngine PUBLIC
    "$<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++2c>"
)

target_sources(ViennaVulkanEngine
    PRIVATE
        Engine.cpp
        versions/v3/EngineV3.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            Engine.ixx
            versions/v3/EngineV3.ixx
)

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        Vulkan::Vulkan
)

vve_set_output_dirs(ViennaVulkanEngine)
