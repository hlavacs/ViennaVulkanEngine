import std;

import VEEngine;
import VVE.TestSupport;

/// @brief Checks imported light descriptors through the public asset facade only.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_asset_light_data_test.gltf", R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "point", "color": [0.25, 0.5, 0.75], "intensity": 3.0, "range": 8.0},
    {"type": "spot", "spot": {"innerConeAngle": 0.2, "outerConeAngle": 0.6}}
  ]}},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [
    {"name": "Rig", "translation": [10, 0, 0], "rotation": [0, 0.7071068, 0, 0.7071068], "children": [1, 2]},
    {"name": "PointLightNode", "translation": [1, 2, 3], "extensions": {"KHR_lights_punctual": {"light": 0}}},
    {"name": "SpotLightNode", "translation": [1, 2, 3], "extensions": {"KHR_lights_punctual": {"light": 1}}}
  ]
})");
   auto engine = vve::EngineBuilder<>{}.applicationName("asset-light-data").build();
   auto assets = engine.world().get<vve::AssetSystem>();
   const auto scene = assets.loadScene(path);
   if (!scene) { return 1; }

   const auto lights = assets.sceneLights(*scene);
   if (!lights || lights->size() != 2 || !lights->front().valid()) { return 2; }

   const auto data = assets.lightData(lights->front());
   if (!data) { return 3; }
   if (data->kind != vve::LightKind::point) { return 4; }
   if (data->color.value.x <= 0.0F || data->color.value.y <= 0.0F || data->color.value.z <= 0.0F) { return 5; }
   if (data->intensity.value <= 0.0F || !std::isfinite(data->intensity.value)) { return 6; }

   // The parent rotates (1,2,3) to (3,2,-1) and adds its translation.
   constexpr float tolerance = 1.0e-4F;
   const auto position = data->position.value;
   if (!(std::abs(position.x - 13.0F) <= tolerance && std::abs(position.y - 2.0F) <= tolerance &&
         std::abs(position.z + 1.0F) <= tolerance)) { return 8; }
   // glTF's explicit range is imported from Assimp's PBR_LightRange metadata.
   if (!(std::abs(data->range.value - 8.0F) <= tolerance)) { return 9; }
   // Assimp folds the authored intensity into the colour; compare their product.
   const auto radiance = vve::math::scale(data->color.value, data->intensity.value);
   if (!(std::abs(radiance.x - 0.75F) <= tolerance && std::abs(radiance.y - 1.5F) <= tolerance &&
         std::abs(radiance.z - 2.25F) <= tolerance)) { return 10; }

   // The spot's local -Z axis inherits the rig's rotation to world -X.
   if (!lights->back().valid()) { return 11; }
   const auto spot = assets.lightData(lights->back());
   if (!spot) { return 12; }
   if (spot->kind != vve::LightKind::spot) { return 13; }
   const auto direction = spot->direction.value;
   if (!(std::abs(direction.x + 1.0F) <= tolerance && std::abs(direction.y) <= tolerance &&
         std::abs(direction.z) <= tolerance)) { return 14; }
   // Preserve the glTF cone half-angles and use the default range when none is authored.
   if (!(std::abs(spot->cone.radians - 0.6F) <= tolerance)) { return 15; }
   if (!(std::abs(spot->inner_cone.radians - 0.2F) <= tolerance)) { return 16; }
   if (!(std::abs(spot->range.value - vve::LightRange{}.value) <= tolerance)) { return 17; }

   const auto missing = assets.lightData(vve::LightHandle{});
   if (missing || missing.error() != vve::Error::missing_object) { return 7; }

   // A single glTF spot pins the axis-to-edge convention independently of world transforms.
   const auto cone_path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_spot_half_angles.gltf", R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "spot", "spot": {"innerConeAngle": 0.2, "outerConeAngle": 0.6}}
  ]}},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [{"name": "Spot", "extensions": {"KHR_lights_punctual": {"light": 0}}}]
})");
   const auto cone_scene = assets.loadScene(cone_path);
   if (!cone_scene) { return 18; }
   const auto cone_lights = assets.sceneLights(*cone_scene);
   if (!cone_lights || cone_lights->size() != 1) { return 19; }
   const auto cone_data = assets.lightData(cone_lights->front());
   if (!cone_data || cone_data->kind != vve::LightKind::spot) { return 20; }
   // Report the imported radians so a failed convention check is inspectable.
   constexpr float cone_tolerance = 1.0e-5F;
   std::cout << "[spot-half-angles] outer=" << cone_data->cone.radians
             << " inner=" << cone_data->inner_cone.radians << " expected_outer=0.6 expected_inner=0.2\n";
   if (!(std::abs(cone_data->cone.radians - 0.6F) <= cone_tolerance)) { return 21; }
   if (!(std::abs(cone_data->inner_cone.radians - 0.2F) <= cone_tolerance)) { return 22; }

   return 0;
}
