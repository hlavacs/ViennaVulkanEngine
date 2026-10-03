/**
 * @file
 * @brief Facade regression test for render handles shared across independent engines.
 *
 * Functional objects:
 * - main: creates objects and imported scene instances without initializing either engine,
 *   rejects foreign handles, and verifies that each engine's own resources survive.
 */

import std;
import VVEngine;

/// @brief Checks process-wide handle uniqueness and ownership without creating windows or a GPU device.
int main() {
   auto engine_a = vve::Engine<>{};
   auto engine_b = vve::Engine<>{};
   auto &a = engine_a.world().get<vve::RenderSystem>();
   auto &b = engine_b.world().get<vve::RenderSystem>();
   bool passed{true}; // Keep every ownership result visible even when handles collide.
   const auto check = [&passed](bool condition, std::string_view name) {
      std::println("[RenderHandleUniquenessTests] {}={}", name, condition);
      passed = passed && condition;
   };

   // Identical creation sequences in different engines must still mint different handles.
   const auto object_a = a.addPlane(vve::PlaneDescriptor{});
   const auto object_b = b.addPlane(vve::PlaneDescriptor{});
   if (!object_a || !object_b || !object_a->valid() || !object_b->valid()) { return 1; }
   check(*object_a != *object_b, "object_handles_differ");

   // Foreign queries and removals use the existing missing-object error contract.
   const auto foreign_transform = b.objectTransform(*object_a);
   const auto foreign_object_removal = b.removeObject(*object_a);
   check(!foreign_transform && foreign_transform.error() == vve::Error::missing_object,
         "foreign_transform_rejected");
   check(!foreign_object_removal && foreign_object_removal.error() == vve::Error::missing_object,
         "foreign_object_removal_rejected");
   check(a.objectTransform(*object_a).has_value(), "own_object_a_survives");
   check(b.objectTransform(*object_b).has_value(), "own_object_b_survives");

   // Keep the deterministic OBJ fixture isolated to this test and build tree.
   std::filesystem::create_directories(VVE_TEST_TMP_DIR);
   const auto path = std::filesystem::path{VVE_TEST_TMP_DIR} / "render_handle_uniqueness.obj";
   {
      std::ofstream file{path};
      file << "o Triangle\n"
           << "v 0 0 0\n"
           << "v 1 0 0\n"
           << "v 0 1 0\n"
           << "f 1 2 3\n";
      file.close();
      if (!file) { return 2; }
   }
   const auto scene_a = engine_a.world().get<vve::AssetSystem>().loadScene(path);
   const auto scene_b = engine_b.world().get<vve::AssetSystem>().loadScene(path);
   if (!scene_a || !scene_b) { return 3; }
   const auto instance_a = a.instantiateScene(*scene_a);
   const auto instance_b = b.instantiateScene(*scene_b);
   if (!instance_a || !instance_b || !instance_a->valid() || !instance_b->valid()) { return 4; }
   check(*instance_a != *instance_b, "instance_handles_differ");

   // Save B's imported object so a rejected removal must preserve its identity and contents.
   const auto objects_b = b.sceneInstanceObjects(*instance_b);
   if (!objects_b || objects_b->size() != 1U) { return 5; }
   const auto foreign_instance_removal = b.removeSceneInstance(*instance_a);
   check(!foreign_instance_removal && foreign_instance_removal.error() == vve::Error::missing_object,
         "foreign_instance_removal_rejected");
   const auto surviving_objects_b = b.sceneInstanceObjects(*instance_b);
   check(surviving_objects_b && surviving_objects_b->size() == 1U &&
         surviving_objects_b->front() == objects_b->front() &&
         b.objectTransform(objects_b->front()).has_value(), "own_instance_b_survives");
   const auto surviving_objects_a = a.sceneInstanceObjects(*instance_a);
   check(surviving_objects_a && surviving_objects_a->size() == 1U &&
         a.objectTransform(surviving_objects_a->front()).has_value(), "own_instance_a_survives");

   // Legitimate removals still work after the foreign handle has been rejected.
   check(b.removeSceneInstance(*instance_b).has_value(), "own_instance_removal_succeeds");
   return passed ? 0 : 6;
}
