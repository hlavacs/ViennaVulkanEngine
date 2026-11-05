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
# IQ-00 freezes Linux Debug; other platform/configuration references belong to IQ-10.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
   # Restore the original sealed IQ-00 captures, including legacy UNORM; never render replacement references.
   set(iq_reference_archive "${CMAKE_CURRENT_SOURCE_DIR}/assets/image-quality-reference.tar.gz")
   set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${iq_reference_archive}")
   file(SHA256 "${iq_reference_archive}" iq_reference_sha256)
   if(NOT iq_reference_sha256 STREQUAL "fe430213fbbf3fe0c00780b22248e0698c5fae3283421a94c5db5f9cfa2d78ed")
      message(FATAL_ERROR "The sealed IQ-00 reference archive changed; restore the original fixture.")
   endif()
   if(NOT EXISTS "${CMAKE_BINARY_DIR}/verify/image-quality/baseline/manifest.txt")
      file(ARCHIVE_EXTRACT INPUT "${iq_reference_archive}" DESTINATION "${CMAKE_BINARY_DIR}/verify/image-quality")
   endif()
   # Exact historical pixels belong to llvmpipe/Mesa 25.2.8; hardware coverage uses the other renderer tests.
   find_file(VVE_IQ_BASELINE_ICD NAMES lvp_icd.json lvp_icd.x86_64.json
      PATHS /usr/share/vulkan/icd.d /usr/local/share/vulkan/icd.d)
   vve_add_engine_test(RenderImageQualityBaselineTests GPU Vulkan::Vulkan vve_test_support)
   target_compile_definitions(RenderImageQualityBaselineTests PRIVATE
      VVE_IQ_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
      VVE_IQ_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
      VVE_IQ_BASELINE_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/baseline")
   set_tests_properties(RenderImageQualityBaselineTests PROPERTIES LABELS "gpu;image_quality" TIMEOUT 900 RUN_SERIAL TRUE
      FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
   if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
      set_tests_properties(RenderImageQualityBaselineTests PROPERTIES ENVIRONMENT_MODIFICATION
         "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d;VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1")
   endif()
endif()

vve_add_engine_test(RenderEnvironmentTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderTerrainTests GPU Vulkan::Vulkan vve_test_support)
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
vve_add_engine_test(RenderToneMappingTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderToneMappingMatrixTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderPostOutputLifecycleTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderPostOutputWindowsTests GPU Vulkan::Vulkan vve_test_support)
# Exercise restore recovery even when the desktop immediately honours ordinary activation requests.
add_test(NAME RenderPostOutputWindowsRestoreRecoveryTests COMMAND RenderPostOutputWindowsTests --force-remap)
target_compile_definitions(RenderPostOutputWindowsTests PRIVATE VVE_POST_OUTPUT_WINDOWS_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/lifecycle-windows")
set_tests_properties(RenderPostOutputWindowsTests RenderPostOutputWindowsRestoreRecoveryTests PROPERTIES LABELS "gpu;image_quality" TIMEOUT 600 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
   ENVIRONMENT_MODIFICATION "VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG")
target_compile_definitions(RenderPostOutputLifecycleTests PRIVATE VVE_POST_OUTPUT_LIFECYCLE_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/lifecycle")
set_tests_properties(RenderPostOutputLifecycleTests PROPERTIES LABELS "gpu;image_quality" TIMEOUT 600 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
   ENVIRONMENT_MODIFICATION "VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG")
if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   set_property(TEST RenderPostOutputLifecycleTests RenderPostOutputWindowsTests APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
endif()
target_compile_definitions(RenderToneMappingMatrixTests PRIVATE VVE_TONEMAP_MATRIX_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/tone-mapping-b")
set_tests_properties(RenderToneMappingMatrixTests PROPERTIES LABELS "gpu;image_quality" TIMEOUT 600 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   set_tests_properties(RenderToneMappingMatrixTests PROPERTIES ENVIRONMENT_MODIFICATION
      "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d;VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG")
endif()
target_compile_definitions(RenderToneMappingTests PRIVATE VVE_TONEMAP_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/tone-mapping")
set_tests_properties(RenderToneMappingTests PROPERTIES LABELS "gpu;image_quality" TIMEOUT 300 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   set_tests_properties(RenderToneMappingTests PROPERTIES ENVIRONMENT_MODIFICATION
      "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d;VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG")
endif()
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
# GI selection, disabled equivalence and real raster-probe transport share the normal native graphics workflow.
vve_add_engine_test(HybridGiBackendTests GPU vve_test_support)
vve_add_engine_test(HybridGiEquivalenceTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiBounceTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiSettingsTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiPerfTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(RenderOutputTimingTests GPU Vulkan::Vulkan vve_test_support)
set_tests_properties(RenderOutputTimingTests PROPERTIES LABELS "gpu;perf" TIMEOUT 120 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
set_tests_properties(HybridGiPerfTests PROPERTIES LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
vve_add_engine_test(HybridGiSkyTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiLeakTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiDynamicsTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiAoTests GPU Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator vve_test_support)
vve_add_engine_test(HybridGiMaterialTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiTransparencyTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiCascadeTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiProceduralTests GPU Vulkan::Vulkan vve_test_support)
vve_add_engine_test(HybridGiLifecycleTests GPU Vulkan::Vulkan vve_test_support)
set(VVE_GI_LIFECYCLE_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp/HybridGiLifecycleTests" CACHE PATH "GI lifecycle diagnostic captures")
target_compile_definitions(HybridGiLifecycleTests PRIVATE VVE_GI_LIFECYCLE_REPORT_DIR="${VVE_GI_LIFECYCLE_REPORT_DIR}")
set_tests_properties(HybridGiLifecycleTests PROPERTIES LABELS "gpu;gi" TIMEOUT 180 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
foreach(lifecycle_case IN ITEMS msaa4 quality budget cycles failure scene-native)
   add_test(NAME HybridGiLifecycle-${lifecycle_case} COMMAND HybridGiLifecycleTests ${lifecycle_case})
   set_tests_properties(HybridGiLifecycle-${lifecycle_case} PROPERTIES LABELS "gpu;gi" TIMEOUT 180 RUN_SERIAL TRUE
      FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
   list(APPEND gi_lifecycle_cases HybridGiLifecycle-${lifecycle_case})
endforeach()
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   set_tests_properties(HybridGiLifecycleTests ${gi_lifecycle_cases} PROPERTIES ENVIRONMENT_MODIFICATION
      "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d;VK_LAYER_VALIDATE_SYNC=set:1")
endif()
# Compile application-owned stages with the same Slang embedding path as the renderer, including only public sampling code.
set(native_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/native-shaders")
get_target_property(native_slangc ViennaVulkanEngine VVE_SLANGC)
set(VVE_NATIVE_TEST_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp/HybridGiProceduralTests" CACHE PATH "Native-draw captures and optional pre-change PNG references")
target_compile_definitions(HybridGiProceduralTests PRIVATE "VVE_NATIVE_TEST_REPORT_DIR=\"${VVE_NATIVE_TEST_REPORT_DIR}\"")
foreach(native_entry IN ITEMS vertexMain gbufferMain captureMain plainMain forwardMain unlitMain)
   set(native_stage fragment)
   if(native_entry STREQUAL vertexMain)
      set(native_stage vertex)
   endif()
   set(native_header "${native_shader_dir}/${native_entry}.h")
   add_custom_command(OUTPUT "${native_header}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${native_shader_dir}"
      COMMAND "${native_slangc}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/native_procedural.slang"
         -I "${PROJECT_SOURCE_DIR}/src/shaders" -target spirv -profile spirv_1_4 -stage ${native_stage}
         -entry ${native_entry} -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name native_${native_entry} -o "${native_header}"
      DEPENDS assets/native_procedural.slang "${PROJECT_SOURCE_DIR}/src/shaders/vve_irradiance.slang" "${PROJECT_SOURCE_DIR}/src/shaders/vve_irradiance.h"
      VERBATIM)
   list(APPEND native_headers "${native_header}")
endforeach()
add_custom_target(vve_native_test_shaders DEPENDS ${native_headers})
add_dependencies(HybridGiProceduralTests vve_native_test_shaders)
target_include_directories(HybridGiProceduralTests PRIVATE "${native_shader_dir}")
vve_add_engine_test(RenderAntialiasingQualityTests GPU Vulkan::Vulkan vve_test_support)
add_dependencies(RenderAntialiasingQualityTests vve_native_test_shaders)
target_include_directories(RenderAntialiasingQualityTests PRIVATE "${native_shader_dir}")
target_compile_definitions(RenderAntialiasingQualityTests PRIVATE VVE_AA_QUALITY_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/msaa")
set_tests_properties(RenderAntialiasingQualityTests PROPERTIES LABELS "gpu;image_quality" TIMEOUT 300 RUN_SERIAL TRUE
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
   ENVIRONMENT_MODIFICATION "VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG")
if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   set_property(TEST RenderAntialiasingQualityTests APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
endif()
set_tests_properties(HybridGiProceduralTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
# Pixel comparison boundary/negative controls run without initializing a window or Vulkan device.
add_test(NAME HybridGiProcedural-image-comparison COMMAND HybridGiProceduralTests image-comparison)
set_tests_properties(HybridGiProcedural-image-comparison PROPERTIES LABELS "gi;cpu" TIMEOUT 15)
foreach(native_case IN ITEMS procedural-bounce procedural-blocker procedural-shadow receive forward-sample none-registered)
   add_test(NAME HybridGiProcedural-${native_case} COMMAND HybridGiProceduralTests ${native_case})
   set_tests_properties(HybridGiProcedural-${native_case} PROPERTIES
      LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
   list(APPEND gi_native_cases HybridGiProcedural-${native_case})
endforeach()
# Release has no engine debug messenger: require the real external layer instead of accepting an unvalidated pass.
set_tests_properties(HybridGiProceduralTests ${gi_native_cases} PROPERTIES
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
   ENVIRONMENT_MODIFICATION "VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1")
if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   set_property(TEST HybridGiProceduralTests ${gi_native_cases} APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
endif()
set_tests_properties(HybridGiCascadeTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
foreach(cascade_case IN ITEMS coarse-bounce continuity coarse-scroll teleport memory memory-low)
   add_test(NAME HybridGiCascade-${cascade_case} COMMAND HybridGiCascadeTests ${cascade_case})
   set_tests_properties(HybridGiCascade-${cascade_case} PROPERTIES
      LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endforeach()
set_tests_properties(HybridGiMaterialTests HybridGiTransparencyTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
foreach(material_case IN ITEMS cutout-msaa4 double-sided metal strength srgb)
   add_test(NAME HybridGiMaterial-${material_case} COMMAND HybridGiMaterialTests ${material_case})
   set_tests_properties(HybridGiMaterial-${material_case} PROPERTIES
      LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endforeach()
foreach(transparent_case IN ITEMS color-msaa4 blocking ordering tonemap)
   add_test(NAME HybridGiTransparency-${transparent_case} COMMAND HybridGiTransparencyTests ${transparent_case})
   set_tests_properties(HybridGiTransparency-${transparent_case} PROPERTIES
      LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endforeach()
set_tests_properties(HybridGiAoTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
foreach(ao_case IN ITEMS msaa4 halo halo-msaa4 material resize toggle probes)
   add_test(NAME HybridGiAo-${ao_case} COMMAND HybridGiAoTests ${ao_case})
   set_tests_properties(HybridGiAo-${ao_case} PROPERTIES
      LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endforeach()
set_tests_properties(HybridGiDynamicsTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
foreach(gi_event IN ITEMS sun-low scroll scroll-low sky move teleport replacement)
   add_test(NAME HybridGiDynamics-${gi_event} COMMAND HybridGiDynamicsTests ${gi_event})
   set_tests_properties(HybridGiDynamics-${gi_event} PROPERTIES
      LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endforeach()
add_test(NAME HybridGiInsideGeometryTests COMMAND HybridGiLeakTests --inside)
add_test(NAME HybridGiEmissiveTests COMMAND HybridGiSkyTests --emissive)
add_test(NAME HybridGiOffCameraTests COMMAND HybridGiBounceTests --off-camera)
add_test(NAME HybridGiBounceMsaaTests COMMAND HybridGiBounceTests --msaa4)
set_tests_properties(HybridGiBounceTests HybridGiBounceMsaaTests HybridGiOffCameraTests HybridGiSettingsTests HybridGiSkyTests HybridGiEmissiveTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
set_tests_properties(HybridGiLeakTests HybridGiInsideGeometryTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 240 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
add_test(NAME HybridGiEquivalenceMsaaTests COMMAND HybridGiEquivalenceTests --msaa4)
set_tests_properties(HybridGiBackendTests HybridGiEquivalenceTests HybridGiEquivalenceMsaaTests PROPERTIES
   LABELS "gpu;gi" TIMEOUT 180 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
if(TARGET simple_forward_demo)
   add_test(NAME HybridGiExampleTests COMMAND simple_forward_demo --renderer hybrid-gi --frames 3
      --output "${CMAKE_CURRENT_BINARY_DIR}/tmp/HybridGiExampleTests/capture.png")
   set_tests_properties(HybridGiExampleTests PROPERTIES
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "gpu;gi;example" TIMEOUT 60 RUN_SERIAL TRUE
      PASS_REGULAR_EXPRESSION "renderer=hybrid-gi gbuffer=1 deferred_lighting=1"
      FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;simple_forward_demo failed")
endif()
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
   foreach(mode Unorm NoMutable UnormNoMutable)
      if(mode STREQUAL "Unorm")
         set(output_option --unorm)
      elseif(mode STREQUAL "NoMutable")
         set(output_option --no-mutable)
      else()
         set(output_option --unorm-no-mutable)
      endif()
      set(name RenderPostOutputWindows${mode}Tests)
      add_test(NAME ${name} COMMAND RenderPostOutputWindowsTests ${output_option})
      set_tests_properties(${name} PROPERTIES LABELS "gpu;image_quality" TIMEOUT 600 RUN_SERIAL TRUE
         FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
         ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR};VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation:VK_LAYER_KHRONOS_profiles;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=set:false")
      if(mode MATCHES "Unorm")
         set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "VK_KHRONOS_PROFILES_EXCLUDE_FORMATS=set:VK_FORMAT_B8G8R8A8_SRGB")
      endif()
      if(mode MATCHES "NoMutable")
         set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "VK_KHRONOS_PROFILES_EXCLUDE_DEVICE_EXTENSIONS=set:VK_KHR_swapchain_mutable_format")
      endif()
   endforeach()
   add_test(NAME RenderToneMappingUnormTests COMMAND RenderToneMappingTests --unorm)
   add_test(NAME RenderToneMappingNoMutableTests COMMAND RenderToneMappingTests --no-mutable)
   add_test(NAME RenderToneMappingMatrixUnormTests COMMAND RenderToneMappingMatrixTests --unorm)
   add_test(NAME RenderToneMappingMatrixNoMutableTests COMMAND RenderToneMappingMatrixTests --no-mutable)
   foreach(name RenderToneMappingMatrixUnormTests RenderToneMappingMatrixNoMutableTests)
      set_tests_properties(${name} PROPERTIES LABELS "gpu;image_quality" TIMEOUT 600 RUN_SERIAL TRUE
         FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
         ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR};VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation:VK_LAYER_KHRONOS_profiles;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=set:false")
   endforeach()
   set_property(TEST RenderToneMappingMatrixUnormTests APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_KHRONOS_PROFILES_EXCLUDE_FORMATS=set:VK_FORMAT_B8G8R8A8_SRGB")
   set_property(TEST RenderToneMappingMatrixNoMutableTests APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_KHRONOS_PROFILES_EXCLUDE_DEVICE_EXTENSIONS=set:VK_KHR_swapchain_mutable_format")
   set_tests_properties(RenderToneMappingUnormTests RenderToneMappingNoMutableTests PROPERTIES
      LABELS "gpu;image_quality" TIMEOUT 300 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
   foreach(name RenderToneMappingUnormTests RenderToneMappingNoMutableTests)
      set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION
         "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR};VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation:VK_LAYER_KHRONOS_profiles;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=set:false")
   endforeach()
   set_property(TEST RenderToneMappingUnormTests APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_KHRONOS_PROFILES_EXCLUDE_FORMATS=set:VK_FORMAT_B8G8R8A8_SRGB")
   set_property(TEST RenderToneMappingNoMutableTests APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_KHRONOS_PROFILES_EXCLUDE_DEVICE_EXTENSIONS=set:VK_KHR_swapchain_mutable_format")
   add_test(NAME RenderAntialiasingUnsupportedTests COMMAND RenderAntialiasingQualityTests --expect-unsupported)
   set_tests_properties(RenderAntialiasingUnsupportedTests PROPERTIES LABELS "gpu;image_quality;capabilities" TIMEOUT 120 RUN_SERIAL TRUE
      FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
      ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR};VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation:VK_LAYER_KHRONOS_profiles;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG;VK_KHRONOS_PROFILES_PROFILE_DIRS=set:${CMAKE_CURRENT_SOURCE_DIR}/assets;VK_KHRONOS_PROFILES_PROFILE_NAME=set:VP_VVE_single_sample;VK_KHRONOS_PROFILES_SIMULATE_CAPABILITIES=set:SIMULATE_PROPERTIES_BIT;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=set:false")
   if(TARGET RenderImageQualityBaselineTests)
      add_test(NAME RenderImageQualityBaselineUnormTests COMMAND RenderImageQualityBaselineTests --unorm)
      # Remove only the surface's BGRA sRGB format features; RGBA material textures remain available.
      set_tests_properties(RenderImageQualityBaselineUnormTests PROPERTIES
         LABELS "gpu;image_quality;capabilities" RUN_SERIAL TRUE TIMEOUT 300
         FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
         ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR};VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation:VK_LAYER_KHRONOS_profiles;VK_LAYER_VALIDATE_SYNC=set:1;VK_KHRONOS_PROFILES_EXCLUDE_FORMATS=set:VK_FORMAT_B8G8R8A8_SRGB;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=set:false")
   endif()
   add_test(NAME RendererBackendUnsupportedTests COMMAND RendererBackendTests --expect-unsupported)
   set_tests_properties(RendererBackendUnsupportedTests PROPERTIES
      ENVIRONMENT "VK_LOADER_LAYERS_ENABLE=*validation,*profiles;VK_KHRONOS_PROFILES_PROFILE_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/assets;VK_KHRONOS_PROFILES_PROFILE_NAME=VP_VVE_forward_only;VK_KHRONOS_PROFILES_SIMULATE_CAPABILITIES=SIMULATE_PROPERTIES_BIT;VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=false"
      ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_TEST_PROFILES_LAYER_DIR}"
      LABELS "gpu;deferred;capabilities" RUN_SERIAL TRUE TIMEOUT 60
      FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
   add_test(NAME HybridGiBackendUnsupportedTests COMMAND HybridGiBackendTests --expect-unsupported)
   # Share the exact real-layer capability restriction used by the unchanged deferred test.
   foreach(profile_property IN ITEMS ENVIRONMENT ENVIRONMENT_MODIFICATION)
      get_test_property(RendererBackendUnsupportedTests ${profile_property} profile_value)
      set_tests_properties(HybridGiBackendUnsupportedTests PROPERTIES ${profile_property} "${profile_value}")
   endforeach()
   set_tests_properties(HybridGiBackendUnsupportedTests PROPERTIES
      LABELS "gpu;gi;capabilities" RUN_SERIAL TRUE TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endif()

# Reuse the existing assertions and assets against deferred rendering, with serialized fixture ownership.
foreach(renderer_regression IN ITEMS
   RenderFrameCountTests RenderRuntimeUpdateTests RenderResizeTests RenderAntialiasingTests
   RenderEnvironmentTests RenderTerrainTests MultiWindowRenderTests RenderShadowOcclusionTests
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

# IQ-03a freezes independent coverage before renderer implementation; shaders are strictly test-owned.
vve_add_engine_test(RenderSmaaQualityTests GPU Vulkan::Vulkan vve_test_support)
target_compile_definitions(RenderSmaaQualityTests PRIVATE VVE_SMAA_QUALITY_DIR="${CMAKE_BINARY_DIR}/verify/image-quality/smaa")
# Ship the original 3 November sealed captures/provenance; clean builds never manufacture GPU history.
# Recovered byte-for-byte from J20261103-183417-403786: 424 frozen files plus manifest.txt/freeze.log.
# The original none/msaa4 decoded receipts remain 8fb02ade1fca61dd/c8e5b19846691cb0.
set(smaa_reference_archive "${CMAKE_CURRENT_SOURCE_DIR}/assets/smaa-reference.tar.gz")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${smaa_reference_archive}")
file(SHA256 "${smaa_reference_archive}" smaa_reference_sha256)
if(NOT smaa_reference_sha256 STREQUAL "6a1f38f30e4a6dd3c7da94fc456c284ab205a32ae77f32fd576da672652a9c19")
   message(FATAL_ERROR "The sealed SMAA reference archive changed; restore the original fixture.")
endif()
set(smaa_reference_dir "${CMAKE_CURRENT_BINARY_DIR}/fixtures/smaa")
file(ARCHIVE_EXTRACT INPUT "${smaa_reference_archive}" DESTINATION "${smaa_reference_dir}")
target_compile_definitions(RenderSmaaQualityTests PRIVATE VVE_SMAA_HISTORICAL_DIR="${smaa_reference_dir}")
foreach(entry fixtureVertex fixtureForward fixtureGbuffer)
   set(stage fragment)
   if(entry STREQUAL fixtureVertex)
      set(stage vertex)
   endif()
   set(header "${native_shader_dir}/${entry}.h")
   add_custom_command(OUTPUT "${header}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${native_shader_dir}"
      COMMAND "${native_slangc}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/smaa_fixture.slang"
         -target spirv -profile spirv_1_4 -stage ${stage} -entry ${entry} -fvk-use-entrypoint-name
         -source-embed-style u32 -source-embed-name smaa_${entry} -o "${header}"
      DEPENDS assets/smaa_fixture.slang VERBATIM)
   target_sources(RenderSmaaQualityTests PRIVATE "${header}")
endforeach()
target_include_directories(RenderSmaaQualityTests PRIVATE "${native_shader_dir}")
add_test(NAME RenderSmaaStageOracleTests COMMAND RenderSmaaQualityTests --oracle)
add_test(NAME RenderSmaaCpuTests COMMAND RenderSmaaQualityTests --cpu)
# The CPU entry now runs all analytic/control fixtures for both sampler models in unoptimized Debug too.
set_tests_properties(RenderSmaaCpuTests PROPERTIES LABELS "image_quality" TIMEOUT 120)
set_tests_properties(RenderSmaaQualityTests RenderSmaaStageOracleTests PROPERTIES LABELS "gpu;image_quality" RUN_SERIAL TRUE TIMEOUT 600
   FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD"
   ENVIRONMENT_MODIFICATION "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d;VK_INSTANCE_LAYERS=set:VK_LAYER_KHRONOS_validation;VK_LAYER_VALIDATE_SYNC=set:1;VK_LAYER_REPORT_FLAGS=set:info,error,warn;VK_LAYER_DEBUG_ACTION=set:VK_DBG_LAYER_ACTION_LOG_MSG")

# Make the selected SDK's validation layer discoverable for every test, including subcommands and ordinary Debug tests.
if(EXISTS "${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   get_property(engine_tests DIRECTORY PROPERTY TESTS)
   set(sdk_layer_path "VK_ADD_LAYER_PATH=path_list_prepend:${VVE_VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
   foreach(name IN LISTS engine_tests)
      get_property(test_environment TEST ${name} PROPERTY ENVIRONMENT_MODIFICATION)
      if(NOT sdk_layer_path IN_LIST test_environment)
         set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT_MODIFICATION "${sdk_layer_path}")
      endif()
   endforeach()
endif()

# Both archived presentation formats must use the same driver as their frozen pixel receipts.
if(TARGET RenderImageQualityBaselineTests AND VVE_IQ_BASELINE_ICD)
   foreach(name IN ITEMS RenderImageQualityBaselineTests RenderImageQualityBaselineUnormTests)
      if(TEST ${name})
         set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT_MODIFICATION "VK_DRIVER_FILES=set:${VVE_IQ_BASELINE_ICD}")
      endif()
   endforeach()
endif()
