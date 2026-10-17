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
island::Flight mission(island::Flight flight = {}, int seconds = 0) {
   if (seconds <= 0) { seconds = static_cast<int>(std::ceil(island::remainingDistance(flight) / 34.0F)) + 260; }
   // The demonstration uses exactly the same controls and physics as player input.
   for (int frame = 0; frame < seconds * 60 && flight.phase != island::Phase::delivered && flight.phase != island::Phase::crashed; ++frame) {
      const auto oldPosition = flight.position;
      const auto controls = island::autopilot(flight);
      island::step(flight, controls, dt);
      if (vve::math::length(vve::math::add(flight.position, vve::math::scale(oldPosition, -1.0F))) > 2.0F) {
         check(false, "autopilot_never_teleports");
         break;
      }
   }
   std::cout << "route_origin=" << flight.originIsland << " route_target=" << flight.targetIsland << " phase=\"" << island::phaseName(flight.phase)
             << "\" elapsed=" << flight.elapsed << " x=" << flight.position.x << " y=" << flight.position.y << " z=" << flight.position.z
             << " nav=" << flight.navigationStage << " deliveries=" << flight.deliveries << '\n';
   return flight;
}

/** @brief Verifies every named island, nonoverlapping coastline footprint and shared local airfield geometry. */
void archipelago() {
   std::set<std::string> names;
   std::set<float> xCoordinates, zCoordinates;
   bool centres = true, flat = true, separated = true, stable = true, bounded = true;
   double fingerprint{};
   // Check every scattered site and ensure cached coordinates stay independent of query ordering.
   for (std::size_t id = 0; id < island::islandCount; ++id) {
      const auto centre = island::islandCenter(id);
      centres = centres && island::nearestIsland(centre.x, centre.z) == id;
      stable = stable && same(centre, island::islandCenter(id));
      bounded = bounded && centre.x >= island::archipelagoMinimum && centre.x < island::archipelagoMinimum + island::archipelagoSpan
                        && -centre.z >= island::archipelagoMinimum && -centre.z < island::archipelagoMinimum + island::archipelagoSpan;
      xCoordinates.insert(centre.x);
      zCoordinates.insert(centre.z);
      fingerprint += (static_cast<double>(centre.x) - static_cast<double>(centre.z)) * static_cast<double>(id + 1);
      flat = flat && island::terrainHeight(centre.x, centre.z) == island::runwayHeight
                  && island::terrainHeight(centre.x + 60.0F, centre.z - 200.0F) == island::runwayHeight;
      names.insert(island::islandName(id));
      // Coastline radii are below 1000 metres; reject every pair closer than the generation contract.
      for (std::size_t other = 0; other < id; ++other) {
         const auto previous = island::islandCenter(other);
         separated = separated && std::hypot(static_cast<float>(centre.x - previous.x), static_cast<float>(centre.z - previous.z))
                                  >= island::islandMinimumSeparation - 0.001F;
      }
   }
   check(island::islandCount == 256 && centres, "all_256_island_centres_resolve");
   check(names.size() == island::islandCount && island::islandName(0) == "North Haven" && island::islandName(1) == "Port Azure",
         "every_island_has_a_unique_stable_name");
   check(flat && separated, "all_airfields_flat_and_islands_separated");
   check(stable && bounded && island::archipelagoSeed == 20261017U, "seeded_scatter_is_stable_and_within_world_bounds");
   check(xCoordinates.size() > 250 && zCoordinates.size() > 250, "random_islands_have_no_shared_grid_rows_or_columns");
   bool nearestSamples = true;
   std::mt19937 random{481U};
   // Compare arbitrary positions, including outside-world samples, against an independent double-distance ordering.
   for (int sample = 0; sample < 96; ++sample) {
      const float x = island::archipelagoMinimum - 5000.0F + static_cast<float>(random() % 54000U);
      const float z = -island::archipelagoMinimum + 5000.0F - static_cast<float>(random() % 54000U);
      const auto expected = std::ranges::min(std::views::iota(std::size_t{}, island::islandCount), {}, [&](std::size_t id) {
         const auto site = island::islandCenter(id);
         return std::hypot(static_cast<double>(site.x) - x, static_cast<double>(site.z) - z);
      });
      nearestSamples = nearestSamples && island::nearestIsland(x, z) == expected;
   }
   check(nearestSamples, "nearest_island_matches_scattered_sites_and_outside_samples");
   std::cout << "archipelago_seed=" << island::archipelagoSeed << " fingerprint=" << std::setprecision(15) << fingerprint << '\n';
   island::Flight far;
   const auto centre = island::islandCenter(island::islandCount - 1);
   far.position = {centre.x + 60.0F, island::aircraftGroundHeight, centre.z - 200.0F};
   island::step(far, {}, dt);
   check(far.grounded && far.phase != island::Phase::crashed, "far_island_local_apron_is_safe");
   check(island::terrainHeight(centre.x + 280.0F, centre.z) > 30.0F
         && island::terrainHeight(centre.x, centre.z + 1200.0F) == 0.0F, "far_island_has_hills_and_separated_water");
}

/** @brief Measures the spatial terrain lookup on a million vertices without imposing a machine-dependent time limit. */
void terrainSampling() {
   constexpr int side = 1024;
   const auto begin = std::chrono::steady_clock::now();
   double sum{};
   // A world-wide regular sample crosses spatial-bucket boundaries, scattered coastlines and open water.
   for (int row = 0; row < side; ++row) {
      for (int column = 0; column < side; ++column) {
         const float x = island::archipelagoMinimum + island::archipelagoSpan * static_cast<float>(column) / static_cast<float>(side - 1);
         const float z = -island::archipelagoMinimum - island::archipelagoSpan * static_cast<float>(row) / static_cast<float>(side - 1);
         sum += island::terrainHeight(x, z);
      }
   }
   const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
   check(std::isfinite(sum) && sum > 1000.0, "spatial_terrain_sampling_produces_finite_land");
   std::cout << "terrain_samples=" << side * side << " terrain_us=" << microseconds << " height_checksum=" << sum << '\n';
}

/** @brief Keeps active cargo immutable and requires valid stationary airport pickup for new jobs. */
void cargoSelection() {
   island::Flight flight;
   check(island::suggestedTarget(flight) == 1 && !island::cargoName(flight).empty(), "initial_job_has_destination_and_manifest");
   const auto initial = flight.position;
   check(!island::acceptCargo(flight, 0) && !island::acceptCargo(flight, island::islandCount), "cargo_rejects_current_and_invalid_islands");
   check(island::acceptCargo(flight, 17) && flight.targetIsland == 17 && same(initial, flight.position), "map_job_selection_never_moves_aircraft");
   flight.velocity.z = -2.0F;
   check(!island::acceptCargo(flight, 18), "moving_aircraft_cannot_load_cargo");
   flight.velocity = {};
   flight.grounded = false;
   flight.phase = island::Phase::flying;
   check(!island::acceptCargo(flight, 18), "airborne_aircraft_cannot_retarget");
   flight.grounded = true;
   flight.phase = island::Phase::landed;
   check(!island::acceptCargo(flight, 18) && flight.targetIsland == 17, "wrong_airport_landing_cannot_abandon_active_cargo");
   bool nearbyOffers = true;
   float longestOffer{};
   // Every airport offers a nearby destination, excluding the airport that supplied the preceding cargo.
   for (std::size_t current = 0; current < island::islandCount; ++current) {
      island::Flight offer;
      offer.position = island::islandCenter(current);
      offer.originIsland = (current + 1) % island::islandCount;
      offer.deliveries = static_cast<unsigned>(current % 7 + 1);
      const auto target = island::suggestedTarget(offer);
      const auto site = island::islandCenter(target);
      const float distance = std::hypot(static_cast<float>(site.x - offer.position.x), static_cast<float>(site.z - offer.position.z));
      longestOffer = std::max(longestOffer, distance);
      int closer{};
      for (std::size_t id = 0; id < island::islandCount; ++id) {
         if (id == current || id == offer.originIsland) { continue; }
         const auto other = island::islandCenter(id);
         if (std::hypot(static_cast<float>(other.x - offer.position.x), static_cast<float>(other.z - offer.position.z)) < distance) { ++closer; }
      }
      nearbyOffers = nearbyOffers && target < island::islandCount && target != current && target != offer.originIsland
                                 && closer < 3 && target == island::suggestedTarget(offer);
   }
   check(nearbyOffers, "cargo_offers_choose_nearest_three_and_avoid_previous_airport");
   std::cout << "longest_nearby_offer=" << longestOffer << '\n';
}

/** @brief Continues two jobs in one aircraft and verifies lateral and reverse aligned-runway approaches. */
void continuingRoutes(island::Flight first) {
   const auto stopped = first.position;
   const float elapsed = first.elapsed, heading = first.heading;
   const auto next = island::suggestedTarget(first);
   check(first.deliveries == 1 && next < island::islandCount && next != 0 && next != 1, "completed_job_offers_nearby_new_island");
   advance(first, {.brake = 1.0F}, 180);
   check(first.deliveries == 1, "completed_delivery_is_counted_once");
   check(island::acceptCargo(first, next) && first.originIsland == 1 && same(first.position, stopped)
         && first.elapsed == elapsed && first.heading == heading && first.takeoffs == 1 && first.landings == 1,
         "next_pickup_preserves_aircraft_position_heading_and_history");
   auto unflown = first;
   const auto nextCentre = island::islandCenter(next);
   unflown.position = {nextCentre.x, island::aircraftGroundHeight, nextCentre.z - 200.0F};
   unflown.phase = island::Phase::landed;
   advance(unflown, {.brake = 1.0F}, 180);
   check(unflown.phase != island::Phase::delivered && unflown.deliveries == 1, "new_cargo_requires_new_takeoff_and_landing");
   first = mission(first);
   check(first.phase == island::Phase::delivered && first.deliveries == 2 && first.takeoffs == 2 && first.landings == 2,
         "two_deliveries_fly_same_aircraft_without_reset");
   check(island::acceptCargo(first, 1), "reverse_route_cargo_accepted_at_current_airport");
   const auto reverse = mission(first);
   check(reverse.phase == island::Phase::delivered && reverse.deliveries == 3 && reverse.takeoffs == 3 && reverse.landings == 3,
         "reverse_route_flies_northern_fix_and_aligned_landing");
   island::Flight lateral;
   check(island::acceptCargo(lateral, 17), "noncollinear_route_selected");
   lateral = mission(lateral);
   const auto target = island::islandCenter(17);
   check(lateral.phase == island::Phase::delivered && lateral.deliveries == 1 && std::abs(lateral.position.x - target.x) < 25.0F,
         "noncollinear_route_delivers_at_selected_island");
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

/** @brief Turns through 180 degrees using actual nose-wheel physics while staying on the 36-metre airstrip. */
void runwayTurnaround() {
   island::Flight flight;
   flight.position.z = -160.0F;
   flight.velocity.z = -3.5F;
   flight.throttle = 0.12F;
   bool onStrip = true;
   // Mild power maintains walking-speed taxiing; the pilot stops steering when the nose points back up the strip.
   for (int frame = 0; frame < 600 && std::abs(flight.heading) < 3.11F; ++frame) {
      island::step(flight, {.throttle = 0.12F, .yaw = 1.0F}, dt);
      onStrip = onStrip && flight.grounded && std::abs(flight.position.x) <= island::runwayHalfWidth
                         && std::abs(flight.position.z) <= island::runwayHalfLength;
   }
   check(onStrip && flight.phase != island::Phase::crashed && std::abs(flight.heading) > 3.11F
         && island::forward(flight).z > 0.99F && flight.velocity.z > 3.0F, "walking_speed_runway_uturn_stays_on_strip");
   advance(flight, {.brake = 1.0F}, 120);
   check(flight.grounded && vve::math::length(flight.velocity) < 0.01F && std::abs(flight.position.x) < island::runwayHalfWidth,
         "reversed_aircraft_brakes_to_stop_on_strip");
   const auto before = flight.position;
   const float reversedHeading = flight.heading;
   check(island::acceptCargo(flight, 1) && same(flight.position, before) && flight.heading == reversedHeading,
         "cargo_acceptance_preserves_pilot_runway_turnaround");
   bool aligned = false, safe = true, limitedPower = true;
   // The departure pilot must steer toward the centre and align before applying takeoff power.
   for (int frame = 0; frame < 1200 && flight.grounded; ++frame) {
      const auto controls = island::autopilot(flight);
      if (std::abs(flight.heading) > 0.15F) { limitedPower = limitedPower && controls.throttle <= 0.7F && controls.pitch == 0.0F; }
      island::step(flight, controls, dt);
      aligned = aligned || std::abs(flight.heading) < 0.1F;
      safe = safe && flight.phase != island::Phase::crashed && std::abs(flight.position.x) <= island::runwayHalfWidth;
   }
   check(limitedPower && aligned && safe && !flight.grounded && flight.takeoffs == 1 && flight.deliveries == 0,
         "autopilot_aligns_reversed_aircraft_before_safe_departure");
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
   archipelago();
   terrainSampling();
   cargoSelection();
   geometryAndStartup();
   aircraftControls();
   deliveryRules();
   landingSafety();
   taxiPavement();
   runwayTurnaround();
   repeatability();
   const auto flight = mission();
   check(flight.phase == island::Phase::delivered && flight.takeoffs == 1 && flight.landings == 1, "complete_delivery_through_real_controls");
   check(flight.elapsed < 120.0F && flight.maxAltitude > 40.0F && flight.maxAltitude < 120.0F, "mission_duration_and_altitude_budget");
   check(flight.position.z >= -2700.0F && flight.position.z <= -2530.0F && vve::math::length(flight.velocity) < 0.8F,
         "completed_delivery_stopped_inside_unloading_zone");
   const auto repeat = mission();
   check(same(flight.position, repeat.position) && flight.elapsed == repeat.elapsed, "autonomous_delivery_repeats_exactly");
   continuingRoutes(flight);
   std::cout << "mission_phase=\"" << island::phaseName(flight.phase) << "\" elapsed=" << flight.elapsed
             << " x=" << flight.position.x << " y=" << flight.position.y << " z=" << flight.position.z
             << " max_altitude=" << flight.maxAltitude << " touchdown_sink=" << flight.touchdownSpeed
             << " takeoffs=" << flight.takeoffs << " landings=" << flight.landings << " failures=" << failures << '\n';
   return failures == 0 ? 0 : 1;
}
