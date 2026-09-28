/**
 * @file
 * @brief Facade integration test for render-scene-instance removal invalidation.
 *
 * Functional objects:
 * - main: loads a deterministic OBJ, instantiates it through RenderSystem, removes the scene instance,
 *   and verifies that the public instance and render-object handles are no longer valid.
 */

import std;

import VEEngine;
import VVE.TestSupport;

/// @brief Proves removing a scene instance invalidates the render objects it created.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_removal_test.obj", vve::test::triangleObj);

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-removal-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 2; }

   const auto objects = render.sceneInstanceObjects(*instance);
   if (!objects || objects->empty()) { return 3; }
   const auto object_handles = *objects; // Copy before removal because the instance mapping is erased.

   const auto removed = render.removeSceneInstance(*instance);
   if (!removed) { return 4; }

   // The removed instance is no longer queryable through the public facade.
   const auto removed_objects = render.sceneInstanceObjects(*instance);
   if (removed_objects) { return 5; }

   // All render objects created by the removed instance must be invalidated as well.
   for (const auto object : object_handles) {
      const auto visible = render.objectVisible(object);
      if (visible) { return 6; }
   }

   return 0;
}
