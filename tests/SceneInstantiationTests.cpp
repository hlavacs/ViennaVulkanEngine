/**
 * @file
 * @brief Facade integration test for turning an imported asset scene into visible render objects.
 *
 * Functional objects:
 * - main: writes a deterministic OBJ, loads it through AssetSystem, instantiates it through RenderSystem,
 *   and verifies that public render objects become visible.
 */

#include <filesystem>
#include <fstream>

import VEEngine;

namespace {

/// @brief Writes a tiny single-triangle OBJ that Assimp imports deterministically.
[[nodiscard]] std::filesystem::path writeTemporaryObj() {
   const auto path = std::filesystem::temp_directory_path() / "vve_scene_instantiation_test.obj";
   std::ofstream file{path};
   file << "o Triangle\n"
        << "v 0 0 0\n"
        << "v 1 0 0\n"
        << "v 0 1 0\n"
        << "f 1 2 3\n";
   return path;
}

/// @brief Removes the temporary source file without making cleanup affect assertion results.
void removeTemporaryObj(const std::filesystem::path &path) {
   std::error_code error{}; // Cleanup is best-effort because test state is already captured in memory.
   std::filesystem::remove(path, error);
}

} // namespace

/// @brief Proves the public asset-to-render bridge creates visible objects for a loaded scene.
int main() {
   const auto path = writeTemporaryObj();

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-instantiation-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   removeTemporaryObj(path);
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
