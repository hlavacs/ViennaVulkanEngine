/**
 * @file
 * @brief Facade integration test for blocking asset-scene removal while render instances depend on it.
 *
 * Functional objects:
 * - main: loads a deterministic OBJ, instantiates it through RenderSystem, and verifies that the
 *   source asset scene cannot be removed while the render scene instance is live.
 */

#include <filesystem>
#include <fstream>

import VEEngine;

namespace {

/// @brief Writes a tiny single-triangle OBJ that Assimp imports deterministically.
[[nodiscard]] std::filesystem::path writeTemporaryObj() {
   const auto path = std::filesystem::temp_directory_path() / "vve_scene_asset_removal_blocked_test.obj";
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

/// @brief Proves a live render scene instance blocks removal of its source asset scene.
int main() {
   const auto path = writeTemporaryObj();

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-asset-removal-blocked-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   removeTemporaryObj(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 2; }

   // Asset scene removal must report failure while render objects still depend on the imported data.
   const auto removed = render.removeScene(*scene);
   if (removed) { return 3; }

   return 0;
}
