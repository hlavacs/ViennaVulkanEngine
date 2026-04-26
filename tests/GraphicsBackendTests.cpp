#include <algorithm>
#include <string_view>
#include <vector>

import std;
import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for backend renderer-id selection.
 */
namespace {

   /// @brief Returns whether a renderer list contains a canonical id.
   [[nodiscard]] bool hasRendererId(const std::vector<vve::v3::RendererDesc> &renderers, std::string_view id) {
      return std::ranges::any_of(renderers, [id](const vve::v3::RendererDesc &renderer) {
         return renderer.id == id;
      });
   }

} // namespace

/**
 * @brief Executes the graphics-backend renderer registry regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   vve::v3::GraphicsBackend backend{};

   const auto supported = backend.supportedRenderers();
   if (!hasRendererId(supported, "forward") || !hasRendererId(supported, "deferred") ||
       !hasRendererId(supported, "path_tracing")) {
      return 1;
   }

   const auto forward = backend.createRenderer("forward");
   if (!forward || forward->id != "forward" || forward->api != vve::GraphicsApi::vulkan ||
       forward->kind != vve::RendererKind::forward_renderer ||
       forward->main_kernel != vve::v3::RenderKernelId::forward_opaque) {
      return 2;
   }

   const auto forward_alias = backend.createRenderer(" Forward-Renderer ");
   if (!forward_alias || forward_alias->id != forward->id || forward_alias->handle.value != forward->handle.value) {
      return 3;
   }

   const auto deferred_alias = backend.createRenderer("deferred-renderer");
   if (!deferred_alias || deferred_alias->id != "deferred" ||
       deferred_alias->main_kernel != vve::v3::RenderKernelId::deferred_gbuffer) {
      return 4;
   }

   const auto path_tracer_alias = backend.createRenderer("path-tracer");
   if (!path_tracer_alias || path_tracer_alias->id != "path_tracing" ||
       path_tracer_alias->main_kernel != vve::v3::RenderKernelId::path_trace) {
      return 5;
   }

   const auto unsupported = backend.createRenderer("software");
   if (unsupported || unsupported.error() != vve::Error::invalid_argument) {
      return 6;
   }

   const auto begin_before_init = backend.beginFrame(vve::v3::FrameContext{});
   if (begin_before_init || begin_before_init.error() != vve::Error::not_initialized) {
      return 7;
   }

   return 0;
}
