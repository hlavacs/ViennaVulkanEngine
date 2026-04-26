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

   const auto forward = backend.createRenderer("forward");
   const auto deferred = backend.createRenderer("deferred");
   if (!forward || !deferred) {
      return 6;
   }

   auto incompatible = testPipeline(render_system, *forward);
   incompatible.pipeline_layout = testLayout(*deferred, incompatible.shader_program);
   incompatible.backend_resources = testResources(incompatible.pipeline_layout);
   const auto rejected = render_system.bindPipelineResources(incompatible);
   if (rejected || rejected.error() != vve::Error::invalid_argument) {
      return 7;
   }

   return 0;
}
