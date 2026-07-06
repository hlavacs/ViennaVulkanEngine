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

/// @brief Compares authored directional-light fields used by the simple render scene.
[[nodiscard]] bool sameDirectionalLight(const vve::simple::RenderDirectionalLight &left,
                                        const vve::simple::RenderDirectionalLight &right) {
   return left.direction_to_light.value.x == right.direction_to_light.value.x &&
          left.direction_to_light.value.y == right.direction_to_light.value.y &&
          left.direction_to_light.value.z == right.direction_to_light.value.z &&
          left.color.value.x == right.color.value.x && left.color.value.y == right.color.value.y &&
          left.color.value.z == right.color.value.z && left.intensity.value == right.intensity.value &&
          left.ambient.value.x == right.ambient.value.x && left.ambient.value.y == right.ambient.value.y &&
          left.ambient.value.z == right.ambient.value.z;
}

/// @brief Verifies each shadowed spot light owns one unique shadow metadata slot and layer.
[[nodiscard]] bool hasUniqueSpotShadowMeta(const vve::simple::RenderSystem &render_system,
                                           std::size_t expected_spot_count) {
   if (render_system.sceneShadowLightMetaCount() < expected_spot_count) { return false; }

   std::vector<std::uint32_t> shadow_slots{}; ///< Metadata shadow resource slots.
   std::vector<std::uint32_t> first_layers{}; ///< Metadata depth-array first layers.
   shadow_slots.reserve(expected_spot_count);
   first_layers.reserve(expected_spot_count);

   // Collect every prepared metadata row through the public renderer diagnostic surface.
   for (std::size_t index{}; index < expected_spot_count; ++index) {
      const auto meta = render_system.sceneShadowLightMeta(index);
      if (!meta || meta->light_type != 1U || meta->layer_count != 1U) { return false; }
      shadow_slots.push_back(meta->shadow_slot);
      first_layers.push_back(meta->first_layer);
   }

   std::ranges::sort(shadow_slots);
   std::ranges::sort(first_layers);
   const auto unique_shadow_slots = std::ranges::unique(shadow_slots); ///< Pairwise shadow-slot proof.
   const auto unique_first_layers = std::ranges::unique(first_layers); ///< Pairwise first-layer proof.
   return static_cast<std::size_t>(unique_shadow_slots.begin() - shadow_slots.begin()) == expected_spot_count &&
          static_cast<std::size_t>(unique_first_layers.begin() - first_layers.begin()) == expected_spot_count;
}

/// @brief Verifies disabled spot lights are absent from dense shader-visible shadow metadata.
[[nodiscard]] bool hasDisabledFirstSpotExcludedFromPackedMeta(const vve::simple::RenderSystem &render_system,
                                                              float disabled_range, float first_enabled_range) {
   if (render_system.sceneShadowLightMetaCount() == 0U) { return false; }
   const auto first_meta = render_system.sceneShadowLightMeta(0); ///< First packed row must belong to an enabled light.
   if (!first_meta || first_meta->light_type != 1U || first_meta->light_index != 0U ||
       first_meta->shadow_slot != 0U || first_meta->first_layer != 0U ||
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
                                                std::size_t spot_row_count, std::size_t point_light_count) {
   constexpr std::size_t point_shadow_face_count{6U}; ///< Cubemap-style point shadows render six views.
   if (point_light_count > vve::simple::kMaxShadowedPointLights) { return false; } ///< CPU metadata cap.
   const std::size_t expected_point_rows{point_shadow_face_count * point_light_count}; ///< Six rows per point light.
   if (render_system.sceneShadowLightMetaCount() != spot_row_count + expected_point_rows) { return false; }

   std::uint32_t max_spot_first_layer{}; ///< Highest spot layer must remain below every point layer.
   // Read the spot rows first so point rows can be checked for non-overlapping array layers.
   for (std::size_t spot_index{}; spot_index < spot_row_count; ++spot_index) {
      const auto meta = render_system.sceneShadowLightMeta(spot_index);
      if (!meta || meta->light_type != 1U) { return false; }
      max_spot_first_layer = std::max(max_spot_first_layer, meta->first_layer);
   }

   std::vector<std::vector<std::uint32_t>> layers_by_light(point_light_count); ///< Layers grouped by source light.
   std::vector<std::uint32_t> all_point_layers{};                              ///< Global uniqueness proof.
   all_point_layers.reserve(expected_point_rows);

   // Walk every point metadata row and check the public CPU contract for cubemap-style faces.
   for (std::size_t row{spot_row_count}; row < render_system.sceneShadowLightMetaCount(); ++row) {
      const auto meta = render_system.sceneShadowLightMeta(row);
      if (!meta || meta->light_type != 2U || meta->layer_count != 1U) { return false; }
      if (meta->light_index >= point_light_count || meta->light_index >= vve::simple::kMaxShadowedPointLights) {
         return false;
      }
      if (meta->first_layer <= max_spot_first_layer) { return false; }
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
   return true;
}

/// @brief Selects the same point-shadow face as the forward fragment shader for the origin debug sample.
[[nodiscard]] std::uint32_t pointShadowDebugFace(const vve::simple::PointLight &light) {
   const vve::Vec3 light_to_origin{-light.position.x, -light.position.y, -light.position.z}; ///< Fixed debug point is world origin.
   const vve::Vec3 abs_light_to_origin{std::abs(light_to_origin.x), std::abs(light_to_origin.y),
                                       std::abs(light_to_origin.z)}; ///< Dominant-axis selector inputs.
   return abs_light_to_origin.x >= abs_light_to_origin.y && abs_light_to_origin.x >= abs_light_to_origin.z
             ? (light_to_origin.x >= 0.0F ? 0U : 1U)
             : (abs_light_to_origin.y >= abs_light_to_origin.z ? (light_to_origin.y >= 0.0F ? 2U : 3U)
                                                               : (light_to_origin.z >= 0.0F ? 4U : 5U));
}

/// @brief Verifies retained point shadow-depth samples expose one CPU diagnostic row per point light.
[[nodiscard]] bool hasPointShadowDepthSamples(const vve::simple::ForwardRenderer &renderer,
                                              const vve::simple::RenderSystem &render_system,
                                              std::size_t point_light_count) {
   if (renderer.scenePointShadowDepthSampleCount() != point_light_count) { return false; }
   if (render_system.scenePointShadowDepthSampleCount() != renderer.scenePointShadowDepthSampleCount()) { return false; }

   std::vector<std::uint32_t> point_slots{}; ///< RenderShadowDepthSample::triangle_id carries the point slot.
   std::vector<std::uint32_t> selected_layers{}; ///< RenderShadowDepthSample::pixel_x carries the dense array layer.
   point_slots.reserve(point_light_count);
   selected_layers.reserve(point_light_count);

   // Each active point light contributes one origin sample using its selected cubemap face.
   for (std::size_t point_index{}; point_index < point_light_count; ++point_index) {
      const auto sample = renderer.scenePointShadowDepthSample(point_index);
      const std::uint32_t expected_face{pointShadowDebugFace(renderer.scene.pointLights[point_index])};
      const std::uint32_t expected_layer{static_cast<std::uint32_t>(
         vve::simple::kMaxShadowedSpotLights + point_index * 6U + expected_face)};
      const float expected_error{sample ? sample->expected_depth - sample->gpu_depth : 0.0F};
      const float expected_shadow_factor{sample && sample->light_ndc.z - sample->bias > sample->gpu_depth ? 0.35F : 1.0F};
      const auto retained_error = renderer.scenePointShadowDepthError(point_index);
      const auto facade_sample = render_system.scenePointShadowDepthSample(point_index); ///< Facade-retained sample.
      const auto facade_gpu_depth = render_system.scenePointShadowDepthGpuDepth(point_index);
      const auto facade_has_gpu = render_system.scenePointShadowDepthHasGpu(point_index);
      const auto facade_error = render_system.scenePointShadowDepthError(point_index);
      if (!sample || !sample->valid || !sample->has_gpu || !std::isfinite(sample->gpu_depth) ||
          !std::isfinite(sample->error) || !retained_error || sample->triangle_id != point_index ||
          sample->face_index != expected_face || sample->face_index >= 6U || sample->pixel_x != expected_layer ||
          sample->world.x != 0.0F || sample->world.y != 0.0F || sample->world.z != 0.0F ||
          sample->expected_depth != sample->light_ndc.z || sample->bias != 0.001F ||
          sample->error != expected_error || *retained_error != sample->error ||
          sample->shadow_factor != expected_shadow_factor || !facade_sample ||
          facade_sample->triangle_id != sample->triangle_id || facade_sample->face_index != sample->face_index ||
          facade_sample->pixel_x != sample->pixel_x || facade_sample->expected_depth != sample->expected_depth ||
          facade_sample->gpu_depth != sample->gpu_depth || facade_sample->error != sample->error ||
          !facade_gpu_depth || *facade_gpu_depth != sample->gpu_depth || !facade_has_gpu || !*facade_has_gpu ||
          !facade_error || *facade_error != sample->error) {
         return false;
      }
      point_slots.push_back(sample->triangle_id);
      selected_layers.push_back(sample->pixel_x);
   }

   std::ranges::sort(point_slots);
   std::ranges::sort(selected_layers);
   const auto unique_point_slots = std::ranges::unique(point_slots); ///< One sample per source point light.
   const auto unique_selected_layers = std::ranges::unique(selected_layers); ///< No selected point layer is reused.
   return static_cast<std::size_t>(unique_point_slots.begin() - point_slots.begin()) == point_light_count &&
          static_cast<std::size_t>(unique_selected_layers.begin() - selected_layers.begin()) == point_light_count &&
          !renderer.scenePointShadowDepthSample(point_light_count) && !renderer.scenePointShadowDepthError(point_light_count) &&
          !render_system.scenePointShadowDepthSample(point_light_count) &&
          !render_system.scenePointShadowDepthGpuDepth(point_light_count) &&
          !render_system.scenePointShadowDepthHasGpu(point_light_count) &&
          !render_system.scenePointShadowDepthError(point_light_count);
}

/// @brief Verifies non-occluded retained shadow samples keep full light contribution.
[[nodiscard]] bool hasFullContributionForNonOccludedShadowSamples(const vve::simple::ForwardRenderer &renderer) {
   constexpr float full_shadow_factor{1.0F}; ///< Renderer.ixx full-contribution shadow factor.
   bool saw_non_occluded_sample{};           ///< At least one retained sample proves normal lighting is preserved.

   const auto check_full_factor = [&](const std::optional<vve::simple::RenderShadowDepthSample> &sample) {
      if (!sample || !sample->valid) { return false; }
      const bool occluded{sample->has_gpu && sample->light_ndc.z - sample->bias > sample->gpu_depth}; ///< CPU compare.
      if (!occluded) {
         saw_non_occluded_sample = true;
         return sample->shadow_factor == full_shadow_factor;
      }
      return true;
   };

   // Directional, spot, and point diagnostics all expose the same full-contribution value when unoccluded.
   for (std::size_t index{}; index < renderer.sceneShadowDepthSampleCount(); ++index) {
      if (!check_full_factor(renderer.sceneShadowDepthSample(index))) { return false; }
   }
   for (std::size_t index{}; index < renderer.sceneSpotShadowDepthSampleCount(); ++index) {
      if (!check_full_factor(renderer.sceneSpotShadowDepthSample(index))) { return false; }
   }
   for (std::size_t index{}; index < renderer.scenePointShadowDepthSampleCount(); ++index) {
      const auto sample = renderer.scenePointShadowDepthSample(index);
      if (!check_full_factor(sample)) { return false; }
      if (sample && sample->has_gpu && sample->light_ndc.z - sample->bias > sample->gpu_depth &&
          sample->shadow_factor >= full_shadow_factor) {
         return false;
      }
   }
   return saw_non_occluded_sample;
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
   if (!plane || !cuboid || !plane->valid() || !cuboid->valid() || *plane == *cuboid) { return false; }
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
   if (instance_count_before_remove != 2U ||
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

   const auto missing = vve::makeHandleForTest<vve::RenderObjectHandle>(9'999U);
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
   return !cleared_visible && cleared_visible.error() == vve::Error::missing_object;
}

/// @brief Verifies object visibility updates the renderer-owned backend draw flag.
[[nodiscard]] bool hasBackendObjectVisibilityUpdate() {
   auto render_system = vve::simple::RenderSystem{};
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!plane) { return false; }

   auto &forward = std::get<vve::simple::ForwardRenderer>(render_system.backend()); ///< Backend scene mirror.
   if (forward.scene.objects.empty() || !forward.scene.objects.front().visible) { return false; }
   if (const auto hidden = render_system.setObjectVisible(*plane, false); !hidden) { return false; }
   const auto hidden_state = render_system.objectVisible(*plane);
   if (!hidden_state || *hidden_state || forward.scene.objects.front().visible) { return false; }
   if (const auto shown = render_system.setObjectVisible(*plane, true); !shown) { return false; }
   const auto shown_state = render_system.objectVisible(*plane);

   const auto missing = vve::makeHandleForTest<vve::RenderObjectHandle>(9'999U);
   const auto missing_hide = render_system.setObjectVisible(missing, false);
   const auto missing_visible = render_system.objectVisible(missing);
   return shown_state && *shown_state && forward.scene.objects.front().visible &&
          !missing_hide && missing_hide.error() == vve::Error::missing_object &&
          !missing_visible && missing_visible.error() == vve::Error::missing_object;
}

/// @brief Verifies object movement updates the renderer-owned backend model matrix.
[[nodiscard]] bool hasBackendObjectTransformUpdate() {
   auto render_system = vve::simple::RenderSystem{};
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!plane) { return false; }

   const auto transform = vve::Transform{.translation = vve::Position{.value = vve::Vec3{1.25F, 2.5F, -3.75F}},
                                         .scale = vve::Scale{.value = vve::Vec3{2.0F, 0.5F, 1.5F}}};
   if (const auto moved = render_system.setObjectTransform(*plane, transform); !moved) { return false; }

   auto &forward = std::get<vve::simple::ForwardRenderer>(render_system.backend()); ///< Backend scene mirror.
   const auto missing = vve::makeHandleForTest<vve::RenderObjectHandle>(9'999U);
   const auto missing_move = render_system.setObjectTransform(missing, transform);
   return !forward.scene.objects.empty() &&
          forward.scene.objects.front().model[3].x == transform.translation.value.x &&
          forward.scene.objects.front().model[3].y == transform.translation.value.y &&
          forward.scene.objects.front().model[3].z == transform.translation.value.z &&
          !missing_move && missing_move.error() == vve::Error::missing_object;
}

/// @brief Verifies public object removal skips backend objects loaded without public handles.
[[nodiscard]] bool hasBackendObjectCorrespondenceWithoutPublicHandle() {
   auto render_system = vve::simple::RenderSystem{};
   auto loaded_scene = vve::simple::makeSampleScene();
   const std::size_t loaded_object_count{loaded_scene.objects.size()}; ///< Objects without public RenderObjectHandle.
   render_system.loadScene(std::move(loaded_scene));
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   const auto cuboid = render_system.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
                                               vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{});
   if (!plane || !cuboid) { return false; }

   auto &forward = std::get<vve::simple::ForwardRenderer>(render_system.backend());
   if (forward.scene.objects.size() != loaded_object_count + 2U || render_system.sceneInstanceCount() != 2U) {
      return false;
   }
   if (const auto removed = render_system.removeObject(*plane); !removed) { return false; }
   const auto cuboid_visible = render_system.objectVisible(*cuboid);
   return forward.scene.objects.size() == loaded_object_count + 1U &&
          forward.scene.objects.front().model[3][0] == 0.0F &&
          render_system.sceneInstanceCount() == 1U && cuboid_visible && *cuboid_visible;
}

/// @brief Verifies asset purging only removes mesh and material data after public objects stop referencing it.
[[nodiscard]] bool hasPurgeUnusedAssetsLifetime() {
   auto render_system = vve::simple::RenderSystem{};
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   const auto cuboid = render_system.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
                                               vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{});
   if (!plane || !cuboid || !plane->valid() || !cuboid->valid()) { return false; }

   constexpr std::size_t object_count{2U};                  ///< Plane and cuboid each mint one instance.
   constexpr std::size_t assets_per_removed_object{2U};     ///< One mesh plus one material becomes unused.
   const std::size_t live_mesh_count{render_system.sceneMeshCount()};           ///< Meshes before no-op purge.
   const std::size_t live_material_count{render_system.sceneMaterialCount()};   ///< Materials before no-op purge.
   const std::size_t live_instance_count{render_system.sceneInstanceCount()};   ///< Instances before no-op purge.
   if (live_mesh_count != object_count || live_material_count != object_count ||
       live_instance_count != object_count || render_system.purgeUnusedAssets() != 0U ||
       render_system.sceneMeshCount() != live_mesh_count ||
       render_system.sceneMaterialCount() != live_material_count ||
       render_system.sceneInstanceCount() != live_instance_count) {
      return false;
   }

   if (const auto removed = render_system.removeObject(*plane); !removed) { return false; }
   const std::size_t mesh_count_before_purge{render_system.sceneMeshCount()};           ///< Removed object assets still exist.
   const std::size_t material_count_before_purge{render_system.sceneMaterialCount()};   ///< Removed object assets still exist.
   const std::size_t instance_count_before_purge{render_system.sceneInstanceCount()};   ///< Purge must not remove instances.
   if (mesh_count_before_purge != live_mesh_count ||
       material_count_before_purge != live_material_count ||
       instance_count_before_purge != live_instance_count - 1U) {
      return false;
   }

   if (render_system.purgeUnusedAssets() != assets_per_removed_object ||
       render_system.sceneMeshCount() != mesh_count_before_purge - 1U ||
       render_system.sceneMaterialCount() != material_count_before_purge - 1U ||
       render_system.sceneInstanceCount() != instance_count_before_purge) {
      return false;
   }
   const auto cuboid_visible = render_system.objectVisible(*cuboid); ///< Surviving object still references live assets.
   return cuboid_visible && *cuboid_visible && render_system.sceneMeshCount() == 1U &&
          render_system.sceneMaterialCount() == 1U;
}

/// @brief Verifies scene asset removal keeps loaded scenes alive while public render objects reference them.
[[nodiscard]] bool hasSceneRemovalLifetime() {
   auto render_system = vve::simple::RenderSystem{};
   const auto missing_scene = render_system.removeScene(vve::SceneHandle{});
   if (missing_scene || missing_scene.error() != vve::Error::missing_object) { return false; }

   const auto scene = render_system.loadScene(vve::simple::makeSampleScene());
   const auto plane = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{});
   if (!scene.valid() || !plane || !plane->valid()) { return false; }

   const auto referenced_scene = render_system.removeScene(scene);
   if (referenced_scene || referenced_scene.error() != vve::Error::invalid_argument) { return false; }

   // The failed scene removal must not erase the loaded scene while public objects still exist.
   if (const auto removed_object = render_system.removeObject(*plane); !removed_object) { return false; }
   if (const auto removed_scene = render_system.removeScene(scene); !removed_scene) { return false; }
   const auto removed_again = render_system.removeScene(scene);
   return !removed_again && removed_again.error() == vve::Error::missing_object;
}

} // namespace

int main() {
   if (!hasForwardShadowBeforeColorContract()) { return 10; }

   // Verify directional-light vector semantics before Vulkan renderer setup.
   vve::simple::RenderScene scene{};
   const auto first_directional = vve::simple::RenderDirectionalLight{
      .direction_to_light = vve::Direction{.value = vve::Vec3{-0.25F, 0.90F, 0.10F}},
      .color = vve::LinearColor{.value = vve::Vec3{1.0F, 0.2F, 0.3F}},
      .intensity = vve::LightIntensity{.value = 1.0F},
      .ambient = vve::LinearColor{.value = vve::Vec3{0.01F, 0.02F, 0.03F}}};
   scene.addDirectionalLight(first_directional);
   for (std::size_t index{1U}; index <= vve::simple::kMaxDirectionalLights; ++index) {
      const auto value = static_cast<float>(index);
      scene.addDirectionalLight(vve::simple::RenderDirectionalLight{
         .direction_to_light = vve::Direction{.value = vve::Vec3{value, value + 0.25F, value + 0.50F}},
         .color = vve::LinearColor{.value = vve::Vec3{0.1F * value, 0.2F * value, 0.3F * value}},
         .intensity = vve::LightIntensity{.value = value + 1.0F},
         .ambient = vve::LinearColor{.value = vve::Vec3{0.01F * value, 0.02F * value, 0.03F * value}}});
   }
   if (scene.directionalLights().size() != vve::simple::kMaxDirectionalLights) { return 20; }
   if (!sameDirectionalLight(scene.directionalLights().front(), first_directional) ||
       !scene.directionalLight() || !sameDirectionalLight(*scene.directionalLight(), first_directional)) {
      return 21;
   }
   const auto replacement_directional = vve::simple::RenderDirectionalLight{
      .direction_to_light = vve::Direction{.value = vve::Vec3{0.75F, -0.50F, 0.25F}},
      .color = vve::LinearColor{.value = vve::Vec3{0.4F, 0.8F, 1.0F}},
      .intensity = vve::LightIntensity{.value = 4.0F},
      .ambient = vve::LinearColor{.value = vve::Vec3{0.07F, 0.08F, 0.09F}}};
   scene.setDirectionalLight(replacement_directional);
   if (scene.directionalLights().size() != 1U ||
       !sameDirectionalLight(scene.directionalLights().front(), replacement_directional) ||
       !scene.directionalLight() || !sameDirectionalLight(*scene.directionalLight(), replacement_directional)) {
      return 22;
   }

   auto engine = vve::simple::Engine{
      vve::ApplicationName{"simple-forward-renderer-tests"},
      vve::MaxFrames{.value = vve::FrameCount{.value = 1}},
      vve::WindowSetups{vve::WindowSetup{}
                           .id("main")
                           .title("simple-forward-renderer-tests")
                           .extent(vve::PixelExtent{.width = 64, .height = 64})
                           .renderer(vve::RendererId{.value = "forward"})
                           .visible(false)}};
   if (!engine.init()) { return 1; }

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
   point_shadow_scene.pointLights = {
      vve::simple::PointLight{.position = vve::Vec3{-2.0F, 2.75F, -1.25F},
                              .color = vve::Vec3{1.0F, 0.74F, 0.46F},
                              .intensity = 3.25F,
                              .range = 6.0F,
                              .ambient = 0.05F},
      vve::simple::PointLight{.position = vve::Vec3{2.25F, 3.25F, 1.50F},
                              .color = vve::Vec3{0.45F, 0.70F, 1.0F},
                              .intensity = 2.75F,
                              .range = 7.0F,
                              .ambient = 0.04F}};
   point_shadow_scene.pointLight = point_shadow_scene.pointLights.front(); ///< Preserve legacy single-light mirror.
   point_shadow_scene.spotLights = {
      vve::simple::SpotLight{.position = vve::Vec3{-3.0F, 3.0F, 0.0F},
                             .direction = vve::Vec3{1.0F, -1.0F, 0.0F},
                             .color = vve::Vec3{1.0F, 0.0F, 0.0F},
                             .intensity = vve::LightIntensity{.value = 9.0F},
                             .range = vve::LightRange{.value = 3.0F},
                             .innerConeAngle = vve::SpotConeAngle{.radians = 0.25F},
                             .outerConeAngle = vve::SpotConeAngle{.radians = 0.45F},
                             .ambient = 0.01F,
                             .enabled = false}}; ///< Regression source: disabled lights must not be packed.
   point_shadow_scene.spotLight = point_shadow_scene.spotLights.front(); ///< Preserve legacy single-light mirror.
   render_system.loadScene(std::move(point_shadow_scene));
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

#ifndef NDEBUG
   std::get<vve::simple::ForwardRenderer>(render_system.backend()).setGpuDebugReadback(true);
#endif
   if (const auto result = engine.renderFrame(); !result) { return 4; }
   const auto status = engine.step();
   if (!status || *status != vve::FrameStatus::stopped) { return 4; }

   // Verify per-frame assembly created one unique spot-shadow metadata row per shadowed spot light.
   constexpr std::size_t expected_shadowed_spot_count{3U}; ///< Focused test scene light count.
   constexpr std::size_t expected_shadowed_point_count{2U}; ///< Focused point-light metadata count.
   if (expected_shadowed_spot_count > vve::simple::kMaxShadowedSpotLights ||
       !hasUniqueSpotShadowMeta(render_system, expected_shadowed_spot_count)) {
      return 15;
   }
   if (!hasPointShadowMetaInvariants(render_system, expected_shadowed_spot_count,
                                     expected_shadowed_point_count)) {
      return 16;
   }
   if (!hasDisabledFirstSpotExcludedFromPackedMeta(render_system, 3.0F, 6.0F)) { return 19; }

   // Verify frame and draw counters that belong to the current forward-renderer diagnostics.
   if (render_system.renderedFrameCount() != 1 || render_system.lastRenderedWindowCount() != 1) { return 5; }
   const auto &forward_renderer = std::get<vve::simple::ForwardRenderer>(render_system.backend());
   if (forward_renderer.presentedFrameCount() != 0 || forward_renderer.triangleDrawCount() != 0 ||
       forward_renderer.triangleVertexCount() != 0 || forward_renderer.sceneUploadCount() != 0 ||
       forward_renderer.sceneMeshDrawCount() != 0 || forward_renderer.sceneInstanceDrawCount() != 0 ||
       forward_renderer.sceneDrawVertexCount() != 0 || forward_renderer.sceneDrawIndexCount() != 0) {
      return 6;
   }
#ifndef NDEBUG
   if (!hasRecordedShadowsBeforeForwardColor(forward_renderer)) { return 17; }
#endif

   // Verify debug sample and comparison accessors expose the current no-readback state.
   if (forward_renderer.sceneDebugSampleCount() != 0 || forward_renderer.sceneCpuDebugSample(0) ||
       forward_renderer.sceneGpuDebugSample(0) || forward_renderer.sceneDebugClipError(0) ||
       forward_renderer.sceneDebugDepthError(0) || forward_renderer.sceneDebugLightSpaceError(0) ||
       forward_renderer.sceneDebugSpotLightSpaceError(0) || forward_renderer.sceneDebugPointLightSpaceError(0) ||
       forward_renderer.sceneDebugLightingError(0) || forward_renderer.sceneDebugShadowSampleError(0) ||
       forward_renderer.sceneDebugSpotShadowSampleError(0) || forward_renderer.sceneDebugPointShadowSampleError(0)) {
      return 7;
   }

#ifndef NDEBUG
   // Directional shadow-depth diagnostics are CPU-populated by engine.renderFrame(); GPU error stays unavailable here.
   const auto directional_shadow_sample = forward_renderer.sceneShadowDepthSample(0); ///< Single retained light-space sample.
   if (forward_renderer.sceneShadowDepthSampleCount() != 1U || !directional_shadow_sample ||
       !directional_shadow_sample->valid || directional_shadow_sample->face_index != 0U ||
       directional_shadow_sample->world.x != 0.0F || directional_shadow_sample->world.y != 0.0F ||
       directional_shadow_sample->world.z != 0.0F ||
       directional_shadow_sample->expected_depth != directional_shadow_sample->light_ndc.z ||
       directional_shadow_sample->bias != 0.001F || directional_shadow_sample->shadow_factor != 1.0F ||
       forward_renderer.sceneShadowDepthSample(1) || forward_renderer.sceneShadowDepthError(0)) {
      return 8;
   }

   // Verify point shadow-depth diagnostics retain one CPU-only sample per shadowed point light.
   if (!hasPointShadowDepthSamples(forward_renderer, render_system, expected_shadowed_point_count)) {
      return 8;
   }
   if (!hasFullContributionForNonOccludedShadowSamples(forward_renderer)) { return 18; }
#endif

   // Verify each active spot light receives a unique shadow-array slot when samples are reachable.
   const auto capped_spot_lights = std::views::take(forward_renderer.scene.spotLights,
                                                    vve::simple::kMaxShadowedSpotLights); ///< CPU scene cap.
   const std::size_t active_spot_light_count{static_cast<std::size_t>(
      std::ranges::count_if(capped_spot_lights, &vve::simple::SpotLight::enabled))}; ///< Enabled shader slots only.
   if (active_spot_light_count != std::min<std::size_t>(3U, vve::simple::kMaxShadowedSpotLights)) { return 11; }
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
