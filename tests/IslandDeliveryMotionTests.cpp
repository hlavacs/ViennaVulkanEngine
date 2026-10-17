/** @file @brief CPU regression measurements for fixed-step aircraft presentation and camera-relative jitter. */
import std;
import VVEngine;
import Island.Game;

namespace {
int checks{};   ///< Number of deterministic motion assertions.
int failures{}; ///< Number of broken presentation contracts.

/** @brief Emits every failure so timing scenarios remain independently diagnosable. */
void check(bool passed, std::string_view name) {
   ++checks;
   if (!passed) { ++failures; }
   std::cout << "check=" << name << " passed=" << passed << '\n';
}

/** @brief Returns aircraft screen height in Full HD pixels using the real camera's orientation and field of view. */
double aircraftPixelY(const island::GameSystem &game) {
   const auto ahead = vve::math::normalize(game.camera.forward.value);
   const auto right = vve::math::normalize(vve::math::cross(ahead, vve::Vec3{0, 1, 0}));
   const auto up = vve::math::cross(right, ahead);
   const auto relative = vve::math::subtract(game.displayed.position, game.camera.position.value);
   return 540.0 / std::tan(static_cast<double>(game.camera.fov_y.radians) * 0.5)
        * static_cast<double>(vve::math::dot(relative, up)) / static_cast<double>(vve::math::dot(relative, ahead));
}

/** @brief Measures relative distance range, screen-height range, mean chase distance, axis drift and zero-tick slip. */
std::array<double, 5> cameraCruise(vve::RenderSystem &render, std::span<const float> schedule, bool sampled) {
   island::GameSystem game;
   game.render = &render;
   game.displayed.position = {0, 80, -1200};
   game.displayed.velocity = {0, 0, -50};
   game.displayed.grounded = false;
   game.displayed.phase = island::Phase::flying;
   game.setCamera(0.0F);
   double time{}, minimum = 1e9, maximum = -1e9, pixelMinimum = 1e9, pixelMaximum = -1e9, sum{}, axisDrift{}, zeroTickSlip{};
   vve::Vec3 referenceAxis{};
   auto previousRelative = vve::math::subtract(game.displayed.position, game.camera.position.value);
   std::size_t frames{}, measured{};
   int previousTick{};
   // Synthetic constant-speed samples isolate camera timing from aerodynamic acceleration and terrain generation.
   while (time < 12.0) {
      const float dt = schedule[frames++ % schedule.size()];
      time += dt;
      const int tick = static_cast<int>(std::floor(time * 120.0 + 1e-7));
      game.displayed.position.z = -1200.0F - 50.0F * static_cast<float>(sampled ? static_cast<double>(tick) / 120.0 : time);
      game.setCamera(dt);
      const auto relative = vve::math::subtract(game.displayed.position, game.camera.position.value);
      const auto axis = vve::math::normalize(game.camera.forward.value);
      if (time > 3.0) {
         const double distance = vve::math::dot(relative, island::forward(game.displayed));
         const double pixel = aircraftPixelY(game);
         if (measured == 0) { referenceAxis = axis; }
         axisDrift = std::max(axisDrift, static_cast<double>(vve::math::length(vve::math::subtract(axis, referenceAxis))));
         minimum = std::min(minimum, distance); maximum = std::max(maximum, distance); sum += distance;
         pixelMinimum = std::min(pixelMinimum, pixel); pixelMaximum = std::max(pixelMaximum, pixel);
         if (sampled && tick == previousTick) {
            zeroTickSlip = std::max(zeroTickSlip, static_cast<double>(vve::math::length(vve::math::subtract(relative, previousRelative))));
         }
         ++measured;
      }
      previousTick = tick;
      previousRelative = relative;
   }
   return {maximum - minimum, pixelMaximum - pixelMinimum, sum / static_cast<double>(measured), axisDrift, zeroTickSlip};
}

/** @brief Reports camera-relative metrics for both raw fixed-tick samples and already-continuous presentation samples. */
void cruiseSchedules(vve::RenderSystem &render) {
   constexpr std::array rates{30.0F, 60.0F, 144.0F};
   constexpr std::array irregular{1.0F / 144.0F, 1.0F / 240.0F, 1.0F / 50.0F, 1.0F / 90.0F, 1.0F / 30.0F, 1.0F / 200.0F, 1.0F / 70.0F};
   // Full HD pixel measurements reveal visible model motion even when its world trajectory looks correct.
   for (const bool sampled : {true, false}) {
      for (const float rate : rates) {
         const std::array schedule{1.0F / rate};
         const auto metric = cameraCruise(render, schedule, sampled);
         std::cout << "motion_case=" << (sampled ? "fixed_tick" : "continuous") << " hz=" << rate
                   << " relative_range_m=" << metric[0] << " vertical_range_px=" << metric[1] << " mean_follow_m=" << metric[2]
                   << " axis_drift=" << metric[3] << " zero_tick_slip_m=" << metric[4] << '\n';
         check(metric[0] < 0.003 && metric[1] < 0.05 && metric[3] < 0.0001, "constant_cruise_has_stable_camera_relative_pose");
         check(std::abs(metric[2] - 24.0) < 0.003 && metric[4] < 0.003, "chase_camera_tracks_translation_without_world_lag");
      }
      const auto metric = cameraCruise(render, irregular, sampled);
      std::cout << "motion_case=" << (sampled ? "fixed_tick" : "continuous") << " hz=irregular"
                << " relative_range_m=" << metric[0] << " vertical_range_px=" << metric[1] << " mean_follow_m=" << metric[2]
                << " axis_drift=" << metric[3] << " zero_tick_slip_m=" << metric[4] << '\n';
      check(metric[0] < 0.003 && metric[1] < 0.05 && metric[3] < 0.0001 && metric[4] < 0.003,
            "irregular_frames_do_not_move_aircraft_relative_to_camera");
   }
}

/** @brief Keeps a stationary view stable through high-rate frames, long pauses and explicit camera-mode changes. */
void stationaryAndSwitches(vve::RenderSystem &render) {
   island::GameSystem game;
   game.render = &render;
   game.displayed.position = {0, 80, -1200};
   game.setCamera(0.0F);
   const auto initial = game.camera.position.value;
   // A view with no aircraft motion must not drift during paused or zero-physics frames.
   for (const float dt : {1.0F / 144.0F, 1.0F / 240.0F, 0.0F, 0.1F, 1.0F, 0.0F}) { game.setCamera(dt); }
   check(vve::math::length(vve::math::subtract(initial, game.camera.position.value)) < 0.001F, "stationary_pause_and_long_frame_keep_camera_still");
   const auto aircraft = game.displayed.position;
   bool finite = true;
   // Camera switches change only the view; the fixed-step aircraft remains untouched.
   for (const int mode : {1, 2, 0, 2, 1, 0}) {
      game.cameraMode = mode;
      game.setCamera(1.0F / 144.0F);
      finite = finite && std::isfinite(game.camera.position.value.x) && std::isfinite(game.camera.position.value.y)
                      && std::isfinite(game.camera.position.value.z) && vve::math::length(game.camera.forward.value) > 0.1F;
   }
   check(finite && vve::math::length(vve::math::subtract(aircraft, game.displayed.position)) == 0.0F, "camera_mode_switches_preserve_aircraft_and_valid_view");
}

/** @brief Compares the real presentation helper with independently sampled 120 Hz physics and continuous wall-clock motion. */
void presentationCruise(vve::RenderSystem &render, std::span<const float> schedule, std::string_view name) {
   constexpr float tick = 1.0F / 120.0F;
   const island::Controls controls{.throttle = (0.15F + 0.002F * 50.0F * 50.0F) / 6.0F};
   island::Flight flight;
   flight.position = {0, 80, -1200}; flight.velocity = {0, 0, -50};
   flight.grounded = false; flight.phase = island::Phase::flying; flight.throttle = controls.throttle;
   std::vector<island::Flight> reference{flight};
   // Reference frames use only the original integrator, without camera or presentation state.
   for (int index = 0; index < 1200; ++index) {
      auto next = reference.back(); island::step(next, controls, tick); reference.push_back(next);
   }
   island::GameSystem game;
   game.render = &render; game.previousTick = game.displayed = flight; game.setCamera(0.0F);
   double time{}, renderError{}, authorityError{}, speedErrorSum{}, relativeError{};
   std::size_t frames{}, speeds{}, zeroTicks{}, movingZeroTicks{};
   // Frames without a physics tick must still move between the previous and current samples.
   while (time < 8.0) {
      const float dt = schedule[frames++ % schedule.size()];
      const auto before = flight.position, displayedBefore = game.displayed.position;
      time += dt;
      game.advanceFlight(flight, controls, dt); game.setCamera(dt);
      const double at = std::max(0.0, time / static_cast<double>(tick) - 1.0);
      const auto index = static_cast<std::size_t>(std::floor(at));
      const float alpha = static_cast<float>(at - static_cast<double>(index));
      const auto expected = vve::math::add(reference[index].position,
         vve::math::scale(vve::math::subtract(reference[index + 1].position, reference[index].position), alpha));
      renderError = std::max(renderError, static_cast<double>(vve::math::length(vve::math::subtract(expected, game.displayed.position))));
      const auto authoritative = static_cast<std::size_t>(std::floor((time + 0.000001) / static_cast<double>(tick)));
      authorityError = std::max(authorityError, static_cast<double>(vve::math::length(vve::math::subtract(flight.position, reference[authoritative].position))));
      relativeError = std::max(relativeError, std::abs(static_cast<double>(game.camera.position.value.z - game.displayed.position.z) - 24.0));
      if (time > 0.1) {
         const double travelled = vve::math::length(vve::math::subtract(game.displayed.position, displayedBefore));
         speedErrorSum += std::pow(travelled / dt - 50.0, 2); ++speeds;
         if (vve::math::length(vve::math::subtract(before, flight.position)) == 0.0F) {
            ++zeroTicks;
            if (travelled > 0.001) { ++movingZeroTicks; }
         }
      }
   }
   const double speedRms = std::sqrt(speedErrorSum / static_cast<double>(speeds));
   std::cout << "presentation_case=" << name << " render_error_m=" << renderError << " authority_error_m=" << authorityError
             << " speed_error_rms=" << speedRms << " zero_tick_frames=" << zeroTicks << " moving_zero_tick_frames=" << movingZeroTicks
             << " relative_error_m=" << relativeError << '\n';
   check(renderError < 0.003 && authorityError < 0.003, "presentation_matches_120hz_reference_without_changing_physics");
   check(speedRms < 0.1 && relativeError < 0.003 && movingZeroTicks == zeroTicks, "presentation_moves_smoothly_even_without_physics_tick");
}

/** @brief Verifies shortest-arc interpolation, current metadata, paused fractions, terminal snapping and reset history. */
void presentationState() {
   constexpr float tick = 1.0F / 120.0F;
   island::GameSystem game;
   island::Flight current;
   current.position = {0, 80, -1200}; current.velocity = {2, 4, -50};
   current.grounded = false; current.phase = island::Phase::flying;
   current.heading = -std::numbers::pi_v<float> + 0.02F; current.pitch = 0.2F; current.bank = 0.4F;
   current.throttle = 0.8F; current.elapsed = 4.0F; current.targetIsland = 99; current.deliveries = 4;
   game.previousTick = current;
   game.previousTick.position.z += 8.0F; game.previousTick.velocity = {0, 0, -48};
   game.previousTick.heading = std::numbers::pi_v<float> - 0.02F;
   game.previousTick.pitch = -0.2F; game.previousTick.bank = -0.4F; game.previousTick.throttle = 0.4F; game.previousTick.elapsed = 3.0F;
   game.accumulator = tick * 0.5F;
   game.advanceFlight(current, {}, 0.0F);
   check(std::abs(std::abs(game.displayed.heading) - std::numbers::pi_v<float>) < 0.0001F
         && std::abs(game.displayed.pitch) < 0.0001F && std::abs(game.displayed.bank) < 0.0001F,
         "heading_wrap_uses_shortest_arc_and_attitudes_interpolate");
   check(std::abs(game.displayed.position.z + 1196.0F) < 0.0001F && std::abs(game.displayed.velocity.y - 2.0F) < 0.0001F
         && std::abs(game.displayed.throttle - 0.6F) < 0.0001F && std::abs(game.displayed.elapsed - 3.5F) < 0.0001F
         && game.displayed.targetIsland == 99 && game.displayed.deliveries == 4, "pose_and_animation_clock_interpolate_with_current_metadata");
   const auto displayed = game.displayed.position, authoritative = current.position;
   const float fraction = game.accumulator;
   // Pausing and opening the route map retain the partial frame instead of restarting interpolation.
   for (const bool map : {false, true}) {
      game.paused = !map; game.mapOpen = map; game.advanceFlight(current, {}, 1.0F);
      check(game.accumulator == fraction && vve::math::length(vve::math::subtract(current.position, authoritative)) == 0.0F
            && vve::math::length(vve::math::subtract(game.displayed.position, displayed)) == 0.0F, "pause_and_map_preserve_pose_and_interpolation_fraction");
   }
   game.paused = game.mapOpen = false;
   game.advanceFlight(current, {}, std::numeric_limits<float>::quiet_NaN()); game.advanceFlight(current, {}, -1.0F);
   check(game.accumulator == fraction && vve::math::length(vve::math::subtract(current.position, authoritative)) == 0.0F,
         "invalid_frame_deltas_do_not_advance_authoritative_physics");
   // Crash and delivery stops must show their final authoritative pose without a one-tick visual drift.
   for (const auto phase : {island::Phase::crashed, island::Phase::delivered}) {
      current.phase = phase; game.advanceFlight(current, {}, 0.0F);
      check(vve::math::length(vve::math::subtract(game.displayed.position, current.position)) == 0.0F
            && game.displayed.elapsed == current.elapsed, "terminal_flight_snaps_to_stopped_authoritative_pose");
   }
   current = island::Flight{}; game.previousTick = game.displayed = current; game.accumulator = 0.0F;
   game.advanceFlight(current, {}, 0.0F);
   check(vve::math::length(vve::math::subtract(game.displayed.position, current.position)) == 0.0F,
         "reset_history_cannot_interpolate_from_old_airborne_pose");
   current.position = vve::math::add(island::islandCenter(1), vve::Vec3{0, island::aircraftGroundHeight, -160});
   current.originIsland = 0; current.targetIsland = 1; current.deliveries = 1; current.phase = island::Phase::delivered;
   game.accumulator = tick * 0.5F;
   check(game.collectNextCargo(current), "cargo_collection_after_stop_resets_presentation_history");
   game.advanceFlight(current, {}, 0.0F);
   check(game.accumulator == 0.0F && vve::math::length(vve::math::subtract(game.displayed.position, current.position)) == 0.0F,
         "next_cargo_never_renders_old_arrival_pose");
   island::Flight large;
   large.position = {0, 80, -1200}; large.velocity = {0, 0, -50}; large.grounded = false; large.phase = island::Phase::flying;
   auto reference = large;
   island::GameSystem late; late.previousTick = large;
   // Bounded demonstrations can group many real fixed ticks into one frame without altering their trajectory.
   for (int index = 0; index < 60; ++index) { island::step(reference, {}, tick); }
   late.advanceFlight(large, {}, 0.5F);
   check(vve::math::length(vve::math::subtract(large.position, reference.position)) < 0.001F
         && std::abs(large.elapsed - reference.elapsed) < 0.0001F, "large_frame_batch_preserves_every_fixed_physics_step");
}
} // namespace

/** @brief Runs actual game-camera functions through a headless facade engine and prints all regression metrics. */
int main() {
   vve::Engine<> engine;
   auto world = engine.world();
   auto &render = world.get<vve::RenderSystem>();
   stationaryAndSwitches(render);
   cruiseSchedules(render);
   constexpr std::array<float, 1> rate30{1.0F / 30.0F}, rate60{1.0F / 60.0F}, rate144{1.0F / 144.0F};
   constexpr std::array irregular{1.0F / 144.0F, 1.0F / 240.0F, 1.0F / 50.0F, 1.0F / 90.0F, 1.0F / 30.0F};
   presentationCruise(render, rate30, "30Hz"); presentationCruise(render, rate60, "60Hz");
   presentationCruise(render, rate144, "144Hz"); presentationCruise(render, irregular, "irregular");
   presentationState();
   check(render.renderedFrameCount() == 0, "motion_verification_needs_no_window_or_gpu_frame");
   std::cout << "motion_checks=" << checks << " failures=" << failures << '\n';
   return failures == 0 ? 0 : 1;
}
