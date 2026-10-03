import std;

import VVEngine;
import VVE.TestSupport;

/// @brief Tests imported camera descriptors through the public asset facade only.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_asset_camera_data_test.gltf", R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [
    {"name": "Rig", "translation": [10, 0, 0], "rotation": [0, 0.7071068, 0, 0.7071068], "children": [1]},
    {"name": "CameraNode", "camera": 0, "translation": [1, 2, 3]}
  ],
  "cameras": [{"name": "MainCamera", "type": "perspective",
               "perspective": {"yfov": 0.7, "aspectRatio": 1.5, "znear": 0.1, "zfar": 50.0}}]
})");
   auto engine = vve::EngineBuilder<>{}.applicationName("asset-camera-data").build();
   auto assets = engine.world().get<vve::AssetSystem>();
   const auto scene = assets.loadScene(path);
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

   // The parent rotates (1,2,3) to (3,2,-1) and adds its translation.
   constexpr float tolerance = 1.0e-4F;
   const auto position = data->position.value;
   if (!(std::abs(position.x - 13.0F) <= tolerance && std::abs(position.y - 2.0F) <= tolerance &&
         std::abs(position.z + 1.0F) <= tolerance)) { return 10; }
   // The rig turns the camera from local -Z to world -X while preserving +Y up.
   const auto direction = data->direction.value;
   if (!(std::abs(direction.x + 1.0F) <= tolerance && std::abs(direction.y) <= tolerance &&
         std::abs(direction.z) <= tolerance)) { return 11; }
   const auto up = data->up.value;
   if (!(std::abs(up.x) <= tolerance && std::abs(up.y - 1.0F) <= tolerance &&
         std::abs(up.z) <= tolerance)) { return 12; }
   // Compare every projection field with the authored perspective camera, not its defaults.
   if (!(std::abs(data->fov.radians - 0.7F) <= 1.0e-3F)) { return 13; }
   if (!(std::abs(data->aspect - 1.5F) <= tolerance)) { return 14; }
   if (!(std::abs(data->near_clip - 0.1F) <= tolerance)) { return 15; }
   if (!(std::abs(data->far_clip - 50.0F) <= tolerance)) { return 16; }

   const auto missing = assets.cameraData(vve::CameraHandle{});
   if (missing || missing.error() != vve::Error::missing_object) { return 9; }

   return 0;
}
