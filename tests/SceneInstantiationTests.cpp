/**
 * @file
 * @brief Facade integration test for turning an imported asset scene into visible render objects.
 *
 * Functional objects:
 * - main: writes a deterministic OBJ, loads it through AssetSystem, instantiates it through RenderSystem,
 *   and verifies that public render objects become visible.
 */

import std;

import VVEngine;
import VVE.TestSupport;

/// @brief Proves the public asset-to-render bridge creates visible objects for a loaded scene.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_instantiation_test.obj", vve::test::triangleObj);

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-instantiation-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   if (!scene || !scene->valid() || !assets.containsScene(*scene)) { return 1; }

   const auto node_count = assets.sceneNodeCount(*scene);
   const auto mesh_count = assets.sceneMeshCount(*scene);
   if (!node_count || !mesh_count || *node_count != 2U || *mesh_count != 1U) { return 2; }

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 3; }

   const auto objects = render.sceneInstanceObjects(*instance);
   if (!objects || objects->empty()) { return 4; }

   // Every object created by the happy-path bridge must be renderer-visible by default.
   for (const auto object : *objects) {
      const auto visible = render.objectVisible(object);
      if (!object.valid() || !visible || !*visible) { return 5; }
   }

   return 0;
}
