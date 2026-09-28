import std;

import VEEngine;
import VVE.TestSupport;

int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_system_test.obj", vve::test::triangleObj);

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-tests").build();
   auto assets = engine.world().get<vve::AssetSystem>();
   const auto scene = assets.loadScene(path);
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
   const auto positions = assets.meshPositions(mesh);
   const auto normals = assets.meshNormals(mesh);
   const auto texcoords = assets.meshTexcoords(mesh);
   const auto indices = assets.meshIndices(mesh);
   if (!vertex_count || !index_count || !bounds || !positions || !normals || !texcoords || !indices) { return 6; }
   if (vertex_count->value != 3 || index_count->value != 3 || !bounds->valid) { return 7; }
   if (positions->size() != 3 || normals->size() != 3 || texcoords->size() != 3 || indices->size() != 3) { return 8; }
   if (indices->at(0) != 0 || indices->at(1) != 1 || indices->at(2) != 2) { return 9; }

   // Derived node lists retain the import traversal's root-first order.
   if (nodes->front() != *root) { return 10; }
   std::size_t material_links{};
   // Materials remain aligned with node meshes, including repeated material handles.
   for (const auto node : *nodes) {
      const auto node_meshes = assets.nodeMeshes(node);
      const auto node_materials = assets.nodeMaterials(node);
      if (!node_meshes || !node_materials || node_meshes->size() != node_materials->size()) { return 11; }
      auto material = node_materials->begin();
      for (const auto mesh_handle : *node_meshes) {
         const auto mesh_material = assets.meshMaterial(mesh_handle);
         if (!mesh_material || *material++ != *mesh_material) { return 12; }
         ++material_links;
      }
   }
   if (material_links == 0U) { return 13; }

   // Derived queries retain missing-handle errors and empty-scene behavior.
   const auto missing_nodes = assets.sceneNodes(vve::SceneHandle{});
   const auto missing_materials = assets.nodeMaterials(vve::NodeHandle{});
   const auto missing_vertices = assets.meshVertexCount(vve::MeshHandle{});
   const auto missing_indices = assets.meshIndexCount(vve::MeshHandle{});
   if (missing_nodes || missing_nodes.error() != vve::Error::missing_object ||
       missing_materials || missing_materials.error() != vve::Error::missing_object ||
       missing_vertices || missing_vertices.error() != vve::Error::missing_object ||
       missing_indices || missing_indices.error() != vve::Error::missing_object) { return 14; }
   const auto empty = assets.addScene(vve::ObjectName{.value = "empty"});
   if (!empty) { return 15; }
   const auto empty_nodes = assets.sceneNodes(*empty);
   const auto foreign_children = assets.sceneNodeChildren(*empty, *root);
   const auto foreign_parent = assets.sceneNodeParent(*empty, *root);
   if (!empty_nodes || !empty_nodes->empty() || foreign_children || foreign_parent ||
       foreign_children.error() != vve::Error::missing_object || foreign_parent.error() != vve::Error::missing_object) { return 16; }
   std::println("D7 scene_nodes={} root_first=1 material_links={} derived_query_errors=1", nodes->size(), material_links);

   return 0;
}
