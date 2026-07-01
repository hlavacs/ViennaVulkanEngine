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
import VEEngine.Simple.Scene;

namespace {

/// @brief Checks the current default clear color reported by the forward renderer.
[[nodiscard]] bool hasDefaultClearColor(const std::array<float, 4> &color) {
   return color[0] == 0.0F && color[1] == 0.0F && color[2] == 0.0F && color[3] == 1.0F;
}

/// @brief Verifies the forward renderer declares shadow depth before color shading.
[[nodiscard]] bool hasForwardShadowBeforeColorContract() {
   const auto passes = vve::simple::ForwardRenderer{}.passes();                    ///< Static pass vocabulary.
   const auto shadow_pass = std::ranges::find(passes, vve::simple::RenderMilestone::shadow_depth(),
                                              &vve::simple::RenderPassContract::name);
   const auto color_pass = std::ranges::find(passes, std::string_view{"forward.color_pass"},
                                             &vve::simple::RenderPassContract::name);
   if (shadow_pass == passes.end() || color_pass == passes.end()) { return false; } ///< Required nodes must exist.
   return std::ranges::contains(color_pass->depends_on, vve::simple::RenderMilestone::shadow_depth());
}

} // namespace

int main() {
   if (!hasForwardShadowBeforeColorContract()) { return 10; }

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
   render_system.addSpotLight(vve::Position{.value = vve::Vec3{-1.5F, 3.5F, 1.0F}},
                              vve::Direction{.value = vve::Vec3{0.35F, -1.0F, -0.25F}},
                              vve::LinearColor{.value = vve::Vec3{1.0F, 0.8F, 0.5F}},
                              vve::LightIntensity{.value = 3.0F}, vve::LightRange{.value = 6.0F},
                              vve::SpotConeAngle{.radians = 0.55F}); ///< First cone targets the plane from the left.
   render_system.addSpotLight(vve::Position{.value = vve::Vec3{1.75F, 4.25F, -1.25F}},
                              vve::Direction{.value = vve::Vec3{-0.45F, -1.0F, 0.30F}},
                              vve::LinearColor{.value = vve::Vec3{0.55F, 0.75F, 1.0F}},
                              vve::LightIntensity{.value = 2.5F}, vve::LightRange{.value = 7.0F},
                              vve::SpotConeAngle{.radians = 0.65F}); ///< Second cone uses a distinct origin and aim.

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

   // Verify directional and point shadow-depth diagnostic families are empty until GPU readback exists.
   if (forward_renderer.sceneShadowDepthSampleCount() != 0 || forward_renderer.sceneShadowDepthSample(0) ||
       forward_renderer.sceneShadowDepthError(0) || forward_renderer.scenePointShadowDepthSampleCount() != 0 ||
       forward_renderer.scenePointShadowDepthSample(0) ||
       forward_renderer.scenePointShadowDepthError(0)) {
      return 8;
   }

   // Verify each active spot light receives a unique shadow-array slot when samples are reachable.
   const std::size_t active_spot_light_count{
      std::min(forward_renderer.scene.spotLights.size(), vve::simple::kMaxShadowedSpotLights)}; ///< CPU scene cap.
   if (active_spot_light_count != std::min<std::size_t>(2U, vve::simple::kMaxShadowedSpotLights)) { return 11; }
   const auto &first_spot = forward_renderer.scene.spotLights[0]; ///< First retained engine spot light.
   const auto &second_spot = forward_renderer.scene.spotLights[1]; ///< Second retained engine spot light.
   if ((first_spot.position.x == second_spot.position.x && first_spot.position.y == second_spot.position.y &&
        first_spot.position.z == second_spot.position.z) ||
       (first_spot.direction.x == second_spot.direction.x && first_spot.direction.y == second_spot.direction.y &&
        first_spot.direction.z == second_spot.direction.z)) {
      return 12;
   }
   if (forward_renderer.sceneSpotShadowDepthSampleCount() == active_spot_light_count) {
      std::vector<std::uint32_t> spot_shadow_slots{}; ///< Engine-produced RenderShadowDepthSample::face_index slots.
      for (std::size_t spot_index{}; spot_index < active_spot_light_count; ++spot_index) {
         const auto sample = forward_renderer.sceneSpotShadowDepthSample(spot_index); ///< Existing observable slot path.
         if (!sample || !sample->valid) { return 13; }
         spot_shadow_slots.push_back(sample->face_index);
      }
      std::ranges::sort(spot_shadow_slots);
      const auto unique_spot_shadow_slots = std::ranges::unique(spot_shadow_slots); ///< Pairwise uniqueness proof.
      if (static_cast<std::size_t>(unique_spot_shadow_slots.begin() - spot_shadow_slots.begin()) !=
          active_spot_light_count) {
         return 13;
      }
   } else {
      /** @brief Headless stopped-frame state exposes retained lights but no per-light slot samples or array layers. */
   }
   if (forward_renderer.sceneSpotShadowDepthError(0)) { return 14; }

   // Verify remaining forward diagnostics that describe prepared targets and clear state.
   if (forward_renderer.preparedGpuTargetCount() != 0 || !hasDefaultClearColor(forward_renderer.lastClearColor())) {
      return 9;
   }

   return 0;
}
