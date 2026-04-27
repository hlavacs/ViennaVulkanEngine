#include <algorithm>

import std;
import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for renderer-selected render graph construction.
 */
namespace {

   /// @brief Returns the first render pass with a non-postprocessing kernel.
   [[nodiscard]] const vve::v3::RenderPassDesc *mainPass(const vve::v3::RenderGraph &graph) {
      const auto pass = std::ranges::find_if(graph.passes, [](const vve::v3::RenderPassDesc &candidate) {
         return candidate.kernel != vve::v3::RenderKernelId::post_process &&
                candidate.kernel != vve::v3::RenderKernelId::post_post_process &&
                candidate.kernel != vve::v3::RenderKernelId::imgui;
      });

      return pass == graph.passes.end() ? nullptr : std::addressof(*pass);
   }

   /// @brief Verifies that a renderer id produces the expected primary kernel.
   [[nodiscard]] bool graphUsesKernel(vve::v3::RenderSystem &render_system, vve::v3::GraphicsBackend &backend,
                                      std::string_view renderer_id, vve::v3::RenderKernelId expected_kernel) {
      const auto renderer = backend.createRenderer(renderer_id);
      if (!renderer) {
         return false;
      }

      const auto graph = render_system.buildStaticGraph(
          vve::v3::WindowHandle{.value = vve::Handle::fromHash(std::string(renderer_id))}, *renderer);
      const auto *pass = mainPass(graph);
      return pass != nullptr && pass->kernel == expected_kernel && pass->debug_name == renderer->display_name;
   }

   /// @brief Builds a minimal reflected layout suitable for renderer binding tests.
   [[nodiscard]] vve::v3::PipelineLayoutDesc testLayout(const vve::v3::RendererDesc &renderer,
                                                        vve::v3::ShaderHandle shader) {
      return vve::v3::PipelineLayoutDesc{
          .renderer = renderer.handle,
          .renderer_id = renderer.id,
          .shader_program = shader,
          .shader_stages = {vve::v3::PipelineShaderStageDesc{.stage = vve::v3::ShaderStage::vertex,
                                                             .entry_point = "vertexMain",
                                                             .spirv_word_count = 16},
                            vve::v3::PipelineShaderStageDesc{.stage = vve::v3::ShaderStage::fragment,
                                                             .entry_point = "fragmentMain",
                                                             .spirv_word_count = 16}},
          .descriptor_sets = {vve::v3::PipelineDescriptorSetLayoutDesc{
              .set = 0,
              .bindings = {vve::v3::PipelineDescriptorBindingDesc{
                  .set = 0,
                  .binding = 0,
                  .kind = vve::v3::DescriptorBindingKind::uniform_buffer,
                  .name = "frame",
                  .type_name = "FrameData",
                  .visible_stages = {vve::v3::ShaderStage::vertex, vve::v3::ShaderStage::fragment}}}}}};
   }

   /// @brief Builds fake backend resource metadata for renderer-side binding validation.
   [[nodiscard]] vve::v3::PipelineBackendResources testResources(const vve::v3::PipelineLayoutDesc &layout) {
      return vve::v3::PipelineBackendResources{
          .handle = vve::v3::PipelineResourceHandle{.value = vve::Handle::fromHash(layout.renderer_id + ".resources")},
          .renderer = layout.renderer,
          .shader_program = layout.shader_program,
          .shader_module_count = layout.shader_stages.size(),
          .descriptor_set_layout_count = layout.descriptor_sets.size(),
          .pipeline_layout_created = true};
   }

   /// @brief Assembles a complete window pipeline for binding tests.
   [[nodiscard]] vve::v3::WindowRenderPipeline testPipeline(vve::v3::RenderSystem &render_system,
                                                           const vve::v3::RendererDesc &renderer) {
      const auto shader = vve::v3::ShaderHandle{
          .value = vve::Handle::fromHash(std::string{"shader."} + renderer.id)};
      auto layout = testLayout(renderer, shader);
      const auto window = vve::v3::WindowHandle{
          .value = vve::Handle::fromHash(std::string{"window."} + renderer.id)};

      return vve::v3::WindowRenderPipeline{
          .window = window,
          .window_id = std::string{"window."} + renderer.id,
          .renderer = renderer,
          .shader_program = shader,
          .pipeline_layout = layout,
          .backend_resources = testResources(layout),
          .graph = render_system.buildStaticGraph(window, renderer)};
   }

   /// @brief Builds a minimal uploaded scene suitable for draw-packet generation tests.
   [[nodiscard]] vve::v3::SceneData testUploadedScene() {
      const auto mesh_handle = vve::v3::MeshHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.mesh"})};
      const auto material_handle = vve::v3::MaterialHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.material"})};
      const auto node_handle = vve::v3::SceneNodeHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.node"})};
      const auto mesh_instance_handle = vve::Handle::fromHash(std::string_view{"tests.render.mesh_instance"});

      vve::v3::SceneData scene{};

      vve::v3::ImportedMesh mesh{};
      mesh.handle = mesh_handle;
      mesh.submeshes.push_back(vve::v3::ImportedSubmesh{
          .index_offset = 6,
          .index_count = 12,
          .material = material_handle});
      scene.mesh_indices.emplace(mesh_handle.value.value(), scene.meshes.size());
      scene.meshes.push_back(std::move(mesh));

      vve::v3::ImportedMaterial material{};
      material.handle = material_handle;
      material.double_sided = true;
      material.alpha_blend = true;
      scene.material_indices.emplace(material_handle.value.value(), scene.materials.size());
      scene.materials.push_back(std::move(material));

      vve::v3::SceneNodeDesc node{};
      node.handle = node_handle;
      node.world_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(1.0F, 2.0F, 3.0F));
      scene.node_indices.emplace(node_handle.value.value(), scene.nodes.size());
      scene.nodes.push_back(std::move(node));

      scene.mesh_instance_indices.emplace(mesh_instance_handle.value(), scene.mesh_instances.size());
      scene.mesh_instances.push_back(vve::v3::SceneMeshInstanceDesc{
          .handle = mesh_instance_handle,
          .node = node_handle,
          .mesh = mesh_handle});

      vve::v3::GpuMeshResources gpu_mesh{};
      gpu_mesh.mesh = mesh_handle;
      gpu_mesh.vertex_buffer =
          vve::v3::GpuBufferHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.vertex_buffer"})};
      gpu_mesh.index_buffer =
          vve::v3::GpuBufferHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.index_buffer"})};
      gpu_mesh.index_count = 24;
      gpu_mesh.resident = true;
      scene.gpu_mesh_indices.emplace(mesh_handle.value.value(), scene.gpu_meshes.size());
      scene.gpu_meshes.push_back(std::move(gpu_mesh));

      return scene;
   }

   /// @brief Verifies that renderer binding stores the backend resources for the selected renderer.
   [[nodiscard]] bool pipelineBindsRenderer(vve::v3::RenderSystem &render_system, vve::v3::GraphicsBackend &backend,
                                            std::string_view renderer_id, vve::v3::RenderKernelId expected_kernel) {
      const auto renderer = backend.createRenderer(renderer_id);
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      const auto stored = render_system.rendererPipeline(pipeline.window);
      return stored && stored->has_value() &&
             binding->renderer.value == renderer->handle.value &&
             binding->backend_resources.value == pipeline.backend_resources.handle.value &&
             binding->shader_program.value == pipeline.shader_program.value &&
             binding->main_kernel == expected_kernel &&
             binding->shader_stage_count == pipeline.pipeline_layout.shader_stages.size() &&
             binding->descriptor_set_layout_count == pipeline.pipeline_layout.descriptor_sets.size() &&
             binding->ready_for_pipeline_creation &&
             (*stored)->backend_resources.value == binding->backend_resources.value;
   }

   /// @brief Verifies renderer-specific graphics pipeline requests reach the backend cleanly.
   [[nodiscard]] bool pipelineRequestRequiresBackendInit(vve::v3::RenderSystem &render_system,
                                                         vve::v3::GraphicsBackend &backend,
                                                         std::string_view renderer_id) {
      const auto renderer = backend.createRenderer(renderer_id);
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      const auto graphics_pipeline = render_system.createGraphicsPipeline(backend, *binding);
      return !graphics_pipeline && graphics_pipeline.error() == vve::Error::not_initialized;
   }

   /// @brief Verifies uploaded mesh resources are translated to backend-neutral draw packets.
   [[nodiscard]] bool drawPacketsReferenceUploadedMesh(vve::v3::RenderSystem &render_system,
                                                       vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      const auto scene = testUploadedScene();
      const auto *pass = mainPass(pipeline.graph);
      if (pass == nullptr) {
         return false;
      }

      const auto build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 42, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!build) {
         return false;
      }

      const auto packet_list = render_system.drawPackets(pipeline.window);
      if (!packet_list || !packet_list->has_value() || (*packet_list)->packets.size() != 1 ||
          (*packet_list)->window.value != pipeline.window.value || (*packet_list)->frame_index != 42) {
         return false;
      }

      const auto &packet = (*packet_list)->packets.front();
      const auto &gpu_mesh = scene.gpu_meshes.front();
      const auto &mesh_instance = scene.mesh_instances.front();
      const auto &submesh = scene.meshes.front().submeshes.front();
      return packet.window.value == pipeline.window.value &&
             packet.pass.value.value() == pass->handle.value.value() &&
             packet.kernel == pass->kernel &&
             packet.node.value.value() == mesh_instance.node.value.value() &&
             packet.mesh_instance.value() == mesh_instance.handle.value() &&
             packet.mesh.value.value() == mesh_instance.mesh.value.value() &&
             packet.material.value.value() == submesh.material.value.value() &&
             packet.vertex_buffer.value.value() == gpu_mesh.vertex_buffer.value.value() &&
             packet.index_buffer.value.value() == gpu_mesh.index_buffer.value.value() &&
             packet.first_index == submesh.index_offset &&
             packet.index_count == submesh.index_count &&
             packet.instance_count == 1 &&
             packet.double_sided &&
             packet.alpha_blend;
   }

} // namespace

/**
 * @brief Executes the render-system graph selection regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   vve::v3::GraphicsBackend backend{};
   vve::v3::RenderSystem render_system{
       vve::RendererKind::forward_renderer,
       vve::ShadowKind::none,
       backend,
       false};

   if (!graphUsesKernel(render_system, backend, "forward", vve::v3::RenderKernelId::forward_opaque)) {
      return 1;
   }

   if (!graphUsesKernel(render_system, backend, "deferred", vve::v3::RenderKernelId::deferred_gbuffer)) {
      return 2;
   }

   if (!graphUsesKernel(render_system, backend, "path_tracing", vve::v3::RenderKernelId::path_trace)) {
      return 3;
   }

   if (!pipelineBindsRenderer(render_system, backend, "forward", vve::v3::RenderKernelId::forward_opaque)) {
      return 4;
   }

   if (!pipelineBindsRenderer(render_system, backend, "deferred", vve::v3::RenderKernelId::deferred_gbuffer)) {
      return 5;
   }

   if (!pipelineRequestRequiresBackendInit(render_system, backend, "forward")) {
      return 6;
   }

   if (!pipelineRequestRequiresBackendInit(render_system, backend, "deferred")) {
      return 7;
   }

   if (!pipelineRequestRequiresBackendInit(render_system, backend, "path_tracing")) {
      return 8;
   }

   const auto forward = backend.createRenderer("forward");
   const auto deferred = backend.createRenderer("deferred");
   if (!forward || !deferred) {
      return 9;
   }

   auto incompatible = testPipeline(render_system, *forward);
   incompatible.pipeline_layout = testLayout(*deferred, incompatible.shader_program);
   incompatible.backend_resources = testResources(incompatible.pipeline_layout);
   const auto rejected = render_system.bindPipelineResources(incompatible);
   if (rejected || rejected.error() != vve::Error::invalid_argument) {
      return 10;
   }

   auto invalid_binding_source = testPipeline(render_system, *forward);
   const auto valid_binding = render_system.bindPipelineResources(invalid_binding_source);
   if (!valid_binding) {
      return 11;
   }
   auto invalid_binding = *valid_binding;
   invalid_binding.main_kernel = vve::v3::RenderKernelId::path_trace;
   const auto invalid_pipeline_request = render_system.createGraphicsPipeline(backend, invalid_binding);
   if (invalid_pipeline_request || invalid_pipeline_request.error() != vve::Error::invalid_argument) {
      return 12;
   }

   if (!drawPacketsReferenceUploadedMesh(render_system, backend)) {
      return 13;
   }

   return 0;
}
