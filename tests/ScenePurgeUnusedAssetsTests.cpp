/**
 * @file
 * @brief Facade integration test for reclaiming imported render resources after scene-instance removal.
 *
 * Functional objects:
 * - main: writes a deterministic OBJ, loads it through AssetSystem, instantiates it through RenderSystem,
 *   removes the instance, purges unused render assets, and verifies mesh/material counts return to baseline.
 */

#include <filesystem>
#include <fstream>

import VEEngine;

namespace {

/// @brief Writes a tiny single-triangle OBJ that Assimp imports deterministically.
[[nodiscard]] std::filesystem::path writeTemporaryObj() {
   const auto path = std::filesystem::temp_directory_path() / "vve_scene_purge_unused_assets_test.obj";
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

/// @brief Proves unused imported render meshes and materials are reclaimed after their scene instance is removed.
int main() {
   const auto path = writeTemporaryObj();

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-purge-unused-assets-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   removeTemporaryObj(path);
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

   return 0;
}
