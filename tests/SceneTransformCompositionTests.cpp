/**
 * @file
 * @brief Facade integration test for hierarchical asset-node transform composition during scene instantiation.
 *
 * Functional objects:
 * - writeTemporaryGltf: writes a deterministic glTF scene with one translated parent and one translated mesh node.
 * - expectedWorldTransform: recomposes an imported node transform from the public AssetSystem hierarchy.
 * - main: loads the fixture, instantiates it through RenderSystem, and compares each render object transform.
 */

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

import VEEngine;

namespace {

/// @brief Compares two facade scalars with a small tolerance for import and matrix decomposition noise.
[[nodiscard]] bool close(vve::Scalar lhs, vve::Scalar rhs) {
   constexpr auto epsilon = static_cast<vve::Scalar>(1.0e-4);
   return std::abs(lhs - rhs) <= epsilon;
}

/// @brief Compares two facade vectors component-wise.
[[nodiscard]] bool close(vve::Vec3 lhs, vve::Vec3 rhs) {
   return close(lhs.x, rhs.x) && close(lhs.y, rhs.y) && close(lhs.z, rhs.z);
}

/// @brief Compares two facade quaternions component-wise.
[[nodiscard]] bool close(vve::Quat lhs, vve::Quat rhs) {
   return close(lhs.w, rhs.w) && close(lhs.x, rhs.x) && close(lhs.y, rhs.y) && close(lhs.z, rhs.z);
}

/// @brief Compares full public transforms component-wise.
[[nodiscard]] bool close(vve::Transform lhs, vve::Transform rhs) {
   return close(lhs.translation.value, rhs.translation.value) && close(lhs.rotation.value, rhs.rotation.value) &&
          close(lhs.scale.value, rhs.scale.value);
}

/// @brief Writes a tiny glTF 2.0 triangle scene with non-identity parent and child translations.
[[nodiscard]] std::filesystem::path writeTemporaryGltf() {
   const auto path = std::filesystem::temp_directory_path() / "vve_scene_transform_composition_test.gltf";
   std::ofstream file{path};
   file << R"({
  "asset": {"version": "2.0", "generator": "vve scene transform composition test"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "ParentOffset", "translation": [2.0, 3.0, 4.0], "children": [1]},
    {"name": "TriangleNode", "translation": [0.5, 1.25, -2.0], "mesh": 0}
  ],
  "meshes": [
    {"name": "TriangleMesh", "primitives": [{"attributes": {"POSITION": 0}, "mode": 4}]}
  ],
  "buffers": [
    {"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA", "byteLength": 36}
  ],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962}],
  "accessors": [
    {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0]}
  ]
})";
   return path;
}

/// @brief Removes the temporary source file without making cleanup affect assertion results.
void removeTemporaryGltf(const std::filesystem::path &path) {
   std::error_code error{}; // Cleanup is best-effort because imported scene data is already in memory.
   std::filesystem::remove(path, error);
}

/// @brief Recomputes a node world transform by walking public AssetSystem parents from leaf to root.
[[nodiscard]] int expectedWorldTransform(vve::AssetSystem &assets,
                                         vve::SceneHandle scene,
                                         vve::NodeHandle node,
                                         vve::Transform &result) {
   const auto root = assets.sceneRootNode(scene);
   if (!root || !root->valid()) { return 10; }

   auto chain = std::vector<vve::Transform>{}; // Local transforms are collected leaf-to-root, then reversed.
   for (auto current = node;;) {
      const auto local = assets.nodeTransform(current);
      if (!local) { return 11; }
      chain.push_back(*local);
      if (current == *root) { break; }

      const auto parent = assets.sceneNodeParent(scene, current);
      if (!parent || !*parent) { return 12; }
      current = **parent;
   }

   auto world = vve::Transform{};
   std::ranges::reverse(chain);
   for (const auto &local : chain) {
      world.translation.value += local.translation.value; // The fixture uses translation-only nodes.
   }
   result = world;
   return 0;
}

/// @brief Checks that a transform's translation proves parent and child composition happened.
[[nodiscard]] bool hasNonZeroTranslation(vve::Transform transform) {
   return !close(transform.translation.value, vve::Vec3{vve::zero(), vve::zero(), vve::zero()});
}

} // namespace

/// @brief Proves RenderSystem::instantiateScene composes asset-node parent transforms into object transforms.
int main() {
   const auto path = writeTemporaryGltf();

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-transform-composition-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   removeTemporaryGltf(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 2; }

   const auto objects = render.sceneInstanceObjects(*instance);
   if (!objects || objects->empty()) { return 3; }

   // Every object must match an independently recomposed world transform from the public asset hierarchy.
   for (const auto object : *objects) {
      const auto source_node = render.objectSourceNode(object);
      if (!source_node || !source_node->valid()) { return 4; }

      auto expected = vve::Transform{};
      if (const auto error = expectedWorldTransform(assets, *scene, *source_node, expected); error != 0) { return error; }
      if (!hasNonZeroTranslation(expected)) { return 5; }

      const auto actual = render.objectTransform(object);
      if (!actual || !close(*actual, expected)) { return 6; }
   }

   return 0;
}
