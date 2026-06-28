add_library(ViennaVulkanEngineSimple STATIC EXCLUDE_FROM_ALL)

target_compile_features(ViennaVulkanEngineSimple PUBLIC cxx_std_23)

target_link_libraries(ViennaVulkanEngineSimple PUBLIC ViennaVulkanEngine)

target_sources(ViennaVulkanEngineSimple
   PUBLIC
      FILE_SET CXX_MODULES FILES
         Assets.ixx
         ECS.ixx
         Engine.ixx
         GUI.ixx
         Graph.ixx
         Handle.ixx
         Math.ixx
         SceneGraph.ixx
         Vector.ixx
         Window.ixx
         WindowSystem.ixx
         World.ixx
)
