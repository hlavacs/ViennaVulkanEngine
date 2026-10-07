/**
 * @file
 * @brief Facade integration test for blocking asset-scene removal while render instances depend on it.
 *
 * Functional objects:
 * - main: loads a deterministic OBJ, instantiates it through RenderSystem, and verifies that the
 *   source asset scene cannot be removed while the render scene instance is live, but can afterwards.
 */

import std;

import VVEngine;
import VVE.TestSupport;

/// @brief Proves a live render scene instance blocks removal of its source asset scene.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_asset_removal_blocked_test.obj", vve::test::triangleObj);

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-asset-removal-blocked-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 2; }

   // Asset scene removal must report failure while render objects still depend on the imported data.
   const auto removed = render.removeScene(*scene);
   if (removed) { return 3; }

   // Once the instance is gone, the scene's render data can be released through the facade.
   if (!render.removeSceneInstance(*instance)) { return 4; }
   if (!render.removeScene(*scene)) { return 5; }
   return 0;
}
