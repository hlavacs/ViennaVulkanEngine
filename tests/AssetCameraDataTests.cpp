#include <cmath>
#include <filesystem>
#include <fstream>

import VEEngine;

/// @brief Writes a minimal glTF scene with one perspective camera for facade import tests.
[[nodiscard]] std::filesystem::path writeFixture() {
   const auto path = std::filesystem::temp_directory_path() / "vve_asset_camera_data_test.gltf";
   std::ofstream file{path};
   file << R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [{"name": "CameraNode", "camera": 0, "translation": [1.0, 2.0, 3.0]}],
  "cameras": [{"name": "MainCamera", "type": "perspective",
               "perspective": {"yfov": 0.7, "aspectRatio": 1.5, "znear": 0.1, "zfar": 50.0}}]
})";
   return path;
}

/// @brief Tests imported camera descriptors through the public asset facade only.
int main() {
   const auto path = writeFixture();
   auto engine = vve::EngineBuilder<>{}.applicationName("asset-camera-data").build();
   auto assets = engine.world().get<vve::AssetSystem>();
   const auto scene = assets.loadScene(path);
   std::error_code error{};
   std::filesystem::remove(path, error);
   if (!scene) { return 1; }

   const auto cameras = assets.sceneCameras(*scene);
   if (!cameras || cameras->size() != 1 || !cameras->front().valid()) { return 2; }

   const auto data = assets.cameraData(cameras->front());
   if (!data) { return 3; }
   if (!std::isfinite(data->position.value.x) || !std::isfinite(data->position.value.y) ||
       !std::isfinite(data->position.value.z)) { return 4; }
   if (!std::isfinite(data->direction.value.x) || !std::isfinite(data->up.value.y)) { return 5; }
   if (data->fov.radians <= 0.0F || !std::isfinite(data->fov.radians)) { return 6; }
   if (data->aspect <= 0.0F || !std::isfinite(data->aspect)) { return 7; }
   if (data->near_clip <= 0.0F || data->far_clip <= data->near_clip) { return 8; }

   const auto missing = assets.cameraData(vve::CameraHandle{});
   if (missing || missing.error() != vve::Error::missing_object) { return 9; }

   return 0;
}
