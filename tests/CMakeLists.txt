# Share test-only setup and capture helpers through a C++23 module.
add_library(vve_test_support STATIC)
target_compile_features(vve_test_support PUBLIC cxx_std_23)
target_sources(vve_test_support PUBLIC FILE_SET CXX_MODULES FILES support/TestSupport.ixx)
target_link_libraries(vve_test_support PUBLIC ViennaVulkanEngine::ViennaVulkanEngine Vulkan::Vulkan)

# SDL/ImGui headers come from the engine; linking their static archives here duplicates runtime state.
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
vve_add_engine_test(AudioSystemTests)
vve_add_engine_test(SpatialAudioTests)
set_tests_properties(SpatialAudioTests PROPERTIES LABELS audio TIMEOUT 15)
vve_add_engine_test(LegacyMusicTests)
target_compile_definitions(LegacyMusicTests PRIVATE
    VVE_TEST_V2_MUSIC_DIR="${PROJECT_SOURCE_DIR}/assets/sounds")
set_tests_properties(LegacyMusicTests PROPERTIES
    ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" LABELS audio TIMEOUT 30)
set_tests_properties(AudioSystemTests PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" LABELS audio TIMEOUT 15)
add_test(NAME AudioInitFailureTests COMMAND AudioSystemTests --expect-init-failure)
set_tests_properties(AudioInitFailureTests PROPERTIES
    ENVIRONMENT "SDL_AUDIO_DRIVER=vve_missing_audio_driver" LABELS audio TIMEOUT 15)
vve_add_engine_test(VectorTests)
vve_add_engine_test(GraphTests)
vve_add_engine_test(ECSTests)
if(TARGET vve_example_support)
   vve_add_engine_test(ExampleSupportTests vve_example_support)
endif()
if(TARGET island_delivery_logic)
   vve_add_engine_test(IslandDeliveryLogicTests island_delivery_logic)
   vve_add_engine_test(IslandDeliverySoundTests island_delivery_game)
   vve_add_engine_test(IslandDeliverySceneTests island_delivery_game)
   vve_add_engine_test(IslandDeliveryMapTests island_delivery_game)
   vve_add_engine_test(IslandDeliveryMotionTests island_delivery_game)
   vve_add_engine_test(IslandDeliveryMarkingTests GPU island_delivery_game vve_test_support)
   add_test(NAME IslandDeliveryMarkingTestsDeferred COMMAND IslandDeliveryMarkingTests)
   set_tests_properties(IslandDeliveryMarkingTests IslandDeliveryMarkingTestsDeferred PROPERTIES
       LABELS "gpu;example;island_delivery" TIMEOUT 180 RUN_SERIAL TRUE)
   set_tests_properties(IslandDeliveryMarkingTests PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
   set_tests_properties(IslandDeliveryMarkingTestsDeferred PROPERTIES
       ENVIRONMENT "SDL_AUDIO_DRIVER=dummy;VVE_TEST_RENDERER=deferred")
   target_compile_definitions(IslandDeliverySoundTests PRIVATE VVE_ISLAND_ASSET_DIR="${VVE_ISLAND_ASSET_DIR}")
   target_compile_definitions(IslandDeliverySceneTests PRIVATE VVE_ISLAND_ASSET_DIR="${VVE_ISLAND_ASSET_DIR}")
   set_tests_properties(IslandDeliverySoundTests PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" LABELS audio TIMEOUT 30)
endif()
vve_add_engine_test(VulkanDeviceRequirementsTests Vulkan::Vulkan)
vve_add_engine_test(VulkanLayerFilterTests Vulkan::Vulkan)
vve_add_engine_test(VulkanMemoryPolicyTests Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
vve_add_engine_test(RenderAcquireActionTests Vulkan::Vulkan)
vve_add_engine_test(WindowInputTests GPU)
vve_add_engine_test(WindowSystemInitTests GPU)
vve_add_engine_test(WindowEventTests GPU)
vve_add_engine_test(WindowMouseButtonTests GPU)
vve_add_engine_test(WindowFullscreenTests GPU)
vve_add_engine_test(WindowSetupDefaultsTests Vulkan::Vulkan)
vve_add_engine_test(DefaultCameraControllerTests)
vve_add_engine_test(WindowOwnershipTests GPU)
vve_add_engine_test(WorldTests GPU)
vve_add_engine_test(UserSystemTests GPU)
vve_add_engine_test(EngineBuilderTests)
# White-box tests import the simple implementation modules straight from the engine library; the inline
# code they instantiate (e.g. simple::Engine's destructor) calls Vulkan directly, so they link it as well.
# Tests that create windows open hidden SDL windows and use the platform video driver. SDL's offscreen driver only works
# with Vulkan drivers that support VK_EXT_headless_surface, so build_linux.sh selects it only without a display.
vve_add_engine_test(SimpleForwardRendererTests GPU Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator vve_test_support)
target_compile_definitions(SimpleForwardRendererTests PRIVATE
   "VVE_TEST_CRATE_TEXTURE=\"${PROJECT_SOURCE_DIR}/assets/game/crate0/diffuse.png\"")
vve_add_engine_test(RenderFrameCountTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderRuntimeUpdateTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderResizeTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderAntialiasingTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderEnvironmentTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(MultiWindowRenderTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderShadowOcclusionTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderTextureDedupTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderTextureCapacityTests)
vve_add_engine_test(RenderTextureOrientationTests GPU Vulkan::Vulkan vve_test_support)
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
vve_add_engine_test(RenderMaterialLightingTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderLightingCaptureTests PRIVATE
   "VVE_TEST_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import.obj\""
   "VVE_TEST_FLAT_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import_flat.obj\"")
vve_add_engine_test(RenderSponzaCaptureTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderSponzaCaptureTests PRIVATE
   "VVE_TEST_SPONZA_SCENE=\"${PROJECT_SOURCE_DIR}/assets/sea_keep_lonely_watcher/scene.gltf\"")
# The example camera must frame the castle and retain its long clipping range across updates.
vve_add_engine_test(SponzaCameraTests)
target_compile_definitions(SponzaCameraTests PRIVATE
   "VVE_TEST_SPONZA_SCENE=\"${PROJECT_SOURCE_DIR}/assets/sea_keep_lonely_watcher/scene.gltf\"")
vve_add_engine_test(RenderMeshDedupTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderMeshDedupTests PRIVATE
   "VVE_TEST_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import.obj\"")
vve_add_engine_test(GuiSystemTests Vulkan::Vulkan)
vve_add_engine_test(GuiCallbackErrorTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(GuiColorCaptureTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(GuiImageCaptureTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(GuiImageTests)
vve_add_engine_test(SceneSystemTests vve_test_support)
vve_add_engine_test(SceneInstantiationTests vve_test_support)
vve_add_engine_test(SceneInstanceLifetimeTests)
vve_add_engine_test(RenderHandleUniquenessTests)
vve_add_engine_test(RenderPrimitiveSharingTests Vulkan::Vulkan)
vve_add_engine_test(RenderTransparencyTests Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderObjectLookTests vve_test_support)
vve_add_engine_test(RenderShapeTests Vulkan::Vulkan)
vve_add_engine_test(RenderPickingTests)
vve_add_engine_test(RenderScreenRayTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderTransparencyCaptureTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderBackgroundColorTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderSkyboxTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderSpriteTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(SceneRemovalTests vve_test_support)
vve_add_engine_test(SceneAssetRemovalBlockedTests vve_test_support)
vve_add_engine_test(AssetSceneUnloadTests Vulkan::Vulkan vve_test_support)
vve_add_engine_test(ScenePurgeUnusedAssetsTests vve_test_support)
vve_add_engine_test(SceneTransformCompositionTests vve_test_support)
vve_add_engine_test(SceneHierarchyTests vve_test_support)
vve_add_engine_test(SceneTreeTests vve_test_support)
vve_add_engine_test(AssetLightDataTests vve_test_support)
vve_add_engine_test(AssetCameraDataTests vve_test_support)
vve_add_engine_test(RenderLightCountTests vve_test_support)
vve_add_engine_test(RenderCameraCountTests)
vve_add_engine_test(RenderSceneLightApplyTests vve_test_support)
vve_add_engine_test(RenderSceneCameraApplyTests vve_test_support)

# One build contains both backends; facade contracts and paired captures choose them at runtime.
vve_add_engine_test(RendererBackendTests GPU)
vve_add_engine_test(DeferredRendererTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(DeferredRendererTests PRIVATE
   "VVE_TEST_MATERIAL_MODEL=\"${PROJECT_SOURCE_DIR}/tests/assets/render_material_import.obj\"")
add_test(NAME DeferredRendererMsaaTests COMMAND DeferredRendererTests --msaa4)
set_tests_properties(RendererBackendTests DeferredRendererTests DeferredRendererMsaaTests PROPERTIES
   LABELS "gpu;deferred" TIMEOUT 180 RUN_SERIAL TRUE)
# Debug builds name G-buffer images, shadow maps and attachments and label every pass for RenderDoc; Release must not.
vve_add_engine_test(RenderDebugLabelsTests GPU Vulkan::Vulkan vve_test_support)
set_tests_properties(RenderDebugLabelsTests PROPERTIES LABELS "gpu;deferred" TIMEOUT 120)
vve_add_engine_test(RenderSkyTests GPU Vulkan::Vulkan vve_test_support)
set_tests_properties(RenderSkyTests PROPERTIES LABELS "gpu;deferred" TIMEOUT 600)

# Khronos Profiles overrides actual Vulkan queries, without an engine test hook. Missing SDK layers leave this check unregistered.
# Schema: KhronosGroup/Vulkan-Profiles; settings: KhronosGroup/Vulkan-Utility-Libraries/docs/layer_configuration.md.
find_path(VVE_TEST_PROFILES_LAYER_DIR VkLayer_khronos_profiles.json
   PATHS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d" "${VVE_VULKAN_SDK_ROOT}/Bin" NO_DEFAULT_PATH)
if(VVE_TEST_PROFILES_LAYER_DIR)
   add_test(NAME RendererBackendUnsupportedTests COMMAND RendererBackendTests --expect-unsupported)
   set_tests_properties(RendererBackendUnsupportedTests PROPERTIES
      ENVIRONMENT "VK_LOADER_LAYERS_ENABLE=*validation,*profiles;VK_KHRONOS_PROFILES_PROFILE_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/assets;VK_KHRONOS_PROFILES_PROFILE_NAME=VP_VVE_forward_only;VK_KHRONOS_PROFILES_SIMULATE_CAPABILITIES=SIMULATE_PROPERTIES_BIT;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=false"
      ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR}"
      LABELS "gpu;deferred;capabilities" RUN_SERIAL TRUE TIMEOUT 60
      FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endif()

# Reuse the existing assertions and assets against deferred rendering, with serialized fixture ownership.
foreach(renderer_regression IN ITEMS
   RenderFrameCountTests RenderRuntimeUpdateTests RenderResizeTests RenderAntialiasingTests
   RenderEnvironmentTests MultiWindowRenderTests RenderShadowOcclusionTests
   RenderTextureDedupTests RenderTextureOrientationTests RenderTexturedPlaneTests VulkanTextureTests
   RenderMaterialImportTests RenderLightingCaptureTests RenderMaterialLightingTests RenderSponzaCaptureTests
   RenderMeshDedupTests GuiCallbackErrorTests GuiColorCaptureTests GuiImageCaptureTests
   RenderScreenRayTests RenderTransparencyCaptureTests RenderBackgroundColorTests RenderSkyboxTests RenderSpriteTests)
   add_test(NAME ${renderer_regression}Deferred COMMAND ${renderer_regression})
   set_tests_properties(${renderer_regression}Deferred PROPERTIES
      ENVIRONMENT "VVE_TEST_RENDERER=deferred" LABELS "gpu;deferred" RUN_SERIAL TRUE TIMEOUT 180)
endforeach()

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
