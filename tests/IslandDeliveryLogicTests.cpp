/** @file @brief Headless deterministic checks for the island airstrip cargo-delivery game. */
import std;
import Island.Logic;

namespace {

constexpr float dt = 1.0F / 60.0F; ///< Fixed step shared by playable flight and deterministic telemetry.
int failures{}; ///< Total failed assertions, reported in the process exit status.

/** @brief Compares facade vector components without relying on implementation-specific vector operators. */
[[nodiscard]] bool same(vve::Vec3 first, vve::Vec3 second) {
   return first.x == second.x && first.y == second.y && first.z == second.z;
}

/** @brief Reports machine-readable checks without losing failures in later scenarios. */
void check(bool passed, std::string_view name) {
   std::cout << "check=" << name << " passed=" << passed << '\n';
   if (!passed) { ++failures; }
}

/** @brief Advances a flight under a constant pilot command for a fixed number of frames. */
void advance(island::Flight &flight, island::Controls controls, int frames) {
   // Repeated fixed updates make all scenarios independent of wall-clock timing.
   for (int frame = 0; frame < frames; ++frame) { island::step(flight, controls, dt); }
}

/** @brief Runs the entire takeoff, channel crossing, landing, taxi and stationary unloading sequence. */
island::Flight mission() {
   island::Flight flight;
   // The demonstration uses exactly the same controls and physics as player input.
   for (int frame = 0; frame < 120 * 60 && flight.phase != island::Phase::delivered && flight.phase != island::Phase::crashed; ++frame) {
      const auto oldPosition = flight.position;
      const auto controls = island::autopilot(flight);
      island::step(flight, controls, dt);
      if (vve::math::length(vve::math::add(flight.position, vve::math::scale(oldPosition, -1.0F))) > 2.0F) {
         check(false, "autopilot_never_teleports");
         break;
      }
   }
   return flight;
}

/** @brief Verifies runway topology, facade direction math and basic stopped-state behavior. */
void geometryAndStartup() {
   check(island::terrainHeight(0.0F, 0.0F) == island::runwayHeight, "home_runway_flat");
   check(island::terrainHeight(60.0F, island::destinationZ + 200.0F) == island::runwayHeight, "destination_apron_flat");
   check(island::terrainHeight(0.0F, -1200.0F) == 0.0F, "channel_is_water");
   check(island::terrainHeight(280.0F, 0.0F) > 35.0F, "hills_clear_runway");
   island::Flight flight;
   const auto nose = island::forward(flight);
   check(nose.x == 0.0F && nose.y == 0.0F && nose.z == -1.0F, "departure_faces_destination");
   check(std::abs(island::remainingDistance(flight) - 2840.0F) < 0.01F, "delivery_distance_metres");
   advance(flight, {}, 600);
   check(flight.phase == island::Phase::ready && flight.position.z == 240.0F && flight.grounded, "stationary_departure_never_delivers");
   check(!island::phaseName(flight.phase).empty(), "phase_has_hud_label");
}

/** @brief Checks acceleration, required rotation speed and effective pilot inputs. */
void aircraftControls() {
   island::Flight flight;
   advance(flight, {.throttle = 1.0F, .pitch = 0.65F}, 60);
   check(flight.grounded && flight.takeoffs == 0 && -flight.velocity.z > 0.0F, "runway_acceleration_before_rotation");
   advance(flight, {.throttle = 1.0F, .pitch = 0.65F}, 600);
   check(!flight.grounded && flight.takeoffs == 1 && flight.position.y > 15.0F && flight.phase == island::Phase::flying,
         "airspeed_and_rotation_produce_takeoff");
   const float oldHeading = flight.heading;
   advance(flight, {.throttle = 0.85F, .pitch = 0.2F, .roll = 0.4F, .yaw = 0.25F}, 60);
   check(flight.heading > oldHeading && flight.bank > 0.0F && flight.position.x > 0.0F, "right_bank_and_rudder_turn_right");
   check(std::abs(vve::math::length(island::forward(flight)) - 1.0F) < 0.0001F, "pitched_direction_stays_normalized");
   island::Flight stopped;
   advance(stopped, {.throttle = 1.0F}, 300);
   const float rollingSpeed = vve::math::length(stopped.velocity);
   advance(stopped, {.brake = 1.0F}, 300);
   check(rollingSpeed > 8.0F && vve::math::length(stopped.velocity) < 0.01F, "wheel_brakes_stop_ground_roll");
   const auto before = stopped.position;
   island::step(stopped, {}, std::numeric_limits<float>::quiet_NaN());
   island::step(stopped, {}, -1.0F);
   check(same(stopped.position, before), "invalid_timestep_ignored");
   island::Flight saturated, normalized;
   island::step(saturated, {.throttle = 50.0F, .pitch = 50.0F, .roll = -50.0F, .yaw = 50.0F, .brake = -50.0F}, dt);
   island::step(normalized, {.throttle = 1.0F, .pitch = 1.0F, .roll = -1.0F, .yaw = 1.0F}, dt);
   check(same(saturated.position, normalized.position) && saturated.pitch == normalized.pitch && saturated.bank == normalized.bank,
         "pilot_commands_are_bounded");
   island::step(saturated, {.throttle = std::numeric_limits<float>::infinity(), .pitch = std::numeric_limits<float>::quiet_NaN()}, dt);
   check(std::isfinite(saturated.pitch) && std::isfinite(saturated.throttle), "nonfinite_commands_do_not_poison_state");
}

/** @brief Rejects flyovers, arrival without flight, wrong destinations and interrupted unloading. */
void deliveryRules() {
   island::Flight parked;
   parked.position.z = -2600.0F;
   advance(parked, {.brake = 1.0F}, 180);
   check(parked.phase == island::Phase::ready && parked.stoppedSeconds == 0.0F, "destination_parking_requires_a_flight");
   island::Flight flyover;
   flyover.position = {0.0F, 45.0F, -2600.0F};
   flyover.grounded = false;
   flyover.phase = island::Phase::flying;
   flyover.takeoffs = 1;
   flyover.velocity = {0.0F, 0.0F, -34.0F};
   advance(flyover, {.throttle = 0.41F}, 180);
   check(flyover.phase == island::Phase::flying && flyover.stoppedSeconds == 0.0F, "flyover_cannot_deliver");
   island::Flight wrongIsland;
   wrongIsland.phase = island::Phase::landed;
   wrongIsland.takeoffs = wrongIsland.landings = 1;
   advance(wrongIsland, {.brake = 1.0F}, 180);
   check(wrongIsland.phase == island::Phase::landed && wrongIsland.stoppedSeconds == 0.0F, "home_landing_cannot_deliver");
   island::Flight unloading = wrongIsland;
   unloading.position.z = -2600.0F;
   advance(unloading, {.brake = 1.0F}, 60);
   check(unloading.phase == island::Phase::landed && unloading.stoppedSeconds > 0.9F, "unloading_requires_stationary_delay");
   unloading.velocity.z = -3.0F;
   island::step(unloading, {}, dt);
   check(unloading.stoppedSeconds == 0.0F, "movement_resets_unloading_delay");
   advance(unloading, {.brake = 1.0F}, 180);
   check(unloading.phase == island::Phase::delivered, "stopped_destination_landing_unloads");
}

/** @brief Checks survivable touchdown and independent water, terrain, speed and sink-rate failures. */
void landingSafety() {
   island::Flight approach;
   approach.position = {0.0F, 5.26F, -2180.0F};
   approach.velocity = {0.0F, -2.0F, -34.0F};
   approach.pitch = -0.06F;
   approach.grounded = false;
   approach.phase = island::Phase::flying;
   approach.takeoffs = 1;
   auto safe = approach;
   island::step(safe, {.throttle = 0.4F, .pitch = -0.2F}, dt);
   check(safe.phase == island::Phase::landed && safe.landings == 1 && safe.grounded && safe.touchdownSpeed < 4.0F,
         "aligned_gentle_touchdown_succeeds");
   auto hard = approach;
   hard.velocity.y = -9.0F;
   island::step(hard, {}, dt);
   check(hard.phase == island::Phase::crashed, "hard_touchdown_crashes");
   auto fast = approach;
   fast.velocity.z = -55.0F;
   island::step(fast, {}, dt);
   check(fast.phase == island::Phase::crashed, "excessive_touchdown_speed_crashes");
   auto water = approach;
   water.position = {0.0F, 1.26F, -1200.0F};
   island::step(water, {}, dt);
   check(water.phase == island::Phase::crashed, "water_contact_crashes");
   auto grass = approach;
   grass.position.x = 40.0F;
   island::step(grass, {}, dt);
   check(grass.phase == island::Phase::crashed, "offrunway_contact_crashes");
   auto apronLanding = approach;
   apronLanding.position = {40.0F, 5.26F, island::destinationZ - 200.0F};
   island::step(apronLanding, {}, dt);
   check(apronLanding.phase == island::Phase::crashed, "apron_is_not_a_landing_strip");
   auto banked = approach;
   banked.bank = 0.5F;
   island::step(banked, {}, dt);
   check(banked.phase == island::Phase::crashed, "banked_touchdown_crashes");
   const auto wreck = hard.position;
   advance(hard, {.throttle = 1.0F}, 60);
   check(same(hard.position, wreck), "crashed_aircraft_stays_stopped");
}

/** @brief Keeps ground taxiing on the same connected pavement polygons drawn at both airfields. */
void taxiPavement() {
   // Test both airfields independently so an offset error cannot hide behind the departure strip.
   for (const float centre : {0.0F, island::destinationZ}) {
      island::Flight taxiway;
      taxiway.position = {22.0F, island::aircraftGroundHeight, centre - 225.0F};
      advance(taxiway, {.brake = 1.0F}, 60);
      check(taxiway.grounded && taxiway.phase != island::Phase::crashed, "connected_taxiway_accepts_ground_contact");
      island::Flight apron;
      apron.position = {45.0F, island::aircraftGroundHeight, centre - 200.0F};
      apron.heading = std::numbers::pi_v<float> / 2.0F;
      advance(apron, {.throttle = 0.4F}, 60);
      check(apron.grounded && apron.phase != island::Phase::crashed && apron.position.x > 45.0F,
            "apron_accepts_ground_roll");
      island::Flight offPavement;
      offPavement.position = {22.0F, island::aircraftGroundHeight, centre + 100.0F};
      island::step(offPavement, {}, dt);
      check(offPavement.phase == island::Phase::crashed, "ground_contact_outside_pavement_crashes");
   }
   island::Flight unloadingPad;
   unloadingPad.position = {-23.0F, island::aircraftGroundHeight, island::destinationZ - 200.0F};
   island::step(unloadingPad, {}, dt);
   check(unloadingPad.grounded && unloadingPad.phase != island::Phase::crashed, "destination_unloading_pad_accepts_ground_contact");
}

/** @brief Verifies that identical input streams, including randomized controls, produce identical state. */
void repeatability() {
   island::Flight first, second;
   std::mt19937 random{20261017U};
   std::uniform_real_distribution<float> command{-1.0F, 1.0F};
   // A fixed seed tests determinism through takeoff and varied control commands.
   for (int frame = 0; frame < 1200; ++frame) {
      const island::Controls controls{1.0F, 0.55F, frame > 600 ? command(random) * 0.1F : 0.0F,
                                      frame > 600 ? command(random) * 0.1F : 0.0F, 0.0F};
      island::step(first, controls, dt);
      island::step(second, controls, dt);
   }
   check(same(first.position, second.position) && same(first.velocity, second.velocity) && first.heading == second.heading
         && first.phase == second.phase && first.elapsed == second.elapsed, "fixed_seed_flights_repeat_exactly");
}

} // namespace

/** @brief Emits compact telemetry and returns failure if any flight or mission contract is broken. */
int main() {
   geometryAndStartup();
   aircraftControls();
   deliveryRules();
   landingSafety();
   taxiPavement();
   repeatability();
   const auto flight = mission();
   check(flight.phase == island::Phase::delivered && flight.takeoffs == 1 && flight.landings == 1, "complete_delivery_through_real_controls");
   check(flight.elapsed < 120.0F && flight.maxAltitude > 40.0F && flight.maxAltitude < 120.0F, "mission_duration_and_altitude_budget");
   check(flight.position.z >= -2700.0F && flight.position.z <= -2530.0F && vve::math::length(flight.velocity) < 0.8F,
         "completed_delivery_stopped_inside_unloading_zone");
   const auto repeat = mission();
   check(same(flight.position, repeat.position) && flight.elapsed == repeat.elapsed, "autonomous_delivery_repeats_exactly");
   std::cout << "mission_phase=\"" << island::phaseName(flight.phase) << "\" elapsed=" << flight.elapsed
             << " x=" << flight.position.x << " y=" << flight.position.y << " z=" << flight.position.z
             << " max_altitude=" << flight.maxAltitude << " touchdown_sink=" << flight.touchdownSpeed
             << " takeoffs=" << flight.takeoffs << " landings=" << flight.landings << " failures=" << failures << '\n';
   return failures == 0 ? 0 : 1;
}
