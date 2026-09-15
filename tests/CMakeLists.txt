function(vve_add_engine_test name)
   add_executable(${name} ${name}.cpp)
   target_link_libraries(${name} PRIVATE ViennaVulkanEngine::ViennaVulkanEngine ${ARGN})
   add_test(NAME ${name} COMMAND ${name})
   vve_set_output_dirs(${name})
endfunction()

vve_add_engine_test(HandleTests)
vve_add_engine_test(SegmentedVectorTests)
vve_add_engine_test(ECSTests)
vve_add_engine_test(WindowInputTests)
vve_add_engine_test(DefaultCameraControllerTests)
vve_add_engine_test(WindowOwnershipTests)
vve_add_engine_test(WorldTests)
vve_add_engine_test(UserSystemTests)
# White-box tests import the simple implementation modules straight from the engine library; the inline
# code they instantiate (e.g. simple::Engine's destructor) calls Vulkan directly, so they link it as well.
vve_add_engine_test(SimpleForwardRendererTests Vulkan::Vulkan)
target_compile_definitions(SimpleForwardRendererTests PRIVATE
   "VVE_TEST_CRATE_TEXTURE=\"${PROJECT_SOURCE_DIR}/assets/game/crate0/diffuse.png\"")
vve_add_engine_test(GuiSystemTests Vulkan::Vulkan)
vve_add_engine_test(SceneSystemTests)
vve_add_engine_test(SceneInstantiationTests)
vve_add_engine_test(SceneRemovalTests)
vve_add_engine_test(SceneAssetRemovalBlockedTests)
vve_add_engine_test(ScenePurgeUnusedAssetsTests)
vve_add_engine_test(SceneTransformCompositionTests)
vve_add_engine_test(AssetLightDataTests)
vve_add_engine_test(AssetCameraDataTests)
vve_add_engine_test(RenderLightCountTests)
vve_add_engine_test(RenderCameraCountTests)
vve_add_engine_test(RenderSceneLightApplyTests)
vve_add_engine_test(RenderSceneCameraApplyTests)

# Run the complete effect chain through the C++ example: the first frame creates every shader pipeline.
if(TARGET postprocessing)
   add_test(NAME PostProcessingSmokeTests COMMAND postprocessing --frames 3)
   set_tests_properties(PostProcessingSmokeTests PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      LABELS "render;postprocessing"
      RUN_SERIAL TRUE
      TIMEOUT 60
      PASS_REGULAR_EXPRESSION "\\[postprocessing\\] frames=3[\r\n]"
      FAIL_REGULAR_EXPRESSION "\\[postprocessing\\].*failed;Failed to create MTLLibrary")
   # Exercise the Metal shader translation path that previously failed on macOS.
   if(APPLE AND VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST)
      set_tests_properties(PostProcessingSmokeTests PROPERTIES
         ENVIRONMENT "VK_DRIVER_FILES=${VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST}")
   endif()
endif()
