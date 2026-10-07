/**
 * @file
 * @brief Facade integration test for hierarchical asset-node transform composition during scene instantiation.
 *
 * Functional objects:
 * - main: loads the fixture, instantiates it through RenderSystem, and compares each render object transform.
 */

import std;

import VVEngine;
import VVE.TestSupport;

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

/// @brief Compares rotations by absolute dot product so opposite quaternion signs count as equal.
[[nodiscard]] bool close(vve::Quat lhs, vve::Quat rhs) {
   constexpr auto epsilon = static_cast<vve::Scalar>(1.0e-4);
   const auto dot = lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
   return std::abs(dot) > vve::one() - epsilon;
}

/// @brief Compares translation and scale component-wise and rotation independently of quaternion sign.
[[nodiscard]] bool close(vve::Transform lhs, vve::Transform rhs) {
   return close(lhs.translation.value, rhs.translation.value) && close(lhs.rotation.value, rhs.rotation.value) &&
          close(lhs.scale.value, rhs.scale.value);
}

/// @brief Checks that a transform's translation proves parent and child composition happened.
[[nodiscard]] bool hasNonZeroTranslation(vve::Transform transform) {
   return !close(transform.translation.value, vve::Vec3{vve::zero(), vve::zero(), vve::zero()});
}

} // namespace

/// @brief Proves RenderSystem::instantiateScene composes asset-node parent transforms into object transforms.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_transform_composition_test.gltf", R"({
  "asset": {"version": "2.0", "generator": "vve scene transform composition test"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "Rig", "translation": [2.0, 3.0, 4.0], "rotation": [0.0, 0.7071068, 0.0, 0.7071068],
     "scale": [2.0, 1.0, 1.0], "children": [1, 2]},
    {"name": "A", "translation": [1.0, 0.0, 0.0], "mesh": 0},
    {"name": "B", "translation": [0.0, 1.0, 0.0], "rotation": [0.0, 0.7071068, 0.0, 0.7071068], "mesh": 0}
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
})");

   auto engine = vve::EngineBuilder<>{}.applicationName("scene-transform-composition-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto instance = render.instantiateScene(*scene);
   if (!instance || !instance->valid()) { return 2; }

   const auto objects = render.sceneInstanceObjects(*instance);
   if (!objects || objects->size() != 2) { return 3; }

   const auto root = assets.sceneRootNode(*scene);
   if (!root || !root->valid()) { return 10; }
   if (!assets.nodeTransform(*root)) { return 11; }
   bool checked_a = false; // Require both authored mesh nodes, regardless of traversal order.
   bool checked_b = false;

   // Only mesh nodes become objects; the mesh-free Rig contributes its transform through the hierarchy.
   for (const auto object : *objects) {
      const auto source_node = render.objectSourceNode(object);
      if (!source_node || !source_node->valid()) { return 4; }
      if (!assets.nodeTransform(*source_node)) { return 11; }
      const auto parent = assets.sceneNodeParent(*scene, *source_node);
      if (!parent || !*parent || **parent != *root) { return 12; }
      const auto name = assets.nodeName(*source_node);
      if (!name) { return 7; }

      // Fixed world-space values are independent of the engine's composition and decomposition code.
      auto expected = vve::Transform{};
      if (name->value == "A") {
         expected.translation.value = vve::Vec3{2.0F, 3.0F, 2.0F};
         expected.rotation.value = vve::Quat{0.7071068F, 0.0F, 0.7071068F, 0.0F}; // Constructor order is w, x, y, z.
         expected.scale.value = vve::Vec3{2.0F, 1.0F, 1.0F};
         checked_a = true;
      } else if (name->value == "B") {
         expected.translation.value = vve::Vec3{2.0F, 4.0F, 4.0F};
         expected.rotation.value = vve::Quat{0.0F, 0.0F, 1.0F, 0.0F}; // Two quarter turns about +Y.
         expected.scale.value = vve::Vec3{1.0F, 1.0F, 2.0F}; // The child's rotation exchanges the scaled axes.
         checked_b = true;
      } else { return 7; }
      if (!hasNonZeroTranslation(expected)) { return 5; }

      const auto actual = render.objectTransform(object);
      if (!actual || !close(*actual, expected)) {
         std::cerr << "[SceneTransformCompositionTests] node=" << name->value << " world_transform_mismatch\n";
         return 6;
      }
   }
   if (!checked_a || !checked_b) { return 8; }

   return 0;
}
