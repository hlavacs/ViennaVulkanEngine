/**
 * @file
 * @brief Facade integration test for render-scene-instance removal invalidation.
 *
 * Functional objects:
 * - main: loads a deterministic OBJ, instantiates it through RenderSystem, removes the scene instance,
 *   and verifies that the public instance and render-object handles are no longer valid.
 */

#include <filesystem>
#include <fstream>

import VEEngine;

namespace {

/// @brief Writes a tiny single-triangle OBJ that Assimp imports deterministically.
[[nodiscard]] std::filesystem::path writeTemporaryObj() {
   const auto path = std::filesystem::temp_directory_path() / "vve_scene_removal_test.obj";
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
   std::error_code error{}; // Cleanup is best-effort because imported scene data is already in memory.
   std::filesystem::remove(path, error);
}

} // namespace

/// @brief Proves removing a scene instance invalidates the render objects it created.
int main() {
   const auto path = writeTemporaryObj();

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-removal-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   removeTemporaryObj(path);
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
