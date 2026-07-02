import std;
import VEEngine;

/**
 * @file
 * @brief Unit tests for the facade default keyboard camera controller.
 */
namespace {

constexpr vve::Scalar epsilon = static_cast<vve::Scalar>(0.0001); ///< Tolerance for scalar camera checks.

/// @brief Creates a deterministic controller state shared by movement tests.
[[nodiscard]] auto makeController() -> vve::DefaultCameraController {
   return vve::DefaultCameraController{.eye = vve::Position{.value = vve::Vec3{0.0F, 0.0F, 0.0F}},
                                       .yaw = 0.0F,
                                       .pitch = 0.0F,
                                       .move_step = 2.0F,
                                       .turn_step = 0.25F,
                                       .max_pitch = 0.5F};
}

/// @brief Checks whether two scalar values are close enough for deterministic controller math.
[[nodiscard]] auto near(vve::Scalar lhs, vve::Scalar rhs) -> bool { return std::abs(lhs - rhs) <= epsilon; }

/// @brief Checks whether two vectors match within the test tolerance.
[[nodiscard]] auto near(vve::Vec3 lhs, vve::Vec3 rhs) -> bool {
   return near(lhs.x, rhs.x) && near(lhs.y, rhs.y) && near(lhs.z, rhs.z);
}

/// @brief Holds one key for a single controller update and clears it afterwards.
auto updateWithKey(vve::DefaultCameraController &controller, vve::InputState input, vve::Key key) -> vve::Camera {
   input.holdKey(static_cast<std::int32_t>(key));
   auto camera = controller.update(input);
   input.releaseKey(static_cast<std::int32_t>(key));
   input.beginFrame();
   return camera;
}

/// @brief Verifies movement in the controller basis and world-up flight axis.
[[nodiscard]] auto testMovement(vve::InputState input) -> int {
   auto forward = makeController();
   const auto forwardCamera = updateWithKey(forward, input, vve::Key::w);
   if (!near(forward.eye.value, vve::Vec3{0.0F, 0.0F, -2.0F}) || !near(forwardCamera.position.value, forward.eye.value)) {
      return 1;
   }
   if (!near(forwardCamera.forward.value, vve::Vec3{0.0F, 0.0F, -1.0F})) { return 2; }

   auto backward = makeController();
   updateWithKey(backward, input, vve::Key::s);
   if (!near(backward.eye.value, vve::Vec3{0.0F, 0.0F, 2.0F})) { return 3; }

   auto left = makeController();
   updateWithKey(left, input, vve::Key::a);
   auto right = makeController();
   updateWithKey(right, input, vve::Key::d);
   if (!near(left.eye.value, vve::Vec3{-2.0F, 0.0F, 0.0F}) || !near(right.eye.value, vve::Vec3{2.0F, 0.0F, 0.0F})) {
      return 4;
   }

   auto up = makeController();
   updateWithKey(up, input, vve::Key::e);
   auto down = makeController();
   updateWithKey(down, input, vve::Key::q);
   if (!near(up.eye.value, vve::Vec3{0.0F, 2.0F, 0.0F}) || !near(down.eye.value, vve::Vec3{0.0F, -2.0F, 0.0F})) {
      return 5;
   }
   return 0;
}

/// @brief Verifies yaw changes, pitch changes, and the pitch clamp.
[[nodiscard]] auto testAngles(vve::InputState input) -> int {
   auto yawLeft = makeController();
   updateWithKey(yawLeft, input, vve::Key::left);
   auto yawRight = makeController();
   updateWithKey(yawRight, input, vve::Key::right);
   if (!near(yawLeft.yaw, -yawLeft.turn_step) || !near(yawRight.yaw, yawRight.turn_step)) { return 6; }

   auto pitchUp = makeController();
   updateWithKey(pitchUp, input, vve::Key::up);
   auto pitchDown = makeController();
   updateWithKey(pitchDown, input, vve::Key::down);
   if (!near(pitchUp.pitch, -pitchUp.turn_step) || !near(pitchDown.pitch, pitchDown.turn_step)) { return 7; }

   auto highPitch = makeController();
   highPitch.pitch = highPitch.max_pitch - 0.01F;
   updateWithKey(highPitch, input, vve::Key::down);
   auto lowPitch = makeController();
   lowPitch.pitch = -lowPitch.max_pitch + 0.01F;
   updateWithKey(lowPitch, input, vve::Key::up);
   if (!near(highPitch.pitch, highPitch.max_pitch) || !near(lowPitch.pitch, -lowPitch.max_pitch)) { return 8; }
   return 0;
}

} // namespace

/**
 * @brief Runs facade camera controller tests with synthetic facade input.
 */
int main() {
   auto engine = vve::EngineBuilder<>{}
                    .applicationName("default-camera-controller-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
                    .addWindow(vve::WindowSetup{}
                                  .id("main")
                                  .title("default-camera-controller-tests")
                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                  .visible(false))
                    .build();
   auto input = engine.world().get<vve::WindowSystem>().input();

   if (const auto result = testMovement(input); result != 0) { return result; }
   if (const auto result = testAngles(input); result != 0) { return result; }
   return 0;
}
