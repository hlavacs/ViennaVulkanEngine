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
   "VVE_SIMPLE_SHADER_DIR=\"${CMAKE_BINARY_DIR}/src/versions/simple/shaders\"")
find_package(Stb REQUIRED)
target_include_directories(SimpleForwardRendererTests PRIVATE ${Stb_INCLUDE_DIR})
target_sources(SimpleForwardRendererTests
   PRIVATE
      FILE_SET simple_forward_renderer_test_modules TYPE CXX_MODULES
      BASE_DIRS "${CMAKE_SOURCE_DIR}"
      FILES
         "${CMAKE_SOURCE_DIR}/src/Math.ixx"
         "${CMAKE_SOURCE_DIR}/src/Error.ixx"
         "${CMAKE_SOURCE_DIR}/src/Handle.ixx"
         "${CMAKE_SOURCE_DIR}/src/Entity.ixx"
         "${CMAKE_SOURCE_DIR}/src/Vector.ixx"
         "${CMAKE_SOURCE_DIR}/src/Types.ixx"
         "${CMAKE_SOURCE_DIR}/src/ECS.ixx"
         "${CMAKE_SOURCE_DIR}/src/ECSContainer.ixx"
         "${CMAKE_SOURCE_DIR}/src/Window.ixx"
         "${CMAKE_SOURCE_DIR}/src/World.ixx"
         "${CMAKE_SOURCE_DIR}/src/Assets.ixx"
         "${CMAKE_SOURCE_DIR}/src/RenderSystem.ixx"
         "${CMAKE_SOURCE_DIR}/src/Gui.ixx"
         "${CMAKE_SOURCE_DIR}/src/Engine.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Error.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Math.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Handle.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Vector.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Types.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Graph.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/ECS.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Window.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Assets.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Resources.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Shaders.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Mesh.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Scene.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/RenderPassContract.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/RenderPass.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Vulkan.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Renderer.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/RenderSystem.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Gui.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Engine.ixx"
)
add_dependencies(SimpleForwardRendererTests ViennaVulkanEngine)
target_link_libraries(SimpleForwardRendererTests PRIVATE "$<TARGET_FILE:ViennaVulkanEngine>" SDL3::SDL3 Vulkan::Vulkan slang::slang)
add_test(NAME SimpleForwardRendererTests COMMAND SimpleForwardRendererTests)
vve_set_output_dirs(SimpleForwardRendererTests)
find_package(imgui CONFIG REQUIRED)
add_executable(GuiSystemTests GuiSystemTests.cpp)
target_compile_features(GuiSystemTests PRIVATE cxx_std_23)
target_compile_definitions(GuiSystemTests PRIVATE
   VULKAN_HPP_NO_EXCEPTIONS
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple
   VVE_ENGINE_IMPLEMENTATION_IS_SIMPLE
   VVE_ENGINE_HAS_SIMPLE
   "VVE_SIMPLE_SHADER_DIR=\"${CMAKE_BINARY_DIR}/src/versions/simple/shaders\"")
target_sources(GuiSystemTests
   PRIVATE
      FILE_SET gui_system_test_modules TYPE CXX_MODULES
      BASE_DIRS "${CMAKE_SOURCE_DIR}"
      FILES
         "${CMAKE_SOURCE_DIR}/src/Math.ixx"
         "${CMAKE_SOURCE_DIR}/src/Error.ixx"
         "${CMAKE_SOURCE_DIR}/src/Handle.ixx"
         "${CMAKE_SOURCE_DIR}/src/Entity.ixx"
         "${CMAKE_SOURCE_DIR}/src/Vector.ixx"
         "${CMAKE_SOURCE_DIR}/src/Types.ixx"
         "${CMAKE_SOURCE_DIR}/src/ECS.ixx"
         "${CMAKE_SOURCE_DIR}/src/ECSContainer.ixx"
         "${CMAKE_SOURCE_DIR}/src/Window.ixx"
         "${CMAKE_SOURCE_DIR}/src/World.ixx"
         "${CMAKE_SOURCE_DIR}/src/Assets.ixx"
         "${CMAKE_SOURCE_DIR}/src/RenderSystem.ixx"
         "${CMAKE_SOURCE_DIR}/src/Gui.ixx"
         "${CMAKE_SOURCE_DIR}/src/Engine.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Error.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Math.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Handle.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Vector.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Types.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Graph.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/ECS.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Window.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Assets.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Resources.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Shaders.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Mesh.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Scene.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/RenderPassContract.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/RenderPass.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Vulkan.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Renderer.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/RenderSystem.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Gui.ixx"
         "${CMAKE_SOURCE_DIR}/src/versions/simple/Engine.ixx"
)
add_dependencies(GuiSystemTests ViennaVulkanEngine)
target_link_libraries(GuiSystemTests PRIVATE "$<TARGET_FILE:ViennaVulkanEngine>" imgui::imgui SDL3::SDL3 Vulkan::Vulkan slang::slang)
add_test(NAME GuiSystemTests COMMAND GuiSystemTests)
vve_set_output_dirs(GuiSystemTests)
vve_add_engine_test(SceneSystemTests)
