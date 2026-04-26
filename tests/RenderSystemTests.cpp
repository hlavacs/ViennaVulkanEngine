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

   return 0;
}
