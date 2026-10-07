/**
 * @file
 * @brief Facade integration test for reclaiming imported render resources after scene-instance removal.
 *
 * Functional objects:
 * - main: writes a deterministic OBJ, loads it through AssetSystem, instantiates it through RenderSystem,
 *   removes the instance, purges unused render assets, and verifies mesh/material counts return to baseline.
 */

import std;

import VVEngine;
import VVE.TestSupport;

/// @brief Proves unused imported render meshes and materials are reclaimed after their scene instance is removed.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_purge_unused_assets_test.obj", vve::test::triangleObj);

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-purge-unused-assets-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto baseline_meshes = render.sceneMeshCount();         // Existing render resources must survive the test.
   const auto baseline_materials = render.sceneMaterialCount();  // Imported scene resources are compared against this.

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 2; }

   // Instantiating the imported scene must create render-side mesh and material resources.
   if (render.sceneMeshCount() <= baseline_meshes || render.sceneMaterialCount() <= baseline_materials) { return 3; }

   const auto removed = render.removeSceneInstance(*instance);
   if (!removed) { return 4; }

   const auto purged = render.purgeUnusedAssets(); // Removed scene instances leave resources eligible for reclamation.
   (void)purged;                                  // The observable contract here is the post-purge resource count.
   if (render.sceneMeshCount() != baseline_meshes || render.sceneMaterialCount() != baseline_materials) { return 5; }

   // Three independent imports leave a middle hole while the first and last remain referenced.
   auto scenes = std::array<vve::SceneHandle, 3>{};
   auto instances = std::array<vve::RenderSceneInstanceHandle, 3>{};
   auto objects = std::array<vve::RenderObjectHandle, 3>{};
   for (const auto index : std::views::iota(0U, scenes.size())) {
      const auto loaded = assets.loadScene(path);
      if (!loaded) { return 6; }
      scenes[index] = *loaded;
      const auto added = render.instantiateScene(*loaded);
      if (!added) { return 6; }
      instances[index] = *added;
      const auto drawn = render.sceneInstanceObjects(*added);
      if (!drawn || drawn->size() != 1U) { return 6; }
      objects[index] = drawn->front();
   }
   if (!render.removeSceneInstance(instances[1]) || render.purgeUnusedAssets() != 2U ||
       render.sceneMeshCount() != 2U || render.sceneMaterialCount() != 2U ||
       render.objectTransform(objects[1]) || !render.objectTransform(objects[0]) ||
       !render.setObjectVisible(objects[2], false) || render.objectVisible(objects[2]).value_or(true)) { return 7; }
   // Reusing the shifted last resources must find both its mesh and material without creating copies.
   const auto repeated = render.instantiateScene(scenes[2]);
   if (!repeated || render.sceneMeshCount() != 2U || render.sceneMaterialCount() != 2U ||
       !render.setObjectTransform(objects[0], vve::Transform{.translation = vve::Position{{2, 0, 0}}})) { return 8; }
   const auto recreated = render.instantiateScene(scenes[1]);
   std::println("D14 purge recreated={} meshes={} materials={}", recreated.has_value(),
      render.sceneMeshCount(), render.sceneMaterialCount());
   if (!recreated || render.sceneMeshCount() != 3U || render.sceneMaterialCount() != 3U ||
       render.purgeUnusedAssets() != 0U) { return 9; }
   for (const auto live : {instances[0], instances[2], *repeated, *recreated}) {
      if (!render.removeSceneInstance(live)) { return 10; }
   }
   if (render.purgeUnusedAssets() != 6U || render.purgeUnusedAssets() != 0U ||
       render.sceneMeshCount() != 0U || render.sceneMaterialCount() != 0U) { return 11; }
   std::println("D14 purge middle_removed=2 retained=4 final_removed=6");
   return 0;
}
