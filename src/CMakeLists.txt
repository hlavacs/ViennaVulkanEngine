add_library(ViennaVulkanEngine SHARED)
add_library(ViennaVulkanEngine::ViennaVulkanEngine ALIAS ViennaVulkanEngine)

target_compile_features(ViennaVulkanEngine PUBLIC cxx_std_20)
target_compile_definitions(ViennaVulkanEngine PRIVATE VVE_ENGINE_BUILD)

target_sources(ViennaVulkanEngine
    PRIVATE
        VEEngine.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            VEEngine.ixx
)

target_include_directories(ViennaVulkanEngine PUBLIC "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(ViennaVulkanEngine
    PUBLIC
        Vulkan::Vulkan
)

vve_set_output_dirs(ViennaVulkanEngine)
