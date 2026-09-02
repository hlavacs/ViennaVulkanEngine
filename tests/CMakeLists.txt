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
add_executable(SimpleForwardRendererTests SimpleForwardRendererTests.cpp)
target_compile_features(SimpleForwardRendererTests PRIVATE cxx_std_23)
target_compile_definitions(SimpleForwardRendererTests PRIVATE
   VULKAN_HPP_NO_EXCEPTIONS
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple
   VVE_ENGINE_IMPLEMENTATION_IS_SIMPLE
   VVE_ENGINE_HAS_SIMPLE
   "VVE_SIMPLE_SHADER_DIR=\"${PROJECT_BINARY_DIR}/src/versions/simple/shaders\""
   "VVE_SIMPLE_SHADER_SOURCE=\"${PROJECT_SOURCE_DIR}/src/versions/simple/shaders/simple_forward.slang\""
   "VVE_TEST_CRATE_TEXTURE=\"${PROJECT_SOURCE_DIR}/assets/game/crate0/diffuse.png\"")
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(assimp CONFIG REQUIRED)
target_include_directories(SimpleForwardRendererTests PRIVATE ${Stb_INCLUDE_DIR})
target_sources(SimpleForwardRendererTests
   PRIVATE
      FILE_SET simple_forward_renderer_test_modules TYPE CXX_MODULES
      BASE_DIRS "${PROJECT_SOURCE_DIR}"
      FILES
         "${PROJECT_SOURCE_DIR}/src/Math.ixx"
         "${PROJECT_SOURCE_DIR}/src/Error.ixx"
         "${PROJECT_SOURCE_DIR}/src/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/Entity.ixx"
         "${PROJECT_SOURCE_DIR}/src/Vector.ixx"
         "${PROJECT_SOURCE_DIR}/src/Types.ixx"
         "${PROJECT_SOURCE_DIR}/src/ECS.ixx"
         "${PROJECT_SOURCE_DIR}/src/ECSContainer.ixx"
         "${PROJECT_SOURCE_DIR}/src/Window.ixx"
         "${PROJECT_SOURCE_DIR}/src/World.ixx"
         "${PROJECT_SOURCE_DIR}/src/Assets.ixx"
         "${PROJECT_SOURCE_DIR}/src/RenderSystem.ixx"
         "${PROJECT_SOURCE_DIR}/src/Gui.ixx"
         "${PROJECT_SOURCE_DIR}/src/Engine.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Error.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Math.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vector.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Types.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Graph.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Window.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Assets.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Shaders.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Mesh.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Scene.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderPassContract.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderPass.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Device.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Commands.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Presentation.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Pipeline.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Shadow.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Readback.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Resources.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Vulkan.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererDebug.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererShadowPrep.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererResources.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererDraw.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystemDebug.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderResources.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSceneImport.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystemScene.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystemObjects.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/Renderer.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystem.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/GUI.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Engine.ixx"
)
add_dependencies(SimpleForwardRendererTests ViennaVulkanEngine)
target_sources(SimpleForwardRendererTests
   PRIVATE
      "${PROJECT_SOURCE_DIR}/src/Engine.cpp"
      "${PROJECT_SOURCE_DIR}/src/RenderSystem.cpp"
      "${PROJECT_SOURCE_DIR}/src/Assets.cpp"
      "${PROJECT_SOURCE_DIR}/src/Window.cpp"
      "${PROJECT_SOURCE_DIR}/src/Gui.cpp"
)
target_sources(SimpleForwardRendererTests PRIVATE "${PROJECT_SOURCE_DIR}/src/versions/simple/stb_image_write_impl.cpp")
target_include_directories(SimpleForwardRendererTests PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(SimpleForwardRendererTests PRIVATE assimp::assimp imgui::imgui glm::glm SDL3::SDL3 Vulkan::Vulkan slang::slang)
add_test(NAME SimpleForwardRendererTests COMMAND SimpleForwardRendererTests)
vve_set_output_dirs(SimpleForwardRendererTests)
add_executable(ShaderReflectionBindingsTests ShaderReflectionBindingsTests.cpp)
target_compile_features(ShaderReflectionBindingsTests PRIVATE cxx_std_23)
target_compile_definitions(ShaderReflectionBindingsTests PRIVATE
   VULKAN_HPP_NO_EXCEPTIONS
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple
   VVE_ENGINE_IMPLEMENTATION_IS_SIMPLE
   VVE_ENGINE_HAS_SIMPLE
   "VVE_SIMPLE_SHADER_SOURCE=\"${PROJECT_SOURCE_DIR}/src/versions/simple/shaders/simple_forward.slang\"")
target_sources(ShaderReflectionBindingsTests
   PRIVATE
      FILE_SET shader_reflection_bindings_test_modules TYPE CXX_MODULES
      BASE_DIRS "${PROJECT_SOURCE_DIR}"
      FILES
         "${PROJECT_SOURCE_DIR}/src/Math.ixx"
         "${PROJECT_SOURCE_DIR}/src/Error.ixx"
         "${PROJECT_SOURCE_DIR}/src/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/Entity.ixx"
         "${PROJECT_SOURCE_DIR}/src/Vector.ixx"
         "${PROJECT_SOURCE_DIR}/src/Types.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Error.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vector.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Shaders.ixx"
)
target_link_libraries(ShaderReflectionBindingsTests PRIVATE glm::glm Vulkan::Vulkan slang::slang)
add_test(NAME ShaderReflectionBindingsTests COMMAND ShaderReflectionBindingsTests)
vve_set_output_dirs(ShaderReflectionBindingsTests)
find_package(imgui CONFIG REQUIRED)
add_executable(GuiSystemTests GuiSystemTests.cpp)
target_compile_features(GuiSystemTests PRIVATE cxx_std_23)
target_compile_definitions(GuiSystemTests PRIVATE
   VULKAN_HPP_NO_EXCEPTIONS
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple
   VVE_ENGINE_IMPLEMENTATION_IS_SIMPLE
   VVE_ENGINE_HAS_SIMPLE
   "VVE_SIMPLE_SHADER_DIR=\"${PROJECT_BINARY_DIR}/src/versions/simple/shaders\""
   "VVE_SIMPLE_SHADER_SOURCE=\"${PROJECT_SOURCE_DIR}/src/versions/simple/shaders/simple_forward.slang\"")
target_sources(GuiSystemTests
   PRIVATE
      FILE_SET gui_system_test_modules TYPE CXX_MODULES
      BASE_DIRS "${PROJECT_SOURCE_DIR}"
      FILES
         "${PROJECT_SOURCE_DIR}/src/Math.ixx"
         "${PROJECT_SOURCE_DIR}/src/Error.ixx"
         "${PROJECT_SOURCE_DIR}/src/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/Entity.ixx"
         "${PROJECT_SOURCE_DIR}/src/Vector.ixx"
         "${PROJECT_SOURCE_DIR}/src/Types.ixx"
         "${PROJECT_SOURCE_DIR}/src/ECS.ixx"
         "${PROJECT_SOURCE_DIR}/src/ECSContainer.ixx"
         "${PROJECT_SOURCE_DIR}/src/Window.ixx"
         "${PROJECT_SOURCE_DIR}/src/World.ixx"
         "${PROJECT_SOURCE_DIR}/src/Assets.ixx"
         "${PROJECT_SOURCE_DIR}/src/RenderSystem.ixx"
         "${PROJECT_SOURCE_DIR}/src/Gui.ixx"
         "${PROJECT_SOURCE_DIR}/src/Engine.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Error.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Math.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vector.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Types.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Graph.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Window.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Assets.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Shaders.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Mesh.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Scene.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderPassContract.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderPass.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Handle.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Device.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Commands.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Presentation.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Pipeline.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Shadow.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Readback.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Resources.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Vulkan/Vulkan.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererDebug.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererShadowPrep.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererResources.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RendererDraw.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystemDebug.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderResources.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSceneImport.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystemScene.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystemObjects.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/Renderer.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Render/RenderSystem.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/GUI.ixx"
         "${PROJECT_SOURCE_DIR}/src/versions/simple/Engine.ixx"
)
add_dependencies(GuiSystemTests ViennaVulkanEngine)
target_sources(GuiSystemTests
   PRIVATE
      "${PROJECT_SOURCE_DIR}/src/Engine.cpp"
      "${PROJECT_SOURCE_DIR}/src/RenderSystem.cpp"
      "${PROJECT_SOURCE_DIR}/src/Assets.cpp"
      "${PROJECT_SOURCE_DIR}/src/Window.cpp"
      "${PROJECT_SOURCE_DIR}/src/Gui.cpp"
)
target_sources(GuiSystemTests PRIVATE "${PROJECT_SOURCE_DIR}/src/versions/simple/stb_image_write_impl.cpp")
target_include_directories(GuiSystemTests PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(GuiSystemTests PRIVATE assimp::assimp glm::glm imgui::imgui SDL3::SDL3 Vulkan::Vulkan slang::slang)
add_test(NAME GuiSystemTests COMMAND GuiSystemTests)
vve_set_output_dirs(GuiSystemTests)
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
