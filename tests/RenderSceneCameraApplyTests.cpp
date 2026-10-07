/**
 * @file
 * @brief Facade test for opt-in imported camera application during scene instantiation.
 *
 * Functional objects:
 * - main: loads a glTF perspective camera, instantiates it with cameras disabled and enabled, and checks render counts.
 */

import std;

import VVEngine;
import VVE.TestSupport;

/// @brief Verifies imported cameras affect render counts only when requested.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_render_scene_camera_apply_test.gltf", R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [{"name": "CameraNode", "camera": 0, "translation": [1.0, 2.0, 3.0]}],
  "cameras": [{"name": "MainCamera", "type": "perspective",
               "perspective": {"yfov": 0.7, "aspectRatio": 1.5, "znear": 0.1, "zfar": 50.0}}]
})");

   auto engine = vve::EngineBuilder<>{}.applicationName("render-scene-camera-apply-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
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
