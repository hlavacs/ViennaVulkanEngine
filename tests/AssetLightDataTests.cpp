#include <cmath>
#include <filesystem>
#include <fstream>

import VEEngine;

/// @brief Writes a minimal glTF scene with one punctual light for facade import tests.
[[nodiscard]] std::filesystem::path writeFixture() {
   const auto path = std::filesystem::temp_directory_path() / "vve_asset_light_data_test.gltf";
   std::ofstream file{path};
   file << R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "point", "color": [0.25, 0.5, 0.75], "intensity": 3.0, "range": 8.0}
  ]}},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [{"name": "PointLightNode", "translation": [1.0, 2.0, 3.0],
             "extensions": {"KHR_lights_punctual": {"light": 0}}}]
})";
   return path;
}

/// @brief Checks imported light descriptors through the public asset facade only.
int main() {
   const auto path = writeFixture();
   auto engine = vve::EngineBuilder<>{}.applicationName("asset-light-data").build();
   auto assets = engine.world().get<vve::AssetSystem>();
   const auto scene = assets.loadScene(path);
   std::error_code error{};
   std::filesystem::remove(path, error);
   if (!scene) { return 1; }

   const auto lights = assets.sceneLights(*scene);
   if (!lights || lights->size() != 1 || !lights->front().valid()) { return 2; }

   const auto data = assets.lightData(lights->front());
   if (!data) { return 3; }
   if (data->kind != vve::LightKind::point) { return 4; }
   if (data->color.value.x <= 0.0F || data->color.value.y <= 0.0F || data->color.value.z <= 0.0F) { return 5; }
   if (data->intensity.value <= 0.0F || !std::isfinite(data->intensity.value)) { return 6; }

   const auto missing = assets.lightData(vve::LightHandle{});
   if (missing || missing.error() != vve::Error::missing_object) { return 7; }

   return 0;
}
