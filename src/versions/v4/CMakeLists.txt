target_sources(ViennaVulkanEngine
   PUBLIC
      FILE_SET CXX_MODULES FILES
         Error.ixx
         Math.ixx
         Handle.ixx
         Vector.ixx
         ECS.ixx
         Types.ixx
         Window.ixx
         World.ixx
         Assets.ixx
         Resources.ixx
         Shaders.ixx
         RenderPass.ixx
         RendererForward.ixx
         RenderSystem.ixx
         Gui.ixx
         Engine.ixx
)

set(VVE_V4_SHADER_FILES
   "${CMAKE_CURRENT_LIST_DIR}/shaders/Lighting.slang"
   "${CMAKE_CURRENT_LIST_DIR}/shaders/Forward.slang"
)
set_source_files_properties(${VVE_V4_SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(ViennaVulkanEngine PRIVATE ${VVE_V4_SHADER_FILES})

add_custom_target(vve_v4_shaders ALL
   COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${CMAKE_BINARY_DIR}/bin/${VVE_OUTPUT_CONFIG_DIR}/shaders/v4"
   COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${CMAKE_SOURCE_DIR}/bin/${VVE_PROJECT_CONFIG_DIR}/shaders/v4"
   COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      ${VVE_V4_SHADER_FILES}
      "${CMAKE_BINARY_DIR}/bin/${VVE_OUTPUT_CONFIG_DIR}/shaders/v4"
   COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      ${VVE_V4_SHADER_FILES}
      "${CMAKE_SOURCE_DIR}/bin/${VVE_PROJECT_CONFIG_DIR}/shaders/v4"
   COMMAND_EXPAND_LISTS
   SOURCES ${VVE_V4_SHADER_FILES}
)
