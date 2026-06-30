/**
 * @file
 * @brief Renderer-specific diagnostics coverage for the simple forward renderer.
 *
 * Functional objects:
 * - main: creates the simple engine implementation, selects the forward renderer,
 *   submits a tiny scene, and verifies direct forward-renderer diagnostic access.
 */

import std;

import VEEngine;
import VEEngine.Simple;
import VEEngine.Simple.Renderer;

namespace {

/// @brief Checks the current default clear color reported by the forward renderer.
[[nodiscard]] bool hasDefaultClearColor(const std::array<float, 4> &color) {
   return color[0] == 0.0F && color[1] == 0.0F && color[2] == 0.0F && color[3] == 1.0F;
}

} // namespace

int main() {
   auto engine = vve::simple::makeEngine(
      vve::ApplicationName{"simple-forward-renderer-tests"},
      vve::MaxFrames{.value = vve::FrameCount{.value = 1}},
      vve::WindowSetups{vve::WindowSetup{}
                           .id("main")
                           .title("simple-forward-renderer-tests")
                           .extent(vve::PixelExtent{.width = 64, .height = 64})
                           .renderer(vve::RendererId{.value = "forward"})
                           .visible(false)});
   if (!engine.init()) { return 1; }

   auto &render_system = engine.renderSystem();
   render_system.clearScene();
   if (const auto result = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{}); !result) {
      return 2;
   }
   render_system.setCamera(vve::Camera{}, vve::PixelExtent{.width = 64, .height = 64});
   render_system.setDirectionalLight(vve::Direction{}, vve::LinearColor{}, vve::LightIntensity{},
                                     vve::LinearColor{});

   // Verify the submitted scene is visible through the public facade before frame submission.
   if (render_system.sceneMeshCount() != 1 || render_system.sceneMaterialCount() != 1 ||
       render_system.sceneInstanceCount() != 1 || render_system.sceneVertexCount() != 4 ||
       render_system.sceneIndexCount() != 6 || !render_system.hasSceneCamera() ||
       !render_system.hasSceneDirectionalLight()) {
      return 3;
   }

   if (const auto result = render_system.renderFrame(engine.windowSystem()); !result) { return 4; }
   const auto status = engine.step();
   if (!status || *status != vve::FrameStatus::stopped) { return 4; }

   // Verify frame and draw counters that belong to the current forward-renderer diagnostics.
   if (render_system.renderedFrameCount() != 1 || render_system.lastRenderedWindowCount() != 1) { return 5; }
   const auto &forward_renderer = std::get<vve::simple::ForwardRenderer>(render_system.backend());
   if (forward_renderer.presentedFrameCount() != 0 || forward_renderer.triangleDrawCount() != 0 ||
       forward_renderer.triangleVertexCount() != 0 || forward_renderer.sceneUploadCount() != 0 ||
       forward_renderer.sceneMeshDrawCount() != 0 || forward_renderer.sceneInstanceDrawCount() != 0 ||
       forward_renderer.sceneDrawVertexCount() != 0 || forward_renderer.sceneDrawIndexCount() != 0) {
      return 6;
   }

   // Verify debug sample and comparison accessors expose the current no-readback state.
   if (forward_renderer.sceneDebugSampleCount() != 0 || forward_renderer.sceneCpuDebugSample(0) ||
       forward_renderer.sceneGpuDebugSample(0) || forward_renderer.sceneDebugClipError(0) ||
       forward_renderer.sceneDebugDepthError(0) || forward_renderer.sceneDebugLightSpaceError(0) ||
       forward_renderer.sceneDebugSpotLightSpaceError(0) || forward_renderer.sceneDebugPointLightSpaceError(0) ||
       forward_renderer.sceneDebugLightingError(0) || forward_renderer.sceneDebugShadowSampleError(0) ||
       forward_renderer.sceneDebugSpotShadowSampleError(0) || forward_renderer.sceneDebugPointShadowSampleError(0)) {
      return 7;
   }

   // Verify all shadow-depth diagnostic families are empty until GPU readback exists.
   if (forward_renderer.sceneShadowDepthSampleCount() != 0 || forward_renderer.sceneShadowDepthSample(0) ||
       forward_renderer.sceneShadowDepthError(0) || forward_renderer.sceneSpotShadowDepthSampleCount() != 0 ||
       forward_renderer.sceneSpotShadowDepthSample(0) || forward_renderer.sceneSpotShadowDepthError(0) ||
       forward_renderer.scenePointShadowDepthSampleCount() != 0 || forward_renderer.scenePointShadowDepthSample(0) ||
       forward_renderer.scenePointShadowDepthError(0)) {
      return 8;
   }

   // Verify remaining forward diagnostics that describe prepared targets and clear state.
   if (forward_renderer.preparedGpuTargetCount() != 0 || !hasDefaultClearColor(forward_renderer.lastClearColor())) {
      return 9;
   }

   return 0;
}
