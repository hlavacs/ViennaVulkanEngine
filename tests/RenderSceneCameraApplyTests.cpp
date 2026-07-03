/**
 * @file
 * @brief Facade test for opt-in imported camera application during scene instantiation.
 *
 * Functional objects:
 * - main: loads a glTF perspective camera, instantiates it with cameras disabled and enabled, and checks render counts.
 */

#include <filesystem>
#include <fstream>

import VEEngine;

namespace {

/// @brief Writes a minimal glTF scene with one perspective camera.
[[nodiscard]] std::filesystem::path writeFixture() {
   const auto path = std::filesystem::temp_directory_path() / "vve_render_scene_camera_apply_test.gltf";
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

/// @brief Removes the temporary source without affecting deterministic assertions.
void removeFixture(const std::filesystem::path &path) {
   std::error_code error{}; // Cleanup is not part of the test result.
   std::filesystem::remove(path, error);
}

} // namespace

/// @brief Verifies imported cameras affect render counts only when requested.
int main() {
   const auto path = writeFixture();

   auto engine = vve::EngineBuilder<>{}.applicationName("render-scene-camera-apply-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   removeFixture(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto cameras = assets.sceneCameras(*scene);
   if (!cameras || cameras->empty()) { return 2; }

   const auto disabled = render.instantiateScene(*scene);
   if (!disabled) { return 3; }
   if (render.sceneCameraCount() != 0U) { return 4; }

   const auto enabled = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.apply_cameras = true});
   if (!enabled) { return 5; }
   if (render.sceneCameraCount() != cameras->size()) { return 6; }

   return 0;
}
