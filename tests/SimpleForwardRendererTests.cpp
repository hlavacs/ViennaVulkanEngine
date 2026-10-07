#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <imgui.h>

/**
 * @file
 * @brief Renderer-specific diagnostics coverage for the simple forward renderer.
 *
 * Functional objects:
 * - main: creates the simple engine implementation, selects the forward renderer,
 *   submits a tiny scene, and verifies direct forward-renderer diagnostic access.
 */

import std;

import VVEngine;
import VVE.TestSupport;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.Scene;

namespace {

/// @brief Checks all clip boundaries, transformed/unknown boxes, flags and dynamic bounds through recorded draws.
[[nodiscard]] bool hasConservativeBounds(vve::simple::Engine &engine) {
   auto &render = engine.renderSystem();
   auto &renderer = render.forward();
   render.clearScene();
   const auto cube = render.addCuboid({-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}, vve::LinearColor{});
   if (!cube || !engine.renderFrame()) { return false; }
   auto &bounds = renderer.meshes.begin()->second.localBox;
   if (!bounds.valid || vve::math::lengthSquared(vve::math::subtract(bounds.minimum.value, vve::Vec3{-1.0F})) != 0.0F ||
       vve::math::lengthSquared(vve::math::subtract(bounds.maximum.value, vve::Vec3{1.0F})) != 0.0F) { return false; }
   renderer.scene.spotLights = {vve::simple::ForwardSpotLight{.position = {0.0F, 0.0F, 0.0F},
      .direction = {0.0F, 0.0F, -1.0F}, .range = {100.0F}, .outerConeAngle = {std::numbers::pi_v<float> / 4.0F}}};
   // Synthetic cached boxes isolate plane contact from triangle rasterization; camera and spot clips coincide.
   const std::array<std::tuple<vve::Vec3, vve::Vec3, vve::Vec3>, 6U> boundaries{{
      {{-3.0F, -0.1F, -2.0F}, {-2.0F, 0.1F, -2.0F}, {-0.1F, 0.0F, 0.0F}},
      {{2.0F, -0.1F, -2.0F}, {3.0F, 0.1F, -2.0F}, {0.1F, 0.0F, 0.0F}},
      {{-0.1F, -3.0F, -2.0F}, {0.1F, -2.0F, -2.0F}, {0.0F, -0.1F, 0.0F}},
      {{-0.1F, 2.0F, -2.0F}, {0.1F, 3.0F, -2.0F}, {0.0F, 0.1F, 0.0F}},
      {{-0.01F, -0.01F, -0.1F}, {0.01F, 0.01F, -0.05F}, {0.0F, 0.0F, 0.02F}},
      {{-1.0F, -1.0F, -101.0F}, {1.0F, 1.0F, -100.0F}, {0.0F, 0.0F, -0.1F}},
   }};
   for (const auto &[minimum, maximum, outward] : boundaries) {
      for (const float step : {0.0F, -1.0F, 1.0F}) {
         const auto offset = vve::math::scale(outward, step);
         bounds = {.minimum = {vve::math::add(minimum, offset)}, .maximum = {vve::math::add(maximum, offset)}, .valid = true};
         if (!engine.renderFrame()) { return false; }
         const auto stats = renderer.lastFrameDrawStats();
         const std::size_t expected = step <= 0.0F ? 1U : 0U;
         if (stats.forwardDraws != expected || stats.shadowDraws != expected) {
            std::println("R2 boundary minimum={},{},{} step={} expected={} forward={} shadow={}",
               minimum.x, minimum.y, minimum.z, step, expected, stats.forwardDraws, stats.shadowDraws);
            return false;
         }
      }
   }
   // Rotation mixes local axes; negative/nonuniform scale must preserve an intersecting world box.
   bounds = {.minimum = {{-1.0F, -1.0F, -1.0F}}, .maximum = {{1.0F, 1.0F, 1.0F}}, .valid = true};
   const vve::Transform transformed{.translation = {{5.5F, 0.0F, -2.0F}},
      .rotation = {{std::cos(std::numbers::pi_v<float> / 8.0F), 0.0F, std::sin(std::numbers::pi_v<float> / 8.0F), 0.0F}},
      .scale = {{-3.0F, 0.5F, 1.0F}}};
   if (!render.setObjectTransform(*cube, transformed) || !engine.renderFrame() ||
       renderer.lastFrameDrawStats().forwardDraws != 1U || renderer.lastFrameDrawStats().shadowDraws != 1U) { return false; }
   if (!render.setObjectTransform(*cube, vve::Transform{.translation = {{1000.0F, 0.0F, 0.0F}}}) ||
       !engine.renderFrame() || renderer.lastFrameDrawStats().forwardDraws != 0U) { return false; }
   bounds.valid = false;
   if (!engine.renderFrame() || renderer.lastFrameDrawStats().forwardDraws != 1U || renderer.lastFrameDrawStats().shadowDraws != 1U) { return false; }
   if (!render.setObjectCastsShadow(*cube, false) || !engine.renderFrame() ||
       renderer.lastFrameDrawStats().forwardDraws != 1U || renderer.lastFrameDrawStats().shadowDraws != 0U) { return false; }
   if (!render.setObjectVisible(*cube, false) || !engine.renderFrame() ||
       renderer.lastFrameDrawStats().forwardDraws != 0U || renderer.lastFrameDrawStats().vertexBufferBinds != 0U) { return false; }
   // Each edit crosses the camera and spot near planes; both in-flight vertex copies must adopt fresh bounds.
   render.clearScene();
   renderer.scene.spotLights = {vve::simple::ForwardSpotLight{.position = {0.0F, 0.0F, 0.0F},
      .direction = {0.0F, 0.0F, -1.0F}, .range = {10.0F}}};
   const auto triangle = render.addTriangleMesh({{-0.5F, -0.5F, 4.0F}, {0.5F, -0.5F, 4.0F}, {0.0F, 0.5F, 4.0F}},
      {0U, 1U, 2U}, vve::LinearColor{});
   if (!triangle || !engine.renderFrame() || renderer.lastFrameDrawStats().forwardDraws != 0U) { return false; }
   for (const float z : {-4.0F, 4.0F, -4.0F}) {
      if (!render.setObjectMeshPositions(*triangle, {{-0.5F, -0.5F, z}, {0.5F, -0.5F, z}, {0.0F, 0.5F, z}}) ||
          !engine.renderFrame() || renderer.meshes.begin()->second.localBox.minimum.value.z != z ||
          renderer.lastFrameDrawStats().forwardDraws != (z < 0.0F ? 1U : 0U) ||
          renderer.lastFrameDrawStats().shadowDraws != (z < 0.0F ? 1U : 0U)) { return false; }
   }
   std::println("R2 conservative_bounds: six planes touch/intersect/outside, transforms, invalid bounds, flags, dynamic edits passed");
   return true;
}

/// @brief Checks per-pass culling and command counts with separate camera and light views.
[[nodiscard]] bool hasCulledDraws() {
   auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
      vve::WindowDesc{.id = "culling", .extent = {64, 64}, .visible = false}}}};
   if (!engine.init()) { return false; }
   auto &render = engine.renderSystem();
   auto &renderer = render.forward();
   render.clearScene();
   render.setCamera(vve::Camera{.position = {{0.0F, 0.0F, 0.0F}}, .fov_y = {std::numbers::pi_v<float> / 2.0F}});
   const auto front = render.addCuboid({-0.5F, -0.5F, -4.5F}, {0.5F, 0.5F, -3.5F}, vve::LinearColor{});
   if (!front || !engine.renderFrame()) { return false; }
   const auto baseline = renderer.lastFrameDrawStats();
   const auto behind = render.addCuboid({-0.5F, -0.5F, 3.5F}, {0.5F, 0.5F, 4.5F}, vve::LinearColor{});
   if (!behind || !engine.renderFrame()) { return false; }
   const auto behind_stats = renderer.lastFrameDrawStats();
   bool correct = baseline.forwardDraws == 1U && behind_stats.forwardDraws == baseline.forwardDraws;
   // A distant, short-range spot clears its layer but draws no objects.
   renderer.scene.spotLights = {vve::simple::ForwardSpotLight{.position = {100.0F, 0.0F, 0.0F},
      .direction = {0.0F, 0.0F, -1.0F}, .range = {3.0F}}};
   if (!engine.renderFrame()) { return false; }
   const auto far_spot = renderer.lastFrameDrawStats();
   correct &= far_spot.shadowDraws == baseline.shadowDraws && far_spot.pipelineBinds == 2U;
   // A point at the camera sees one cube on each Z face, including the cube behind the camera.
   renderer.scene.spotLights.clear();
   renderer.scene.pointLights = {vve::simple::ForwardPointLight{.position = {0.0F, 0.0F, 0.0F}, .range = 10.0F}};
   if (!engine.renderFrame()) { return false; }
   const auto point = renderer.lastFrameDrawStats();
   correct &= point.forwardDraws == 1U && point.shadowDraws == 2U && point.pipelineBinds == 2U;
   std::println("R2 behind lastFrameDrawStats={},{},{},{}", behind_stats.forwardDraws, behind_stats.shadowDraws,
      behind_stats.vertexBufferBinds, behind_stats.pipelineBinds);
   std::println("R2 far_spot lastFrameDrawStats={},{},{},{}", far_spot.forwardDraws, far_spot.shadowDraws,
      far_spot.vertexBufferBinds, far_spot.pipelineBinds);
   std::println("R2 point lastFrameDrawStats={},{},{},{}", point.forwardDraws, point.shadowDraws,
      point.vertexBufferBinds, point.pipelineBinds);
   // Directional cascades use their own orthographic clips, including the caster-depth backoff.
   renderer.scene.pointLights.clear();
   renderer.scene.directionalLights = {vve::simple::ForwardDirectionalLight{}};
   if (!engine.renderFrame()) { return false; }
   const auto directional = renderer.lastFrameDrawStats();
   if (!render.addCuboid({999.0F, 999.0F, 999.0F}, {1001.0F, 1001.0F, 1001.0F}, vve::LinearColor{}) ||
       !engine.renderFrame()) { return false; }
   const auto distant = renderer.lastFrameDrawStats();
   correct &= directional.shadowDraws > 0U && distant.shadowDraws == directional.shadowDraws &&
      distant.forwardDraws == directional.forwardDraws && distant.pipelineBinds == 2U;
   std::println("R2 directional lastFrameDrawStats={},{},{},{}", distant.forwardDraws, distant.shadowDraws,
      distant.vertexBufferBinds, distant.pipelineBinds);
   correct &= hasConservativeBounds(engine);
   render.waitIdle();
   engine.gui().shutdownVulkan();
   render.shutdown();
   return correct && renderer.validationErrorCount() == 0U;
}

/// @brief Checks static mesh placement and the memory selected for per-frame uniform uploads.
[[nodiscard]] bool hasExpectedBufferMemory(const vve::simple::ForwardRenderer &renderer) {
   if (renderer.meshes.empty() || renderer.targets.empty()) { return false; }
   const auto &mesh = renderer.meshes.begin()->second;
   VkMemoryPropertyFlags vertex_flags{}, index_flags{};
   vmaGetAllocationMemoryProperties(renderer.allocator, mesh.vertexBuffer.allocation, &vertex_flags);
   vmaGetAllocationMemoryProperties(renderer.allocator, mesh.indexBuffer.allocation, &index_flags);
   std::println("R4 static vertex_flags={} index_flags={} vertex_mapped={} index_mapped={}", vertex_flags, index_flags,
      mesh.vertexBuffer.mapped != nullptr, mesh.indexBuffer.mapped != nullptr);
   if (!(vertex_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) || !(index_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
       mesh.vertexBuffer.mapped != nullptr || mesh.indexBuffer.mapped != nullptr) { return false; }
   // Upload memory may be cached on UMA, where the same allocation is also device-local.
   for (const auto &buffer : renderer.targets.front().uniformBuffers.buffers) {
      VkMemoryPropertyFlags flags{};
      vmaGetAllocationMemoryProperties(renderer.allocator, buffer.allocation, &flags);
      std::println("R4 uniform flags={} mapped={}", flags, buffer.mapped != nullptr);
      if (buffer.mapped == nullptr ||
          ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) && !(flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))) { return false; }
   }
   return true;
}

/// @brief Checks zero/negative intensity packing, ambient-only flags and normalized axes without a GPU.
[[nodiscard]] bool hasAmbientOnlyLightPacking() {
   auto renderer = vve::simple::ForwardRenderer{};
   // Dark entries must disappear; ambient entries retain a lighting slot ahead of the shadowed light.
   for (const float intensity : {0.0F, -1.0F}) {
      renderer.scene.spotLights = {
         vve::simple::ForwardSpotLight{.intensity = {intensity}, .ambient = 0.0F},
         vve::simple::ForwardSpotLight{.intensity = {intensity}, .ambient = 0.2F}, vve::simple::ForwardSpotLight{}};
      renderer.scene.directionalLights = {
         vve::simple::ForwardDirectionalLight{.intensity = {intensity}, .ambient = 0.0F},
         vve::simple::ForwardDirectionalLight{.intensity = {intensity}, .ambient = 0.2F}, vve::simple::ForwardDirectionalLight{}};
      renderer.scene.pointLights = {vve::simple::ForwardPointLight{.intensity = intensity}, vve::simple::ForwardPointLight{}};
      renderer.prepareShadowFrame(vve::math::identityMat4(), 1.0F, 1.0F, 0.1F, 100.0F);
      const auto &frame = renderer.frameUniforms(); ///< The prepared data is also the uniform upload source.
      if (frame.activeSpotLightCount != 2U || frame.activeDirectionalLightCount != 2U || frame.activePointLightCount != 1U ||
          renderer.shadowLightMeta.size() != 11U || frame.pointLightColorIntensities[1].w != 0.0F) { return false; }
      for (const auto &directions : {frame.spotLightDirections, frame.directionalLightDirections}) {
         if (directions[0].w != 1.0F || directions[1].w != 0.0F) { return false; }
         for (const auto &axis : directions | std::views::take(2)) {
            if (std::abs(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z - 1.0F) > 1e-5F) { return false; }
         }
      }
      if (frame.spotLightConeAmbients[0].w != 0.2F || frame.directionalLightAmbients[0].w != 0.2F) { return false; }
      // Metadata and readback must never describe the unrendered ambient-only layers.
      for (const auto &meta : renderer.shadowLightMeta) {
         if ((meta.light_type == 1U || meta.light_type == 3U) && meta.light_index != 1U) { return false; }
      }
      renderer.recordShadowDepthSamples();
      if (renderer.shadowDepthSamples.size() != 2U || renderer.shadowDepthSamples[0].layer != 1U) { return false; }
   }
   return true;
}

/// @brief Exercises one-layer sampling, every window/frame descriptor after growth, and retained capacity after clearing.
[[nodiscard]] bool hasMultiWindowShadowGrowth() {
   auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
      vve::WindowDesc{.id = "a", .extent = {64, 64}, .visible = false},
      vve::WindowDesc{.id = "b", .extent = {64, 64}, .visible = false}}}};
   if (!engine.init()) { return false; }
   auto &render = engine.renderSystem();
   auto &renderer = render.forward();
   render.clearScene();
   if (!render.addPlane(vve::Vec2{8.0F, 8.0F}, vve::LinearColor{}) || !engine.renderFrame()) { return false; }
   if (renderer.dirShadowArray.layerCount != 1U || renderer.spotShadowArray.layerCount != 1U ||
       renderer.pointShadowArray.layerCount != 1U || renderer.lastShadowLayerPassCount() != 0U) { return false; }
   renderer.scene.spotLights = {vve::simple::ForwardSpotLight{}};
   if (!engine.renderFrame() || renderer.spotShadowArray.layerCount != 1U || renderer.lastShadowLayerPassCount() != 1U) { return false; }
   // Ambient-only slots precede shadow casters, so active layers are not a contiguous prefix.
   renderer.scene.spotLights.insert(renderer.scene.spotLights.begin(), vve::simple::ForwardSpotLight{.intensity = {0.0F}, .ambient = 0.2F});
   renderer.scene.directionalLights = {
      vve::simple::ForwardDirectionalLight{.intensity = {0.0F}, .ambient = 0.2F}, vve::simple::ForwardDirectionalLight{}};
   renderer.scene.pointLights = {vve::simple::ForwardPointLight{.intensity = 0.0F}, vve::simple::ForwardPointLight{}};
   // Cover both frame slots in both windows without debug readback serializing the frames.
   for (std::size_t frame{}; frame < vve::simple::ForwardRenderer::framesInFlight; ++frame) {
      if (!engine.renderFrame() || render.lastRenderedWindowCount() != 2U || renderer.lastShadowLayerPassCount() != 11U ||
          renderer.dirShadowArray.layerCount != 8U || renderer.spotShadowArray.layerCount != 2U ||
          renderer.pointShadowArray.layerCount != vve::simple::ForwardRenderer::pointShadowFaceCount) { return false; }
   }
   renderer.clearScene();
   if (!engine.renderFrame() || renderer.lastShadowLayerPassCount() != 0U || renderer.dirShadowArray.layerCount != 8U ||
       renderer.spotShadowArray.layerCount != 2U || renderer.pointShadowArray.layerCount != vve::simple::ForwardRenderer::pointShadowFaceCount) { return false; }
   render.waitIdle();
   std::println("R1 multi_window_growth retained=8/2/6 empty_passes={} pipeline_creates={} validation_errors={}",
      renderer.lastShadowLayerPassCount(), renderer.forwardPipelineCreateCount(), renderer.validationErrorCount());
   return renderer.forwardPipelineCreateCount() == 1U && renderer.validationErrorCount() == 0U;
}

/// @brief Proves ERROR callbacks are counted exactly once without contaminating rendering diagnostics.
[[nodiscard]] bool hasValidationMessageCounter() {
   auto renderer = vve::simple::ForwardRenderer{}; ///< Isolated instance for the intentional validation error.
   if (renderer.instance.create("validation-counter-test") != VK_SUCCESS) { return false; }
   if (!renderer.validationActive()) {
      std::cout << "SimpleForwardRendererTests validation injection skipped: "
         << (renderer.instance.layers.empty() ? "VK_LAYER_KHRONOS_validation disabled or unavailable" :
            "VK_EXT_debug_utils unavailable") << '\n';
      return true;
   }
   const auto submitMessage = reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(
      vkGetInstanceProcAddr(renderer.instance.instance, "vkSubmitDebugUtilsMessageEXT"));
   if (submitMessage == nullptr) { return false; }
   const auto before = renderer.validationErrorCount();
   const VkDebugUtilsMessengerCallbackDataEXT message{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
      .pMessageIdName = "R14-IntentionalError",
      .pMessage = "Intentional ERROR: validation counter plumbing test"};
   submitMessage(renderer.instance.instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &message);
   const auto after = renderer.validationErrorCount();
   std::cout << "SimpleForwardRendererTests validation injection before=" << before
      << " after=" << after << " delta=" << after - before << '\n';
   renderer.instance.cleanup();
   return before == 0U && after == before + 1U && renderer.validationErrorCount() == after;
}

/// @brief Verifies explicit command recording emits all shadow pass tags before forward color.
[[nodiscard]] bool hasRecordedShadowsBeforeForwardColor(const vve::simple::ForwardRenderer &renderer) {
   using RecordedPass = vve::simple::ForwardRenderer::RecordedPass; ///< Concrete backend diagnostic enum.
   bool saw_directional_shadow{};                                   ///< Directional depth was recorded before color.
   bool saw_spot_shadow{};                                          ///< Spot depth was recorded before color.
   bool saw_point_shadow{};                                         ///< Point depth was recorded before color.
   bool saw_forward_color{};                                        ///< Final forward pass has been reached.

   // Shadow generation must be complete before the swapchain color pass starts.
   for (const RecordedPass pass : renderer.lastRecordedPassOrder()) {
      if (pass == RecordedPass::forward_color) {
         saw_forward_color = true;
      } else if (saw_forward_color) {
         return false;
      } else if (pass == RecordedPass::directional_shadow) {
         saw_directional_shadow = true;
      } else if (pass == RecordedPass::spot_shadow) {
         saw_spot_shadow = true;
      } else if (pass == RecordedPass::point_shadow) {
         saw_point_shadow = true;
      }
   }
   return saw_directional_shadow && saw_spot_shadow && saw_point_shadow && saw_forward_color;
}

/// @brief Compares authored directional-light fields with the renderer's stored light.
[[nodiscard]] bool sameDirectionalLight(const vve::simple::ForwardDirectionalLight &left,
                                        const vve::DirectionalLight &right) {
   return left.direction.x == right.direction.value.x && left.direction.y == right.direction.value.y &&
          left.direction.z == right.direction.value.z && left.color.x == right.color.value.x &&
          left.color.y == right.color.value.y && left.color.z == right.color.value.z &&
          left.intensity.value == right.intensity.value && left.ambient == right.ambient.value.x;
}

/// @brief Verifies each shadowed spot light owns its packed layer inside the spot array.
[[nodiscard]] bool hasUniqueSpotShadowMeta(const vve::simple::RenderSystem &render_system,
                                           std::size_t expected_spot_count) {
   if (render_system.sceneShadowLightMetaCount() < expected_spot_count) { return false; }

   std::vector<std::uint32_t> first_layers{}; ///< Metadata depth-array first layers.
   first_layers.reserve(expected_spot_count);

   // Collect every prepared metadata row through the public renderer diagnostic surface.
   for (std::size_t index{}; index < expected_spot_count; ++index) {
      const auto meta = render_system.sceneShadowLightMeta(index);
      if (!meta || meta->light_type != 1U || meta->light_index != index || meta->first_layer != index) { return false; }
      first_layers.push_back(meta->first_layer);
   }

   std::ranges::sort(first_layers);
   const auto unique_first_layers = std::ranges::unique(first_layers); ///< Pairwise first-layer proof.
   return static_cast<std::size_t>(unique_first_layers.begin() - first_layers.begin()) == expected_spot_count;
}

/// @brief Verifies disabled spot lights are absent from dense shader-visible shadow metadata.
[[nodiscard]] bool hasDisabledFirstSpotExcludedFromPackedMeta(const vve::simple::RenderSystem &render_system,
                                                              float disabled_range, float first_enabled_range) {
   if (render_system.sceneShadowLightMetaCount() == 0U) { return false; }
   const auto first_meta = render_system.sceneShadowLightMeta(0); ///< First packed row must belong to an enabled light.
   if (!first_meta || first_meta->light_type != 1U || first_meta->light_index != 0U ||
       first_meta->first_layer != 0U ||
       first_meta->far_plane != first_enabled_range) {
      return false;
   }

   // Disabled source lights must not allocate spot metadata or consume dense shader slots.
   for (std::size_t row{}; row < render_system.sceneShadowLightMetaCount(); ++row) {
      const auto meta = render_system.sceneShadowLightMeta(row);
      if (!meta || meta->light_type != 1U) { continue; }
      if (meta->far_plane == disabled_range) { return false; }
   }
   return true;
}

/// @brief Verifies point-shadow metadata rows use six unique contiguous layers per point light.
[[nodiscard]] bool hasPointShadowMetaInvariants(const vve::simple::RenderSystem &render_system,
                                                std::size_t spot_row_count, std::size_t point_light_count,
                                                std::size_t directional_light_count) {
   constexpr std::size_t point_shadow_face_count{vve::simple::ForwardRenderer::pointShadowFaceCount}; ///< Cubemap-style point shadows render six views.
   if (point_light_count > vve::simple::kMaxShadowedPointLights) { return false; } ///< CPU metadata cap.
   const std::size_t expected_point_rows{point_shadow_face_count * point_light_count}; ///< Six rows per point light.
   const std::size_t first_directional_row{spot_row_count + expected_point_rows}; ///< Directional cascades follow spot and point rows.
   const std::size_t expected_directional_rows{directional_light_count * vve::simple::kNumShadowCascades}; ///< Four rows per directional light.
   if (render_system.sceneShadowLightMetaCount() != first_directional_row + expected_directional_rows) { return false; }

   // Spot rows address their own array, independently of point-face layers.
   for (std::size_t spot_index{}; spot_index < spot_row_count; ++spot_index) {
      const auto meta = render_system.sceneShadowLightMeta(spot_index);
      if (!meta || meta->light_type != 1U || meta->first_layer != spot_index) { return false; }
   }

   std::vector<std::vector<std::uint32_t>> layers_by_light(point_light_count); ///< Layers grouped by source light.
   std::vector<std::uint32_t> all_point_layers{};                              ///< Global uniqueness proof.
   all_point_layers.reserve(expected_point_rows);

   // Walk every point metadata row and check the public CPU contract for cubemap-style faces.
   for (std::size_t row{spot_row_count}; row < first_directional_row; ++row) {
      const auto meta = render_system.sceneShadowLightMeta(row);
      if (!meta || meta->light_type != 2U) { return false; }
      if (std::abs(meta->projection[0][0]) >= 0.999F) { return false; } ///< Point faces overlap beyond ninety degrees.
      if (meta->light_index >= point_light_count || meta->light_index >= vve::simple::kMaxShadowedPointLights) {
         return false;
      }
      const auto face = static_cast<std::uint32_t>((row - spot_row_count) % point_shadow_face_count); ///< Face within this packed light.
      if (meta->first_layer != meta->light_index * point_shadow_face_count + face) { return false; }
      layers_by_light[meta->light_index].push_back(meta->first_layer);
      all_point_layers.push_back(meta->first_layer);
   }

   std::ranges::sort(all_point_layers);
   const auto unique_point_layers = std::ranges::unique(all_point_layers); ///< No point layer is reused.
   if (static_cast<std::size_t>(unique_point_layers.begin() - all_point_layers.begin()) != expected_point_rows) {
      return false;
   }

   // Each point light must own exactly one contiguous six-layer range.
   for (auto &layers : layers_by_light) {
      if (layers.size() != point_shadow_face_count) { return false; }
      std::ranges::sort(layers);
      for (std::size_t face_index{1U}; face_index < layers.size(); ++face_index) {
         if (layers[face_index] != layers.front() + static_cast<std::uint32_t>(face_index)) { return false; }
      }
   }

   // Trailing directional metadata must preserve the flattened light-cascade layer layout.
   for (std::size_t row{first_directional_row}; row < render_system.sceneShadowLightMetaCount(); ++row) {
      const auto meta = render_system.sceneShadowLightMeta(row);
      const std::size_t directional_row{row - first_directional_row}; ///< Dense row among directional cascades.
      const std::uint32_t light_index{static_cast<std::uint32_t>(directional_row / vve::simple::kNumShadowCascades)};
      const std::uint32_t cascade_index{static_cast<std::uint32_t>(directional_row % vve::simple::kNumShadowCascades)};
      const std::uint32_t expected_layer{light_index * static_cast<std::uint32_t>(vve::simple::kNumShadowCascades) + cascade_index};
      if (!meta || meta->light_type != 3U || meta->light_index != light_index || meta->first_layer != expected_layer) {
         return false;
      }
   }
   return true;
}

/// @brief Selects the same point-shadow face as the forward fragment shader for the origin debug sample.
[[nodiscard]] std::uint32_t pointShadowDebugFace(const vve::simple::ForwardPointLight &light) {
   const vve::Vec3 light_to_origin{-light.position.x, -light.position.y, -light.position.z}; ///< Fixed debug point is world origin.
   const vve::Vec3 abs_light_to_origin{std::abs(light_to_origin.x), std::abs(light_to_origin.y),
                                       std::abs(light_to_origin.z)}; ///< Dominant-axis selector inputs.
   return abs_light_to_origin.x >= abs_light_to_origin.y && abs_light_to_origin.x >= abs_light_to_origin.z
             ? (light_to_origin.x >= 0.0F ? 0U : 1U)
             : (abs_light_to_origin.y >= abs_light_to_origin.z ? (light_to_origin.y >= 0.0F ? 2U : 3U)
                                                               : (light_to_origin.z >= 0.0F ? 4U : 5U));
}

/// @brief Returns the recorded shadow-depth samples of one light type (1 spot, 2 point, 3 directional).
[[nodiscard]] std::vector<vve::RenderShadowDepthSample> samplesOfType(const vve::simple::ForwardRenderer &renderer,
                                                                            std::uint32_t light_type) {
   std::vector<vve::RenderShadowDepthSample> result{};
   for (const auto &sample : renderer.shadowDepthSamples) {
      if (sample.light_type == light_type) { result.push_back(sample); }
   }
   return result;
}

/// @brief Verifies one origin sample was read back from the GPU with finite diagnostic values.
[[nodiscard]] bool hasConsistentGpuSample(const vve::RenderShadowDepthSample &sample) {
   if (!sample.has_gpu || !std::isfinite(sample.gpu_depth) || !std::isfinite(sample.error) ||
       !std::isfinite(sample.expected_depth) || !std::isfinite(sample.bias) || !std::isfinite(sample.shadow_factor)) { return false; }
   if (sample.world.x != 0.0F || sample.world.y != 0.0F || sample.world.z != 0.0F) { return false; }
   if (sample.expected_depth != sample.light_ndc.z || sample.error != std::abs(sample.expected_depth - sample.gpu_depth)) { return false; }
   return true;
}

/// @brief Verifies one point sample per point light, each on the shader-selected face and its own array layer.
[[nodiscard]] bool hasPointShadowDepthSamples(const vve::simple::ForwardRenderer &renderer,
                                              const vve::simple::RenderSystem &render_system,
                                              std::size_t point_light_count) {
   const auto samples = samplesOfType(renderer, 2U);
   if (samples.size() != point_light_count || render_system.shadowDepthSamples().size() != renderer.shadowDepthSamples.size()) {
      return false;
   }
   std::vector<std::uint32_t> layers{}; ///< No selected point layer may be reused.
   for (std::size_t point_index{}; point_index < point_light_count; ++point_index) {
      const auto &sample = samples[point_index];
      const std::uint32_t expected_face{pointShadowDebugFace(renderer.scene.pointLights[point_index])};
      if (!hasConsistentGpuSample(sample) || sample.light_index != point_index || sample.face_index != expected_face ||
          sample.layer != point_index * vve::simple::ForwardRenderer::pointShadowFaceCount + expected_face) {
         return false;
      }
      layers.push_back(sample.layer);
   }
   std::ranges::sort(layers);
   return std::ranges::adjacent_find(layers) == layers.end();
}

/// @brief Verifies every non-occluded sample keeps full light contribution and at least one such sample exists.
[[nodiscard]] bool hasFullContributionForNonOccludedShadowSamples(const vve::simple::ForwardRenderer &renderer) {
   bool saw_non_occluded_sample{}; ///< At least one retained sample proves normal lighting is preserved.
   for (const auto &sample : renderer.shadowDepthSamples) {
      if (!hasConsistentGpuSample(sample)) { return false; }
      saw_non_occluded_sample = saw_non_occluded_sample || sample.shadow_factor == 1.0F;
   }
   return saw_non_occluded_sample;
}

/// @brief Verifies the light-zero cascade-zero CPU projection agrees with its rendered depth texel.
[[nodiscard]] bool hasDirectionalShadowDepthAgreement(const vve::simple::ForwardRenderer &renderer) {
   constexpr float depth_tolerance{0.002F}; ///< Raster bias and texel quantization may move stored depth slightly.
   const auto samples = samplesOfType(renderer, 3U);
   if (samples.size() != 1U || samples.front().layer != 0U || !hasConsistentGpuSample(samples.front())) { return false; }
   return samples.front().error <= depth_tolerance;
}

/// @brief Renders empty and full directional-light lists and checks flattened cascade ownership.
[[nodiscard]] bool hasDirectionalRuntimeToggleCoverage(vve::simple::Engine &engine,
                                                       vve::simple::RenderSystem &render_system) {
   using RecordedPass = vve::simple::ForwardRenderer::RecordedPass; ///< Concrete pass tags expose draw counts.
   auto &renderer = render_system.forward();
   const auto directional_pass_count = [&renderer] {
      return static_cast<std::size_t>(std::ranges::count(renderer.lastRecordedPassOrder(),
                                                        RecordedPass::directional_shadow));
   };

   // An empty vector leaves no directional shadow pass, metadata, or sample.
   render_system.clearScene();
   if (!render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{}) || !engine.renderFrame()) {
      return false;
   }
   const bool has_directional_meta = std::ranges::any_of(renderer.shadowLightMeta, [](const auto &meta) {
      return meta.light_type == 3U;
   });
   if (!renderer.scene.directionalLights.empty() || has_directional_meta ||
       !samplesOfType(renderer, 3U).empty() || directional_pass_count() != 0U) {
      return false;
   }

   // Fill every packed light slot and require all cascade matrices, layers, metadata rows, and passes.
   for (std::size_t light_index{}; light_index < vve::simple::kMaxDirectionalLights; ++light_index) {
      const float component{static_cast<float>(light_index + 1U)};
      render_system.addDirectionalLight(
         vve::Direction{.value = vve::Vec3{-component, -1.0F, 0.25F * component}},
         vve::LinearColor{.value = vve::Vec3{1.0F, 0.8F, 0.6F}},
         vve::LightIntensity{.value = 1.0F}, vve::LinearColor{});
   }
   if (!engine.renderFrame() || renderer.scene.directionalLights.size() != vve::simple::kMaxDirectionalLights ||
       renderer.dirShadowArray.layerViews.size() !=
          vve::simple::kMaxDirectionalLights * vve::simple::kNumShadowCascades) {
      return false;
   }

   std::size_t directional_row{}; ///< Dense directional row index across unrelated metadata entries.
   for (const auto &meta : renderer.shadowLightMeta) {
      if (meta.light_type != 3U) { continue; }
      const std::uint32_t expected_light{static_cast<std::uint32_t>(directional_row / vve::simple::kNumShadowCascades)};
      const std::uint32_t expected_layer{static_cast<std::uint32_t>(directional_row)};
      if (meta.light_index != expected_light || meta.first_layer != expected_layer) {
         return false;
      }
      ++directional_row;
   }
   const std::size_t expected_cascade_passes{
      vve::simple::kMaxDirectionalLights * vve::simple::kNumShadowCascades};
   std::println("R1 directional regrow layers={} passes={}", renderer.dirShadowArray.layerCount, renderer.lastShadowLayerPassCount());
   return directional_row == expected_cascade_passes &&
          directional_pass_count() == expected_cascade_passes &&
          renderer.lastShadowLayerPassCount() == expected_cascade_passes &&
          hasDirectionalShadowDepthAgreement(renderer);
}

/// @brief Checks only the public render-object lifetime facade on a live render system.
[[nodiscard]] bool hasPublicRenderObjectLifetime() {
   auto engine = vve::EngineBuilder<>{}
                    .applicationName("simple-forward-renderer-lifetime-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
                    .addWindow(vve::WindowSetup{}
                                  .id("main")
                                  .title("simple-forward-renderer-lifetime-tests")
                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                  .renderer(vve::RendererId{.value = "forward"})
                                  .visible(false))
                    .build();
   if (!engine.init()) { return false; }

   auto world = engine.world();
   auto &render_system = world.get<vve::RenderSystem>();
   render_system.clearScene();
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
	const auto cuboid = render_system.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
																											 vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{});
	const auto triangle = render_system.addTriangleMesh(
		{vve::Vec3{-1.0F, 0.0F, 0.0F}, vve::Vec3{1.0F, 0.0F, 0.0F},
		 vve::Vec3{0.0F, 1.0F, 0.0F}}, {0U, 1U, 2U}, vve::LinearColor{});
	if (!plane || !cuboid || !triangle || !plane->valid() || !cuboid->valid() ||
		!triangle->valid() || *plane == *cuboid || *plane == *triangle) { return false; }
	if (const auto updated = render_system.setObjectMeshPositions(*triangle,
		{vve::Vec3{-1.0F, 0.0F, 0.0F}, vve::Vec3{1.0F, 0.0F, 0.0F},
		 vve::Vec3{0.0F, 1.5F, 0.0F}}); !updated) {
		return false;
	}
	const auto wrong_vertex_count = render_system.setObjectMeshPositions(
		*triangle, {vve::Vec3{}, vve::Vec3{}});
	const auto invalid_triangle = render_system.addTriangleMesh(
		{vve::Vec3{}, vve::Vec3{}, vve::Vec3{}}, {0U, 1U, 3U}, vve::LinearColor{});
	if (wrong_vertex_count || wrong_vertex_count.error() != vve::Error::invalid_argument ||
		invalid_triangle || invalid_triangle.error() != vve::Error::invalid_argument) {
		return false;
	}
	const std::size_t instance_count_before_remove{render_system.sceneInstanceCount()}; ///< CPU instances mirror public objects.

   if (const auto hidden = render_system.setObjectVisible(*plane, false); !hidden) { return false; }
   const auto hidden_state = render_system.objectVisible(*plane);
   if (!hidden_state || *hidden_state) { return false; }
   if (const auto shown = render_system.setObjectVisible(*plane, true); !shown) { return false; }
   const auto shown_state = render_system.objectVisible(*plane);
   if (!shown_state || !*shown_state) { return false; }

   const auto transform = vve::Transform{.translation = vve::Position{.value = vve::Vec3{1.25F, 2.5F, -3.75F}},
                                         .scale = vve::Scale{.value = vve::Vec3{2.0F, 0.5F, 1.5F}}};
   if (const auto moved = render_system.setObjectTransform(*plane, transform); !moved) { return false; }
   const auto moved_transform = render_system.objectTransform(*plane);
   if (!moved_transform ||
       moved_transform->translation.value.x != transform.translation.value.x ||
       moved_transform->translation.value.y != transform.translation.value.y ||
       moved_transform->translation.value.z != transform.translation.value.z ||
       moved_transform->scale.value.x != transform.scale.value.x ||
       moved_transform->scale.value.y != transform.scale.value.y ||
       moved_transform->scale.value.z != transform.scale.value.z) {
      return false;
   }

   if (const auto removed = render_system.removeObject(*plane); !removed) { return false; }
	if (instance_count_before_remove != 3U ||
       render_system.sceneInstanceCount() != instance_count_before_remove - 1U) {
      return false;
   }
   const auto removed_visible = render_system.objectVisible(*plane);
   const auto removed_transform = render_system.objectTransform(*plane);
   const auto removed_again = render_system.removeObject(*plane);
   const auto surviving_visible = render_system.objectVisible(*cuboid);
   if (removed_visible || removed_visible.error() != vve::Error::missing_object ||
       removed_transform || removed_transform.error() != vve::Error::missing_object ||
       removed_again || removed_again.error() != vve::Error::missing_object ||
       !surviving_visible || !*surviving_visible) {
      return false;
   }

   const auto missing = vve::makeHandleForTest<vve::RenderObjectHandle>(vve::RenderObjectHandle::id_mask);
   const auto default_visible = render_system.objectVisible(vve::RenderObjectHandle{});
   const auto default_transform = render_system.objectTransform(vve::RenderObjectHandle{});
   const auto missing_visible = render_system.objectVisible(missing);
   const auto missing_transform = render_system.objectTransform(missing);
   const auto missing_hide = render_system.setObjectVisible(missing, false);
   const auto missing_move = render_system.setObjectTransform(missing, transform);
   const auto missing_remove = render_system.removeObject(missing);
   if (default_visible || default_visible.error() != vve::Error::missing_object ||
       default_transform || default_transform.error() != vve::Error::missing_object ||
       missing_visible || missing_visible.error() != vve::Error::missing_object ||
       missing_transform || missing_transform.error() != vve::Error::missing_object ||
       missing_hide || missing_hide.error() != vve::Error::missing_object ||
       missing_move || missing_move.error() != vve::Error::missing_object ||
       missing_remove || missing_remove.error() != vve::Error::missing_object) {
      return false;
   }

	render_system.clearScene();
	const auto cleared_visible = render_system.objectVisible(*cuboid);
	const auto cleared_triangle = render_system.setObjectMeshPositions(
		*triangle, {vve::Vec3{}, vve::Vec3{}, vve::Vec3{}});
	return !cleared_visible && cleared_visible.error() == vve::Error::missing_object &&
		!cleared_triangle && cleared_triangle.error() == vve::Error::missing_object;
}

/// @brief Verifies object visibility updates the renderer-owned instance draw flag.
[[nodiscard]] bool hasBackendObjectVisibilityUpdate() {
   auto render_system = vve::simple::RenderSystem{};
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!plane) { return false; }

   if (const auto hidden = render_system.setObjectVisible(*plane, false); !hidden) { return false; }
   const auto hidden_state = render_system.objectVisible(*plane);
   if (!hidden_state || *hidden_state) { return false; }
   if (const auto shown = render_system.setObjectVisible(*plane, true); !shown) { return false; }
   const auto shown_state = render_system.objectVisible(*plane);

   const auto missing = vve::makeHandleForTest<vve::RenderObjectHandle>(vve::RenderObjectHandle::id_mask);
   const auto missing_hide = render_system.setObjectVisible(missing, false);
   const auto missing_visible = render_system.objectVisible(missing);
   return shown_state && *shown_state &&
          !missing_hide && missing_hide.error() == vve::Error::missing_object &&
          !missing_visible && missing_visible.error() == vve::Error::missing_object;
}

/// @brief Verifies object movement updates the renderer-owned instance transform.
[[nodiscard]] bool hasBackendObjectTransformUpdate() {
   auto render_system = vve::simple::RenderSystem{};
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!plane) { return false; }

   const auto transform = vve::Transform{.translation = vve::Position{.value = vve::Vec3{1.25F, 2.5F, -3.75F}},
                                         .scale = vve::Scale{.value = vve::Vec3{2.0F, 0.5F, 1.5F}}};
   if (const auto moved = render_system.setObjectTransform(*plane, transform); !moved) { return false; }

   const auto current = render_system.objectTransform(*plane);
   const auto missing = vve::makeHandleForTest<vve::RenderObjectHandle>(vve::RenderObjectHandle::id_mask);
   const auto missing_move = render_system.setObjectTransform(missing, transform);
   return current && current->translation.value.x == transform.translation.value.x &&
          current->translation.value.y == transform.translation.value.y &&
          current->translation.value.z == transform.translation.value.z &&
          current->scale.value.x == transform.scale.value.x &&
          current->scale.value.y == transform.scale.value.y &&
          current->scale.value.z == transform.scale.value.z &&
          !missing_move && missing_move.error() == vve::Error::missing_object;
}

/// @brief Verifies stable object handles survive removal of a different instance.
[[nodiscard]] bool hasBackendObjectCorrespondenceWithoutPublicHandle() {
   auto render_system = vve::simple::RenderSystem{};
   render_system.loadScene(vve::simple::Scene{});
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   const auto cuboid = render_system.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
                                               vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{});
   if (!plane || !cuboid) { return false; }

   if (render_system.sceneInstanceCount() != 2U) { return false; }
   if (const auto removed = render_system.removeObject(*plane); !removed) { return false; }
   const auto cuboid_visible = render_system.objectVisible(*cuboid);
   return render_system.sceneInstanceCount() == 1U && cuboid_visible && *cuboid_visible;
}

/// @brief Verifies objects and textures added after renderer initialization reach live GPU resources.
[[nodiscard]] bool hasRuntimeGpuObjectSynchronization() {
   auto engine = vve::test::hiddenEngine("simple-forward-runtime-object-tests", vve::PixelExtent{.width = 64, .height = 64});
   if (!engine.init()) { return false; }

   auto &render_system = engine.renderSystem();
   render_system.clearScene();
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!plane || !engine.renderFrame()) { return false; }
   auto &renderer = render_system.forward();
   if (renderer.meshes.size() != 1U || render_system.sceneInstanceCount() != 1U) { return false; }

   const auto cuboid = render_system.addTexturedCuboid(
      vve::Vec3{-0.5F, -0.5F, -0.5F}, vve::Vec3{0.5F, 0.5F, 0.5F},
      std::filesystem::path{VVE_TEST_CRATE_TEXTURE});
   if (!cuboid || !engine.renderFrame() || renderer.meshes.size() != 2U ||
       render_system.sceneInstanceCount() != 2U || renderer.objectTextures[0].extent.width == 0U) {
      return false;
   }

   // The crate must retain a complete mip chain down to one texel.
   const auto &texture = renderer.objectTextures[0];
   const auto expected_mips = 1U + static_cast<std::uint32_t>(std::floor(std::log2(
      std::max(texture.extent.width, texture.extent.height))));
   std::println("R6 crate mip_levels={} expected={}", texture.mipLevels, expected_mips);
   if (texture.mipLevels != expected_mips) { return false; }

   if (const auto removed = render_system.removeObject(*plane); !removed) { return false; }
   if (!engine.renderFrame() || renderer.meshes.size() != 1U || render_system.sceneInstanceCount() != 1U) {
      return false;
   }
   render_system.clearScene();
   if (!engine.renderFrame() || !renderer.meshes.empty() || render_system.sceneInstanceCount() != 0U) { return false; }
   std::cout << "runtime validationActive=" << renderer.validationActive()
      << " validationErrorCount=" << renderer.validationErrorCount() << '\n';
   if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return false; }
   return true;
}

/// @brief Verifies asset purging only removes mesh and material data after public objects stop referencing it.
[[nodiscard]] bool hasPurgeUnusedAssetsLifetime() {
   auto render_system = vve::simple::RenderSystem{};
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   const auto cuboid = render_system.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
                                               vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{});
   if (!plane || !cuboid || !plane->valid() || !cuboid->valid()) { return false; }

   constexpr std::size_t object_count{2U};                  ///< Plane and cuboid each mint one instance.
   const std::size_t live_mesh_count{render_system.sceneMeshCount()};           ///< Meshes before no-op purge.
   const std::size_t live_material_count{render_system.sceneMaterialCount()};   ///< Materials before no-op purge.
   const std::size_t live_instance_count{render_system.sceneInstanceCount()};   ///< Instances before no-op purge.
   if (live_mesh_count != object_count || live_material_count != 1U ||
       live_instance_count != object_count || render_system.purgeUnusedAssets() != 0U ||
       render_system.sceneMeshCount() != live_mesh_count ||
       render_system.sceneMaterialCount() != live_material_count ||
       render_system.sceneInstanceCount() != live_instance_count) {
      return false;
   }

   if (const auto removed = render_system.removeObject(*plane); !removed) { return false; }
   const std::size_t mesh_count_before_purge{render_system.sceneMeshCount()};           ///< Removal already released the plane mesh.
   const std::size_t material_count_before_purge{render_system.sceneMaterialCount()};   ///< The cuboid retains the shared material.
   const std::size_t instance_count_before_purge{render_system.sceneInstanceCount()};   ///< Purge must not remove instances.
   if (mesh_count_before_purge != live_mesh_count - 1U ||
       material_count_before_purge != live_material_count ||
       instance_count_before_purge != live_instance_count - 1U) {
      return false;
   }

   if (render_system.purgeUnusedAssets() != 0U ||
       render_system.sceneMeshCount() != mesh_count_before_purge ||
       render_system.sceneMaterialCount() != material_count_before_purge ||
       render_system.sceneInstanceCount() != instance_count_before_purge) {
      return false;
   }
   const auto cuboid_visible = render_system.objectVisible(*cuboid); ///< Surviving object still references live assets.
   return cuboid_visible && *cuboid_visible && render_system.sceneMeshCount() == 1U &&
          render_system.sceneMaterialCount() == 1U;
}

/// @brief Verifies a loaded light scene can be removed independently of unrelated objects, exactly once.
[[nodiscard]] bool hasSceneRemovalLifetime() {
   auto render_system = vve::simple::RenderSystem{};
   const auto missing_scene = render_system.removeScene(vve::SceneHandle{});
   if (missing_scene || missing_scene.error() != vve::Error::missing_object) { return false; }

   auto lights = vve::simple::Scene{};
   lights.pointLights.push_back(vve::simple::ForwardPointLight{});
   const auto scene = render_system.loadScene(std::move(lights));
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!scene.valid() || !plane || !plane->valid() || render_system.scenePointLightCount() != 1U) { return false; }

   // Objects do not belong to a loaded light scene, so they do not block its removal; its lights go with it.
   if (const auto removed_scene = render_system.removeScene(scene); !removed_scene) { return false; }
   if (render_system.scenePointLightCount() != 0U || render_system.sceneInstanceCount() != 1U) { return false; }
   const auto removed_again = render_system.removeScene(scene);
   return !removed_again && removed_again.error() == vve::Error::missing_object;
}

} // namespace

/// @brief Checks light storage and authoring defaults, then exercises the renderer and its GPU diagnostics.
int main() {
   // Verify directional-light vector semantics before Vulkan renderer setup.
   {
      vve::simple::RenderSystem render_system{};
      const auto &scene = render_system.forward().scene;
      const auto first_directional = vve::DirectionalLight{
         .direction = vve::Direction{.value = vve::Vec3{-0.25F, 0.90F, 0.10F}},
         .color = vve::LinearColor{.value = vve::Vec3{1.0F, 0.2F, 0.3F}},
         .intensity = vve::LightIntensity{.value = 1.0F},
         .ambient = vve::LinearColor{.value = vve::Vec3{0.01F, 0.02F, 0.03F}}};
      render_system.addDirectionalLight(first_directional.direction, first_directional.color,
         first_directional.intensity, first_directional.ambient);
      // Filling beyond the cap must keep the first authored light intact.
      for (std::size_t index{1U}; index <= vve::simple::kMaxDirectionalLights; ++index) {
         const auto value = static_cast<float>(index);
         render_system.addDirectionalLight(
            vve::Direction{.value = vve::Vec3{value, value + 0.25F, value + 0.50F}},
            vve::LinearColor{.value = vve::Vec3{0.1F * value, 0.2F * value, 0.3F * value}},
            vve::LightIntensity{.value = value + 1.0F},
            vve::LinearColor{.value = vve::Vec3{0.01F * value, 0.02F * value, 0.03F * value}});
      }
      if (scene.directionalLights.size() != vve::simple::kMaxDirectionalLights ||
          render_system.sceneDirectionalLightCount() != scene.directionalLights.size()) { return 20; }
      if (!render_system.hasSceneDirectionalLight() ||
          !sameDirectionalLight(scene.directionalLights.front(), first_directional)) {
         return 21;
      }
      const auto replacement_directional = vve::DirectionalLight{
         .direction = vve::Direction{.value = vve::Vec3{0.75F, -0.50F, 0.25F}},
         .color = vve::LinearColor{.value = vve::Vec3{0.4F, 0.8F, 1.0F}},
         .intensity = vve::LightIntensity{.value = 4.0F},
         .ambient = vve::LinearColor{.value = vve::Vec3{0.07F, 0.08F, 0.09F}}};
      render_system.setDirectionalLight(replacement_directional.direction, replacement_directional.color,
         replacement_directional.intensity, replacement_directional.ambient);
      if (scene.directionalLights.size() != 1U || render_system.sceneDirectionalLightCount() != 1U ||
          !render_system.hasSceneDirectionalLight() ||
          !sameDirectionalLight(scene.directionalLights.front(), replacement_directional)) {
         return 22;
      }

      // Both point-light spellings must use the facade descriptor's ambient default.
      const auto point = vve::PointLight{};
      render_system.addPointLight(point.position, point.color, point.intensity, point.range);
      render_system.addPointLight(point.position, point.color, point.intensity, point.range, point.ambient);
      if (scene.pointLights.size() != 2U || render_system.scenePointLightCount() != 2U ||
          !render_system.hasScenePointLight()) { return 43; }
      const bool ambient_equal = scene.pointLights[0].ambient == scene.pointLights[1].ambient &&
         scene.pointLights[0].ambient == point.ambient.value.x;
      std::println("D1 point_ambient_equal={} implicit={} explicit={}", ambient_equal,
         scene.pointLights[0].ambient, scene.pointLights[1].ambient);
      if (!ambient_equal) { return 43; }
      // Replacing a light without ambient must reset a previous explicit ambient to the descriptor default.
      const auto custom_ambient = vve::LinearColor{.value = vve::Vec3{0.3F, 0.3F, 0.3F}};
      render_system.setPointLight(point.position, point.color, point.intensity, point.range, custom_ambient);
      render_system.setPointLight(point.position, point.color, point.intensity, point.range);
      if (scene.pointLights.size() != 1U || scene.pointLights.front().ambient != point.ambient.value.x ||
          scene.ambient != point.ambient.value.x) { return 44; }
      const auto spot = vve::SpotLight{};
      render_system.addSpotLight(spot.position, spot.direction, spot.color, spot.intensity, spot.range, spot.cone);
      render_system.addSpotLight(spot.position, spot.direction, spot.color, spot.intensity, spot.range, spot.cone, spot.ambient);
      if (scene.spotLights.size() != 2U || render_system.sceneSpotLightCount() != 2U ||
          !render_system.hasSceneSpotLight() || scene.spotLights[0].ambient != scene.spotLights[1].ambient ||
          scene.spotLights[0].ambient != spot.ambient.value.x) { return 45; }
      render_system.setSpotLight(spot.position, spot.direction, spot.color, spot.intensity, spot.range, spot.cone, custom_ambient);
      render_system.setSpotLight(spot.position, spot.direction, spot.color, spot.intensity, spot.range, spot.cone);
      if (scene.spotLights.size() != 1U || scene.spotLights.front().ambient != spot.ambient.value.x) { return 46; }
      render_system.clearScene();
      if (render_system.sceneDirectionalLightCount() != 0U || render_system.scenePointLightCount() != 0U ||
          render_system.sceneSpotLightCount() != 0U || render_system.hasSceneDirectionalLight() ||
          render_system.hasScenePointLight() || render_system.hasSceneSpotLight()) { return 47; }
   }
   if (!hasCulledDraws()) { return 41; }
   if (!hasAmbientOnlyLightPacking() || !hasMultiWindowShadowGrowth()) { return 40; }

   auto engine = vve::test::hiddenEngine("simple-forward-renderer-tests", vve::PixelExtent{.width = 64, .height = 64}, vve::MaxFrames{.value = vve::FrameCount{.value = 1}});
   if (!engine.init()) { return 1; }

   if (!hasValidationMessageCounter()) { return 30; }
   auto &render_system = engine.renderSystem();
   render_system.clearScene();
   if (!hasPublicRenderObjectLifetime() || !hasBackendObjectVisibilityUpdate() ||
       !hasBackendObjectTransformUpdate()) {
      return 23;
   }
   if (!hasBackendObjectCorrespondenceWithoutPublicHandle()) { return 24; }
   if (!hasSceneRemovalLifetime()) { return 25; }
   if (!hasPurgeUnusedAssetsLifetime()) { return 26; }
   vve::simple::Scene point_shadow_scene{}; ///< Public scene-loading path carries multiple point lights.
   if (!hasRuntimeGpuObjectSynchronization()) { return 27; }
   point_shadow_scene.pointLights = {
      vve::simple::ForwardPointLight{.position = vve::Vec3{-2.0F, 2.75F, -1.25F},
                              .color = vve::Vec3{1.0F, 0.74F, 0.46F},
                              .intensity = 3.25F,
                              .range = 6.0F,
                              .ambient = 0.05F},
      vve::simple::ForwardPointLight{.position = vve::Vec3{2.25F, 3.25F, 1.50F},
                              .color = vve::Vec3{0.45F, 0.70F, 1.0F},
                              .intensity = 2.75F,
                              .range = 7.0F,
                              .ambient = 0.04F}};
   point_shadow_scene.spotLights = {
      vve::simple::ForwardSpotLight{.position = vve::Vec3{-3.0F, 3.0F, 0.0F},
                             .direction = vve::Vec3{1.0F, -1.0F, 0.0F},
                             .color = vve::Vec3{1.0F, 0.0F, 0.0F},
                             .intensity = vve::LightIntensity{.value = 9.0F},
                             .range = vve::LightRange{.value = 3.0F},
                             .innerConeAngle = vve::SpotConeAngle{.radians = 0.25F},
                             .outerConeAngle = vve::SpotConeAngle{.radians = 0.45F},
                             .ambient = 0.01F,
                             .enabled = false}}; ///< Regression source: disabled lights must not be packed.
   render_system.loadScene(std::move(point_shadow_scene));
   if (const auto result = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{}); !result) {
      return 2;
   }
   render_system.setCamera(vve::Camera{});
   render_system.setDirectionalLight(vve::Direction{.value = vve::Vec3{0.25F, -1.0F, 0.35F}},
                                     vve::LinearColor{}, vve::LightIntensity{},
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
   render_system.addSpotLight(vve::Position{.value = vve::Vec3{0.25F, 5.0F, 1.75F}},
                              vve::Direction{.value = vve::Vec3{-0.05F, -1.0F, -0.50F}},
                              vve::LinearColor{.value = vve::Vec3{0.65F, 1.0F, 0.7F}},
                              vve::LightIntensity{.value = 2.75F}, vve::LightRange{.value = 8.0F},
                              vve::SpotConeAngle{.radians = 0.60F}); ///< Third cone proves metadata uniqueness beyond two slots.

   // Verify the submitted scene is visible through the public facade before frame submission.
   if (render_system.sceneMeshCount() != 1 || render_system.sceneMaterialCount() != 1 ||
       render_system.sceneInstanceCount() != 1 || render_system.sceneVertexCount() != 4 ||
       render_system.sceneIndexCount() != 6 || !render_system.hasSceneCamera() ||
       !render_system.hasSceneDirectionalLight()) {
      return 3;
   }

   render_system.forward().setGpuDebugReadback(true);
   if (const auto result = engine.renderFrame(); !result) { return 4; }
   // The point-shadow scene packs two points and three spots; the disabled spot consumes no uniform slot.
   const auto &frame_uniforms = render_system.forward().frameUniforms();
   std::println("[SimpleForwardRendererTests] activePointLightCount={} activeSpotLightCount={} expected=2,3",
      frame_uniforms.activePointLightCount, frame_uniforms.activeSpotLightCount);
   if (frame_uniforms.activePointLightCount != 2U || frame_uniforms.activeSpotLightCount != 3U) { return 42; }
   if (!engine.gui().ready()) { return 32; }
   const auto status = engine.step();
   if (!status || *status != vve::FrameStatus::stopped) { return 4; }

   // Verify per-frame assembly created one unique spot-shadow metadata row per shadowed spot light.
   constexpr std::size_t expected_shadowed_spot_count{3U}; ///< Focused test scene light count.
   constexpr std::size_t expected_shadowed_point_count{2U}; ///< Focused point-light metadata count.
   constexpr std::size_t expected_shadowed_directional_count{1U}; ///< Focused directional-light cascade count.
   if (expected_shadowed_spot_count > vve::simple::kMaxShadowedSpotLights ||
       !hasUniqueSpotShadowMeta(render_system, expected_shadowed_spot_count)) {
      return 15;
   }
   if (!hasPointShadowMetaInvariants(render_system, expected_shadowed_spot_count,
                                     expected_shadowed_point_count, expected_shadowed_directional_count)) {
      return 16;
   }
   if (!hasDisabledFirstSpotExcludedFromPackedMeta(render_system, 3.0F, 6.0F)) { return 19; }

   if (render_system.renderedFrameCount() != 1 || render_system.lastRenderedWindowCount() != 1) { return 5; }
   const auto &forward_renderer = render_system.forward();
   if (!hasExpectedBufferMemory(forward_renderer)) { return 40; }
   std::println("R1 shadow layers dir/spot/point={}/{}/{}", forward_renderer.dirShadowArray.layerCount,
      forward_renderer.spotShadowArray.layerCount, forward_renderer.pointShadowArray.layerCount);
   if (forward_renderer.dirShadowArray.layerCount != 4U || forward_renderer.spotShadowArray.layerCount != 3U ||
       forward_renderer.pointShadowArray.layerCount != 12U) { return 38; }
   const auto initial_shadow_passes = forward_renderer.lastShadowLayerPassCount();
   if (initial_shadow_passes != 19U) { return 39; }
   if (!hasRecordedShadowsBeforeForwardColor(forward_renderer)) { return 17; }
	using RecordedPass = vve::simple::ForwardRenderer::RecordedPass; ///< Tags for optional post-forward passes.
	if (std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::gui) != 0 ||
		std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::post_process) != 0) { return 35; }

   // Directional shadow-depth diagnostics are recorded by engine.renderFrame() for light zero, cascade zero.
   const auto directional_samples = samplesOfType(forward_renderer, 3U); ///< Single retained light-space sample.
   if (directional_samples.size() != 1U || directional_samples.front().light_index != 0U ||
       directional_samples.front().layer != 0U || directional_samples.front().bias != vve::simple::ForwardRenderer::directionalCompareBias) {
      return 8;
   }

   // Verify point shadow-depth diagnostics retain one GPU sample per shadowed point light.
   if (!hasPointShadowDepthSamples(forward_renderer, render_system, expected_shadowed_point_count)) {
      return 8;
   }
   if (!hasFullContributionForNonOccludedShadowSamples(forward_renderer)) { return 18; }
   if (!hasDirectionalShadowDepthAgreement(forward_renderer)) { return 28; }

   // Verify each active spot light has a sample and a unique shadow-array slot.
   const auto capped_spot_lights = std::views::take(forward_renderer.scene.spotLights,
                                                    vve::simple::kMaxShadowedSpotLights); ///< CPU scene cap.
   const std::size_t active_spot_light_count{static_cast<std::size_t>(
      std::ranges::count_if(capped_spot_lights, &vve::simple::ForwardSpotLight::enabled))}; ///< Enabled shader slots only.
   if (active_spot_light_count != std::min<std::size_t>(3U, vve::simple::kMaxShadowedSpotLights)) { return 11; }
   const auto &first_spot = forward_renderer.scene.spotLights[0]; ///< First retained engine spot light.
   const auto &second_spot = forward_renderer.scene.spotLights[1]; ///< Second retained engine spot light.
   if ((first_spot.position.x == second_spot.position.x && first_spot.position.y == second_spot.position.y &&
        first_spot.position.z == second_spot.position.z) ||
       (first_spot.direction.x == second_spot.direction.x && first_spot.direction.y == second_spot.direction.y &&
        first_spot.direction.z == second_spot.direction.z)) {
      return 12;
   }
   const auto spot_samples = samplesOfType(forward_renderer, 1U); ///< One sample per packed spot light after a rendered frame.
   if (spot_samples.size() != active_spot_light_count) { return 13; }
   std::vector<std::uint32_t> spot_shadow_layers{}; ///< Each active spot light owns its own shadow-array layer.
   for (const auto &sample : spot_samples) { spot_shadow_layers.push_back(sample.layer); }
   std::ranges::sort(spot_shadow_layers);
   if (std::ranges::adjacent_find(spot_shadow_layers) != spot_shadow_layers.end()) { return 13; }

   // A zero-intensity spot keeps its ambient lighting slot but adds no shadow clear or draw.
   render_system.addSpotLight(vve::Position{{0.0F, 4.0F, 0.0F}}, vve::Direction{{0.0F, -1.0F, 0.0F}},
      vve::LinearColor{}, vve::LightIntensity{0.0F}, vve::LightRange{10.0F}, vve::SpotConeAngle{0.5F}, vve::LinearColor{{0.2F, 0.2F, 0.2F}});
   if (!engine.renderFrame()) { return 39; }
   std::println("R1 lastShadowLayerPassCount before={} after={}", initial_shadow_passes, forward_renderer.lastShadowLayerPassCount());
   if (forward_renderer.lastShadowLayerPassCount() != 19U || samplesOfType(forward_renderer, 1U).size() != 3U ||
       forward_renderer.spotShadowArray.layerCount != 4U) { return 39; }
   if (!hasDirectionalRuntimeToggleCoverage(engine, render_system)) { return 29; }
	// Disabling readback must discard the previous frame's samples and skip rebuilding them.
	render_system.forward().setGpuDebugReadback(false);
	if (!engine.renderFrame()) { return 34; }
	std::println("SimpleForwardRendererTests readback_off_samples={}", forward_renderer.shadowDepthSamples.size());
	if (!forward_renderer.shadowDepthSamples.empty() || !render_system.shadowDepthSamples().empty()) { return 34; }
	// An installed callback with no draw data must also skip the GUI pass.
	std::size_t gui_calls{};
	engine.gui().draw([&]{ ++gui_calls; });
	if (!engine.renderFrame() || gui_calls != 1U ||
		std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::gui) != 0) { return 35; }
	// An explicitly sized window produces vertices immediately, even on its first hidden-window frame.
	engine.gui().draw([&] {
		++gui_calls;
		ImGui::SetNextWindowPos(ImVec2{0, 0});
		ImGui::SetNextWindowSize(ImVec2{64, 64});
		ImGui::Begin("GUI pass test", nullptr, ImGuiWindowFlags_NoSavedSettings);
		ImGui::TextUnformatted("Visible GUI");
		ImGui::End();
	});
	if (!engine.renderFrame() || gui_calls != 2U) { return 36; }
	const auto passes = forward_renderer.lastRecordedPassOrder();
	std::println("SimpleForwardRendererTests gui_passes={} post_process_passes={} gui_calls={}",
		std::ranges::count(passes, RecordedPass::gui), std::ranges::count(passes, RecordedPass::post_process), gui_calls);
	if (std::ranges::count(passes, RecordedPass::gui) != 1 || passes.back() != RecordedPass::gui ||
		std::ranges::count(passes, RecordedPass::post_process) != 0) { return 36; }
	// Removing the callback must not reuse the preceding frame's draw data.
	engine.gui().draw({});
	if (!engine.renderFrame() || std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::gui) != 0) { return 37; }
	// First render a spot, so switching every spot checkbox off must remove an existing shadow pass.
	const auto spot = vve::SpotLight{};
	render_system.setSpotLight(spot.position, spot.direction, spot.color, spot.intensity, spot.range, spot.cone, spot.ambient);
	if (!engine.renderFrame() ||
		std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::spot_shadow) != 1) { return 48; }
	// Rebuild the enabled lights as testscene does when every spot checkbox is off.
	render_system.clearLights();
	const auto directional = vve::DirectionalLight{};
	const auto point = vve::PointLight{};
	render_system.addDirectionalLight(directional.direction, directional.color, directional.intensity, directional.ambient);
	render_system.addPointLight(point.position, point.color, point.intensity, point.range, point.ambient);
	if (!engine.renderFrame()) { return 48; }
	const auto spot_passes = std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::spot_shadow);
	std::println("E2 all_spots_off spot_shadow_passes={} active_spots={}",
		spot_passes, forward_renderer.frameUniforms().activeSpotLightCount);
	if (spot_passes != 0 || forward_renderer.frameUniforms().activeSpotLightCount != 0U ||
		std::ranges::count(forward_renderer.lastRecordedPassOrder(), RecordedPass::forward_color) != 1) { return 48; }
	// Camera clip planes must reach the projection uploaded for the current window.
	const vve::Camera clipped_camera{.fov_y = {0.8F}, .clip = {0.5F, 50.0F}};
	render_system.setCamera(clipped_camera);
	if (!engine.renderFrame()) { return 49; }
	const auto extent = forward_renderer.targets.front().swapchain.extent;
	const auto expected_projection = vve::math::perspectiveVulkan(clipped_camera.fov_y.radians,
		static_cast<vve::Scalar>(extent.width) / static_cast<vve::Scalar>(extent.height), 0.5F, 50.0F);
	const auto &projection = forward_renderer.frameUniforms().projection;
	vve::Scalar projection_error{}; ///< Largest element difference from the requested projection.
	// Check every element so both depth mapping and the window aspect are covered.
	for (const auto column : std::views::iota(0, 4)) {
		for (const auto row : std::views::iota(0, 4)) {
			const auto error = std::abs(projection[column][row] - expected_projection[column][row]);
			if (!std::isfinite(error)) { return 49; }
			projection_error = std::max(projection_error, error);
		}
	}
	std::println("D11a camera_clip near={} far={} projection_error={}",
		clipped_camera.clip.near_plane, clipped_camera.clip.far_plane, projection_error);
	if (projection_error > 1e-5F) { return 49; }
	render_system.waitIdle();
   // Ordinary hidden-window rendering must not need the bounded-wait retry path.
   std::println("SimpleForwardRendererTests skippedFrameCount={}", forward_renderer.skippedFrameCount());
   if (forward_renderer.skippedFrameCount() != 0U) { return 33; }
   std::cout << "SimpleForwardRendererTests validationActive=" << forward_renderer.validationActive()
      << " validationErrorCount=" << forward_renderer.validationErrorCount() << '\n';
   if (forward_renderer.validationActive() && forward_renderer.validationErrorCount() != 0U) { return 31; }

   return 0;
}
