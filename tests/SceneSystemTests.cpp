#include <cstdint>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for the v3 scene-system transform propagation.
 */
namespace {

   /**
    * @brief Returns the translation stored in a homogeneous transform matrix.
    * @param matrix Matrix to inspect.
    * @return Translation vector stored in the last matrix column.
    */
   [[nodiscard]] vve::math::Vec3 translationOf(const vve::math::Mat4 &matrix) {
      return vve::math::Vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
   }

} // namespace

/**
 * @brief Executes the scene-system regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   vve::v3::SceneSystem scene_system{};
   vve::v3::ImportedScene imported_scene{};
   imported_scene.handle = vve::v3::SceneHandle{vve::Handle{1}};
   imported_scene.name = "transform_test";
   imported_scene.nodes.push_back(vve::v3::ImportedSceneNode{
       .handle = vve::v3::SceneNodeHandle{vve::Handle{1}},
       .parent = {},
       .name = "Root",
       .local_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(1.0F, 0.0F, 0.0F))});
   imported_scene.nodes.push_back(vve::v3::ImportedSceneNode{
       .handle = vve::v3::SceneNodeHandle{vve::Handle{2}},
       .parent = vve::v3::SceneNodeHandle{vve::Handle{1}},
       .name = "Child",
       .local_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(2.0F, 0.0F, 0.0F))});
   imported_scene.nodes.push_back(vve::v3::ImportedSceneNode{
       .handle = vve::v3::SceneNodeHandle{vve::Handle{3}},
       .parent = vve::v3::SceneNodeHandle{vve::Handle{2}},
       .name = "Grandchild",
       .local_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(3.0F, 0.0F, 0.0F))});
   imported_scene.lights.push_back(vve::v3::ImportedLight{
       .handle = vve::v3::LightHandle{vve::Handle{10}},
       .node = vve::v3::SceneNodeHandle{vve::Handle{2}},
       .name = "Key",
       .type = vve::v3::SceneLightType::directional});
   imported_scene.cameras.push_back(vve::v3::ImportedCamera{
       .handle = vve::v3::CameraHandle{vve::Handle{11}},
       .node = vve::v3::SceneNodeHandle{vve::Handle{3}},
       .name = "Camera"});

   const auto scene_result = scene_system.instantiate(imported_scene);
   if (!scene_result) {
      return 1;
   }

   auto scene = *scene_result;
   if (!scene_system.updateTransforms(vve::v3::FrameContext{.frame_index = 7, .delta_seconds = 0.0}, scene)) {
      return 2;
   }

   if (scene.nodes.size() != 3) {
      return 3;
   }

   if (scene.lights.size() != 1 || scene.lights.front().node.value.value() != 2 ||
       scene.lights.front().type != vve::v3::SceneLightType::directional) {
      return 13;
   }

   if (scene.cameras.size() != 1 || scene.cameras.front().node.value.value() != 3) {
      return 14;
   }

   const auto root_translation = translationOf(scene.nodes[0].world_transform);
   if (root_translation.x != 1.0F || root_translation.y != 0.0F || root_translation.z != 0.0F) {
      return 4;
   }

   const auto child_translation = translationOf(scene.nodes[1].world_transform);
   if (child_translation.x != 3.0F || child_translation.y != 0.0F || child_translation.z != 0.0F) {
      return 5;
   }

   const auto grandchild_translation = translationOf(scene.nodes[2].world_transform);
   if (grandchild_translation.x != 6.0F || grandchild_translation.y != 0.0F || grandchild_translation.z != 0.0F) {
      return 6;
   }

   if (scene.nodes[0].last_updated_frame != 7 || scene.nodes[1].last_updated_frame != 7 ||
       scene.nodes[2].last_updated_frame != 7) {
      return 7;
   }

   scene.nodes[0].local_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(5.0F, 0.0F, 0.0F));
   if (!scene_system.updateTransforms(vve::v3::FrameContext{.frame_index = 7, .delta_seconds = 0.0}, scene)) {
      return 8;
   }

   const auto unchanged_grandchild_translation = translationOf(scene.nodes[2].world_transform);
   if (unchanged_grandchild_translation.x != 6.0F) {
      return 9;
   }

   if (!scene_system.updateTransforms(vve::v3::FrameContext{.frame_index = 8, .delta_seconds = 0.0}, scene)) {
      return 10;
   }

   const auto updated_grandchild_translation = translationOf(scene.nodes[2].world_transform);
   if (updated_grandchild_translation.x != 10.0F || scene.nodes[2].last_updated_frame != 8) {
      return 11;
   }

   scene.nodes[1].parent = vve::v3::SceneNodeHandle{vve::Handle{99}};
   if (scene_system.updateTransforms(vve::v3::FrameContext{.frame_index = 9, .delta_seconds = 0.0}, scene)) {
      return 12;
   }

   return 0;
}
