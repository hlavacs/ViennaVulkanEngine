#include <expected>
#include <string>

import VEEngine;

/**
 * @file
 * @brief Regression tests for the public world facade.
 */
namespace {

   /// @brief Test component storing a 2D position.
   struct Position {
      /// @brief Horizontal coordinate used by the test.
      int x{0};
      /// @brief Vertical coordinate used by the test.
      int y{0};

      [[nodiscard]] friend bool operator==(const Position &, const Position &) = default;
   };

   /// @brief Test component storing a string tag.
   struct Tag {
      /// @brief Tag text used by the test.
      std::string value{};

      [[nodiscard]] friend bool operator==(const Tag &, const Tag &) = default;
   };

   /// @brief Captures a camera supplied through the runtime bridge.
   struct CameraCapture {
      /// @brief Last camera observed by the callback.
      vve::Camera camera{};
      /// @brief Whether the callback was invoked.
      bool called{false};
   };

   /// @brief Runtime callback used to verify `World::setCamera()` forwarding.
   [[nodiscard]] std::expected<void, vve::Error> captureCamera(void *context, const vve::Camera &camera) {
      if (context == nullptr) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      auto &capture = *static_cast<CameraCapture *>(context);
      capture.camera = camera;
      capture.called = true;
      return {};
   }

} // namespace

/**
 * @brief Executes the world-facade regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   vve::ECS<> ecs{};
   vve::World world{ecs};

   const auto entity_result = world.createEntity();
   if (!entity_result) {
      return 1;
   }

   const auto entity = *entity_result;
   const auto exists_result = world.exists(entity);
   if (!exists_result || !*exists_result) {
      return 2;
   }

   if (!world.addComponent(entity, Position{1, 2})) {
      return 3;
   }

   if (!world.addComponent(entity, Tag{"player"})) {
      return 4;
   }

   const auto position_result = world.getComponent<Position>(entity);
   if (!position_result || !position_result->has_value() || **position_result != Position{1, 2}) {
      return 5;
   }

   // `modifyComponent()` should read, mutate, and write back the component.
   if (!world.modifyComponent<Position>(entity, [](Position &position) {
          position.x += 10;
          position.y += 20;
       })) {
      return 6;
   }

   const auto moved_position_result = world.getComponent<Position>(entity);
   if (!moved_position_result || !moved_position_result->has_value() || **moved_position_result != Position{11, 22}) {
      return 7;
   }

   const auto spawned_result = world.spawn(Position{7, 9}, Tag{"enemy"});
   if (!spawned_result) {
      return 8;
   }

   const auto spawned = *spawned_result;
   const auto has_tag_result = world.hasComponent<Tag>(spawned);
   if (!has_tag_result || !*has_tag_result) {
      return 9;
   }

   // The transform helper methods should operate on the standard transform
   if (!world.addComponent(spawned, vve::Transform{})) { // component without exposing storage internals.
      return 91;
   }

   if (!world.translate(spawned, vve::math::Vec3(1.0F, 2.0F, 3.0F))) {
      return 92;
   }

   if (!world.setScale(spawned, vve::math::Vec3(2.0F, 2.0F, 2.0F))) {
      return 93;
   }

   const auto transform_result = world.getTransform(spawned);
   if (!transform_result || !transform_result->has_value()) {
      return 94;
   }

   const auto &transform = **transform_result;
   if (transform.translation.x != 1.0F || transform.translation.y != 2.0F || transform.translation.z != 3.0F ||
       transform.scale.x != 2.0F || transform.scale.y != 2.0F || transform.scale.z != 2.0F) {
      return 95;
   }

   if (!world.setComponent(spawned, Position{4, 8})) {
      return 10;
   }

   const auto updated_position_result = world.getComponent<Position>(spawned);
   if (!updated_position_result || !updated_position_result->has_value() ||
       **updated_position_result != Position{4, 8}) {
      return 11;
   }

   if (!world.removeComponent<Tag>(spawned)) {
      return 12;
   }

   const auto has_removed_tag_result = world.hasComponent<Tag>(spawned);
   if (!has_removed_tag_result || *has_removed_tag_result) {
      return 13;
   }

   if (!world.destroyObject(spawned)) {
      return 14;
   }

   const auto exists_after_destroy_result = world.exists(spawned);
   if (!exists_after_destroy_result || *exists_after_destroy_result) {
      return 15;
   }

   // A world without runtime access should expose empty window and input state.
   if (world.findWindow("main").has_value()) {
      return 16;
   }

   if (world.input().isKeyDown('W')) {
      return 17;
   }

   if (world.loadScene("assets/example.glb")) {
      return 18;
   }

   if (world.setCamera(vve::Camera{})) {
      return 181;
   }

   vve::ECS<> runtime_ecs{};
   CameraCapture camera_capture{};
   vve::detail::WorldRuntimeAccess runtime_access{};
   runtime_access.set_camera = &captureCamera;
   runtime_access.set_camera_context = &camera_capture;

   vve::World runtime_world{runtime_ecs, runtime_access};
   const auto camera = vve::Camera::lookAt(vve::math::Vec3(2.0F, 3.0F, 4.0F),
                                           vve::math::Vec3(2.0F, 3.0F, 0.0F),
                                           vve::math::Vec3(0.0F, 1.0F, 0.0F),
                                           0.75F, 0.2F, 250.0F);

   if (!runtime_world.setCamera(camera)) {
      return 182;
   }

   if (!camera_capture.called || camera_capture.camera.position.x != 2.0F ||
       camera_capture.camera.position.y != 3.0F || camera_capture.camera.position.z != 4.0F ||
       camera_capture.camera.view_transform[3][0] != -2.0F ||
       camera_capture.camera.view_transform[3][1] != -3.0F ||
       camera_capture.camera.view_transform[3][2] != -4.0F ||
       camera_capture.camera.vertical_fov_radians != 0.75F ||
       camera_capture.camera.near_plane != 0.2F ||
       camera_capture.camera.far_plane != 250.0F) {
      return 183;
   }

   vve::InputState input{};
   vve::detail::pressKey(input, 'W');
   if (!input.isKeyDown('W') || !input.wasKeyPressed('W')) {
      return 19;
   }

   vve::detail::beginInputFrame(input);
   if (input.isKeyDown('W') || input.wasKeyPressed('W')) {
      return 20;
   }

   vve::detail::holdKey(input, 'W');
   if (!input.isKeyDown('W') || input.wasKeyPressed('W')) {
      return 21;
   }

   vve::detail::releaseKey(input, 'W');
   if (input.isKeyDown('W') || !input.wasKeyReleased('W')) {
      return 22;
   }

   return 0;
}
