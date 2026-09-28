# Share test-only setup and capture helpers through a C++23 module.
add_library(vve_test_support STATIC)
target_compile_features(vve_test_support PUBLIC cxx_std_23)
target_sources(vve_test_support PUBLIC FILE_SET CXX_MODULES FILES support/TestSupport.ixx)
target_link_libraries(vve_test_support PUBLIC ViennaVulkanEngine::ViennaVulkanEngine Vulkan::Vulkan)

# GuiSystemTests inspects the Dear ImGui context directly.
find_package(imgui CONFIG REQUIRED)
# Buffer policy and placement tests inspect VMA allocation flags directly.
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# GPU marks tests that require a window or Vulkan device; remaining arguments are link libraries.
function(vve_add_engine_test name)
   cmake_parse_arguments(PARSE_ARGV 1 TEST "GPU" "" "")
   add_executable(${name} ${name}.cpp)
   # Keep fixtures isolated between tests and build trees.
   target_compile_definitions(${name} PRIVATE VVE_TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/tmp/${name}")
   target_link_libraries(${name} PRIVATE ViennaVulkanEngine::ViennaVulkanEngine ${TEST_UNPARSED_ARGUMENTS})
   add_test(NAME ${name} COMMAND ${name})
   if(TEST_GPU)
      set_tests_properties(${name} PROPERTIES LABELS gpu)
   endif()
   vve_set_output_dirs(${name})
endfunction()

vve_add_engine_test(HandleTests)
vve_add_engine_test(VectorTests)
vve_add_engine_test(GraphTests)
vve_add_engine_test(ECSTests)
vve_add_engine_test(VulkanDeviceRequirementsTests Vulkan::Vulkan)
vve_add_engine_test(VulkanLayerFilterTests Vulkan::Vulkan)
vve_add_engine_test(VulkanMemoryPolicyTests Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
vve_add_engine_test(RenderAcquireActionTests Vulkan::Vulkan)
vve_add_engine_test(WindowInputTests GPU)
vve_add_engine_test(WindowSystemInitTests GPU SDL3::SDL3)
vve_add_engine_test(WindowEventTests GPU SDL3::SDL3 imgui::imgui)
vve_add_engine_test(WindowSetupDefaultsTests Vulkan::Vulkan)
vve_add_engine_test(DefaultCameraControllerTests)
vve_add_engine_test(WindowOwnershipTests GPU)
vve_add_engine_test(WorldTests GPU)
vve_add_engine_test(UserSystemTests GPU)
# White-box tests import the simple implementation modules straight from the engine library; the inline
# code they instantiate (e.g. simple::Engine's destructor) calls Vulkan directly, so they link it as well.
# Tests that create windows open hidden SDL windows and use the platform video driver. SDL's offscreen driver only works
# with Vulkan drivers that support VK_EXT_headless_surface, so build_linux.sh selects it only without a display.
vve_add_engine_test(SimpleForwardRendererTests GPU Vulkan::Vulkan imgui::imgui GPUOpen::VulkanMemoryAllocator vve_test_support)
target_compile_definitions(SimpleForwardRendererTests PRIVATE
   "VVE_TEST_CRATE_TEXTURE=\"${PROJECT_SOURCE_DIR}/assets/game/crate0/diffuse.png\"")
vve_add_engine_test(RenderFrameCountTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderRuntimeUpdateTests GPU Vulkan::Vulkan)
vve_add_engine_test(RenderResizeTests GPU Vulkan::Vulkan SDL3::SDL3 vve_test_support)
vve_add_engine_test(MultiWindowRenderTests GPU Vulkan::Vulkan SDL3::SDL3 imgui::imgui vve_test_support)
vve_add_engine_test(RenderShadowOcclusionTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderTextureDedupTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderTextureOrientationTests GPU Vulkan::Vulkan SDL3::SDL3 vve_test_support)
vve_add_engine_test(RenderTexturedPlaneTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderHeightMapTests Vulkan::Vulkan)
vve_add_engine_test(VulkanTextureTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderTextureDedupTests PRIVATE
   "VVE_TEST_TEXTURE_A=\"${PROJECT_SOURCE_DIR}/assets/game/crate0/diffuse.png\""
   "VVE_TEST_TEXTURE_B=\"${PROJECT_SOURCE_DIR}/assets/game/crate1/diffuse.png\"")
vve_add_engine_test(RenderMaterialImportTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderMaterialImportTests PRIVATE
   "VVE_TEST_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import.obj\"")
vve_add_engine_test(RenderLightingCaptureTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderLightingCaptureTests PRIVATE
   "VVE_TEST_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import.obj\""
   "VVE_TEST_FLAT_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import_flat.obj\"")
vve_add_engine_test(RenderSponzaCaptureTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderSponzaCaptureTests PRIVATE
   "VVE_TEST_SPONZA_SCENE=\"${PROJECT_SOURCE_DIR}/assets/sea_keep_lonely_watcher/scene.gltf\"")
vve_add_engine_test(RenderMeshDedupTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderMeshDedupTests PRIVATE
   "VVE_TEST_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import.obj\"")
vve_add_engine_test(GuiSystemTests Vulkan::Vulkan imgui::imgui)
vve_add_engine_test(GuiCallbackErrorTests GPU Vulkan::Vulkan imgui::imgui vve_test_support)
vve_add_engine_test(GuiColorCaptureTests GPU Vulkan::Vulkan imgui::imgui vve_test_support)
vve_add_engine_test(SceneSystemTests vve_test_support)
vve_add_engine_test(SceneInstantiationTests vve_test_support)
vve_add_engine_test(SceneInstanceLifetimeTests)
vve_add_engine_test(RenderHandleUniquenessTests)
vve_add_engine_test(RenderPrimitiveSharingTests Vulkan::Vulkan)
vve_add_engine_test(SceneRemovalTests vve_test_support)
vve_add_engine_test(SceneAssetRemovalBlockedTests vve_test_support)
vve_add_engine_test(ScenePurgeUnusedAssetsTests vve_test_support)
vve_add_engine_test(SceneTransformCompositionTests vve_test_support)
vve_add_engine_test(AssetLightDataTests vve_test_support)
vve_add_engine_test(AssetCameraDataTests vve_test_support)
vve_add_engine_test(RenderLightCountTests vve_test_support)
vve_add_engine_test(RenderCameraCountTests)
vve_add_engine_test(RenderSceneLightApplyTests vve_test_support)
vve_add_engine_test(RenderSceneCameraApplyTests vve_test_support)

# Run the complete effect chain through the C++ example: the first frame creates every shader pipeline.
if(TARGET postprocessing)
   add_test(NAME PostProcessingSmokeTests COMMAND postprocessing --frames 3 --all-effects)
   set_tests_properties(PostProcessingSmokeTests PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      LABELS "gpu;example;render;postprocessing"
      RUN_SERIAL TRUE
      TIMEOUT 60
      FAIL_REGULAR_EXPRESSION "\\[postprocessing\\].*failed;Failed to create MTLLibrary")
   # Exercise the Metal shader translation path that previously failed on macOS.
   if(APPLE AND VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST)
      set_tests_properties(PostProcessingSmokeTests PROPERTIES
         ENVIRONMENT "VK_DRIVER_FILES=${VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST}")
   endif()
endif()
