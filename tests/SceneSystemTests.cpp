#include <filesystem>
#include <fstream>

import VEEngine;

int main() {
   const auto path = std::filesystem::temp_directory_path() / "vve_scene_system_test.obj";
   {
      std::ofstream file{path};
      file << "o Triangle\n"
           << "v 0 0 0\n"
           << "v 1 0 0\n"
           << "v 0 1 0\n"
           << "f 1 2 3\n";
   }

   auto engine = vve::makeEngine(vve::ApplicationName{"scene-tests"});
   auto assets = engine.world().assets();
   const auto scene = assets.loadScene(path);
   std::error_code error{};
   std::filesystem::remove(path, error);
   if (!scene || !assets.containsScene(*scene)) { return 1; }

   const auto scene_name = assets.sceneName(*scene);
   const auto nodes = assets.sceneNodes(*scene);
   const auto meshes = assets.sceneMeshes(*scene);
   const auto materials = assets.sceneMaterials(*scene);
   if (!scene_name || !nodes || !meshes || !materials) { return 2; }
   if (scene_name->value != path.filename().string() || nodes->empty() || meshes->empty() || materials->empty()) {
      return 3;
   }

   const auto root = assets.sceneRootNode(*scene);
   if (!root || !root->valid()) { return 4; }
   const auto root_parent = assets.sceneNodeParent(*scene, *root);
   if (!root_parent || root_parent->has_value()) { return 5; }

   const auto mesh = meshes->front();
   const auto vertex_count = assets.meshVertexCount(mesh);
   const auto index_count = assets.meshIndexCount(mesh);
   const auto bounds = assets.meshBounds(mesh);
   if (!vertex_count || !index_count || !bounds) { return 6; }
   if (vertex_count->value != 3 || index_count->value != 3 || !bounds->valid) { return 7; }

   return 0;
}
