add_library(VulkanHelpers STATIC)
add_library(vh::VulkanHelpers ALIAS VulkanHelpers)
add_library(ViennaVulkanEngine::VulkanHelpers ALIAS VulkanHelpers)

target_compile_features(VulkanHelpers PUBLIC cxx_std_23)

target_sources(VulkanHelpers
   PRIVATE
      src/Vulkan.cpp
      src/VulkanLow.cpp
   PUBLIC
      FILE_SET CXX_MODULES FILES
         VulkanLow.ixx
         Vulkan.ixx
)

target_link_libraries(VulkanHelpers
   PUBLIC
      Vulkan::Vulkan
)

vve_set_output_dirs(VulkanHelpers)
vve_mirror_target_file(VulkanHelpers "${VVE_PROJECT_ARCHIVE_OUTPUT_DIR}")
