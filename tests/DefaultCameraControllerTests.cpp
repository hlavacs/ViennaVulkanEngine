import std;
import VEEngine;

/**
 * @file
 * @brief Unit tests for the facade default keyboard camera controller.
 */
namespace {

constexpr vve::Scalar epsilon = static_cast<vve::Scalar>(0.0001); ///< Tolerance for scalar camera checks.

/// @brief Preserves the legacy 2-unit and 0.25-radian steps at the clamped 0.1-second delta.
[[nodiscard]] auto makeController() -> vve::DefaultCameraController {
   return vve::DefaultCameraController{.eye = vve::Position{.value = vve::Vec3{0.0F, 0.0F, 0.0F}},
                                       .yaw = 0.0F,
                                       .pitch = 0.0F,
                                       .move_speed = 20.0F,
                                       .turn_speed = 2.5F,
                                       .max_pitch = 0.5F};
}

/// @brief Checks whether two scalar values are close enough for deterministic controller math.
[[nodiscard]] auto near(vve::Scalar lhs, vve::Scalar rhs) -> bool { return std::abs(lhs - rhs) <= epsilon; }

/// @brief Checks whether two vectors match within the test tolerance.
[[nodiscard]] auto near(vve::Vec3 lhs, vve::Vec3 rhs) -> bool {
   return near(lhs.x, rhs.x) && near(lhs.y, rhs.y) && near(lhs.z, rhs.z);
}

/// @brief Holds one key for a timed update; legacy cases use one second to exercise the clamp.
auto updateWithKey(vve::DefaultCameraController &controller, vve::InputState input, vve::Key key,
                   vve::DeltaTime dt = vve::DeltaTime{1}) -> vve::Camera {
   input.holdKey(static_cast<std::int32_t>(key));
   auto camera = controller.update(input, dt);
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
   if (!near(yawLeft.yaw, -yawLeft.turn_speed * 0.1F) || !near(yawRight.yaw, yawRight.turn_speed * 0.1F)) { return 6; }

   auto pitchUp = makeController();
   updateWithKey(pitchUp, input, vve::Key::up);
   auto pitchDown = makeController();
   updateWithKey(pitchDown, input, vve::Key::down);
   if (!near(pitchUp.pitch, -pitchUp.turn_speed * 0.1F) || !near(pitchDown.pitch, pitchDown.turn_speed * 0.1F)) { return 7; }

   auto highPitch = makeController();
   highPitch.pitch = highPitch.max_pitch - 0.01F;
   updateWithKey(highPitch, input, vve::Key::down);
   auto lowPitch = makeController();
   lowPitch.pitch = -lowPitch.max_pitch + 0.01F;
   updateWithKey(lowPitch, input, vve::Key::up);
   if (!near(highPitch.pitch, highPitch.max_pitch) || !near(lowPitch.pitch, -lowPitch.max_pitch)) { return 8; }
   return 0;
}

/// @brief Verifies that either Shift key doubles movement without changing the configured base speed.
[[nodiscard]] auto testShiftMovement(vve::InputState input) -> int {
   auto leftShift = makeController();
   input.holdKey(static_cast<std::int32_t>(vve::Key::left_shift));
   updateWithKey(leftShift, input, vve::Key::w);
   input.releaseKey(static_cast<std::int32_t>(vve::Key::left_shift));
   if (!near(leftShift.eye.value, vve::Vec3{0.0F, 0.0F, -4.0F}) || !near(leftShift.move_speed * 0.1F, 2.0F)) { return 9; }

   auto rightShift = makeController();
   input.holdKey(static_cast<std::int32_t>(vve::Key::right_shift));
   updateWithKey(rightShift, input, vve::Key::e);
   input.releaseKey(static_cast<std::int32_t>(vve::Key::right_shift));
   if (!near(rightShift.eye.value, vve::Vec3{0.0F, 4.0F, 0.0F}) || !near(rightShift.move_speed * 0.1F, 2.0F)) { return 10; }
   return 0;
}

/// @brief Verifies equal elapsed time gives equal movement and yaw, with bounded and nonnegative deltas.
[[nodiscard]] auto testDeltaTime(vve::InputState input) -> int {
   constexpr vve::Scalar timingTolerance = 1e-5F; ///< Ruling tolerance for frame-rate independence.
   auto single = makeController();
   single.move_speed = 10.0F;
   single.turn_speed = 10.0F;
   auto split = single;
   auto yawSingle = single;
   auto yawSplit = single;
   auto clamped = single;
   auto stationary = single;

   // One tenth of a second moves one unit, whether submitted in one frame or two.
   updateWithKey(single, input, vve::Key::w, vve::DeltaTime{0.1});
   updateWithKey(split, input, vve::Key::w, vve::DeltaTime{0.05});
   updateWithKey(split, input, vve::Key::w, vve::DeltaTime{0.05});
   const auto movementError = vve::math::length(vve::math::subtract(single.eye.value, vve::Vec3{0.0F, 0.0F, -1.0F}));
   const auto splitError = vve::math::length(vve::math::subtract(single.eye.value, split.eye.value));
   std::println("camera_timing movement_error={} split_error={}", movementError, splitError);
   if (movementError > timingTolerance || splitError > timingTolerance) { return 11; }

   // Turning uses the same elapsed-time rule, with a total yaw change of one radian.
   updateWithKey(yawSingle, input, vve::Key::right, vve::DeltaTime{0.1});
   updateWithKey(yawSplit, input, vve::Key::right, vve::DeltaTime{0.05});
   updateWithKey(yawSplit, input, vve::Key::right, vve::DeltaTime{0.05});
   std::println("camera_timing yaw_single={} yaw_split={}", yawSingle.yaw, yawSplit.yaw);
   if (std::abs(yawSingle.yaw - 1.0F) > timingTolerance ||
       std::abs(yawSplit.yaw - yawSingle.yaw) > timingTolerance) { return 12; }

   // A long stall still moves for exactly the capped duration, while negative or zero time does nothing.
   updateWithKey(clamped, input, vve::Key::w, vve::DeltaTime{10});
   const auto distance = vve::math::length(clamped.eye.value);
   const auto limit = clamped.move_speed * 0.1F;
   std::println("camera_timing clamped_distance={} limit={}", distance, limit);
   if (distance > limit || distance < limit - timingTolerance) { return 13; }
   // Check each nonpositive delta separately so opposite movement errors cannot cancel.
   for (const auto dt : {vve::DeltaTime{-1}, vve::DeltaTime{0}}) {
      updateWithKey(stationary, input, vve::Key::w, dt);
      updateWithKey(stationary, input, vve::Key::right, dt);
      if (vve::math::length(stationary.eye.value) > timingTolerance ||
          std::abs(stationary.yaw) > timingTolerance) { return 14; }
   }
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

   if (const auto result = testDeltaTime(input); result != 0) { return result; }
   if (const auto result = testMovement(input); result != 0) { return result; }
   if (const auto result = testAngles(input); result != 0) { return result; }
   if (const auto result = testShiftMovement(input); result != 0) { return result; }
   return 0;
}
