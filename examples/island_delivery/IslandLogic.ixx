/** @file @brief Deterministic aircraft handling and landing deliveries, using only VVE facade math. */
export module Island.Logic;
import std;
export import VVEngine;

export namespace island {

inline constexpr float runwayHeight = 4.0F;       ///< Height of both paved island airstrips in metres.
inline constexpr float aircraftGroundHeight = 5.25F; ///< Aircraft origin with its wheels on an airstrip.
inline constexpr float destinationZ = -2400.0F;  ///< Centre of the destination airstrip in metres.
inline constexpr float runwayHalfLength = 350.0F; ///< Paved half-length of each airstrip.
inline constexpr float runwayHalfWidth = 18.0F;   ///< Paved half-width of each airstrip.
inline constexpr std::size_t islandCount = 256;  ///< Independently named delivery destinations in the archipelago.
inline constexpr std::size_t gridSide = 16;      ///< Island sites per grid column and row.
inline constexpr float islandSpacing = 2400.0F;  ///< Centre spacing, keeping coastlines separated by navigable water.

/** @brief Returns a stable island centre; IDs zero and one retain the original north-to-south delivery route. */
[[nodiscard]] vve::Vec3 islandCenter(std::size_t id) {
   return {static_cast<float>(id / gridSide) * islandSpacing, 0.0F, -static_cast<float>(id % gridSide) * islandSpacing};
}

/** @brief Names every destination uniquely while preserving the two original airfields. */
[[nodiscard]] std::string islandName(std::size_t id) {
   if (id == 0) { return "North Haven"; }
   if (id == 1) { return "Port Azure"; }
   constexpr std::array first{"Coral", "Amber", "Cobalt", "Jade", "Silver", "Pearl", "Copper", "Indigo",
                              "Emerald", "Golden", "Ruby", "Opal", "Ivory", "Violet", "Crimson", "Sapphire"};
   constexpr std::array second{"Key", "Bay", "Cove", "Harbor", "Point", "Reach", "Landing", "Atoll",
                               "Cape", "Shoal", "Haven", "Lagoon", "Sound", "Reef", "Sands", "Isle"};
   return std::string{first[(id / gridSide) % gridSide]} + " " + second[id % gridSide];
}

/** @brief Locates the closest grid site in constant time, including positions outside the archipelago. */
[[nodiscard]] std::size_t nearestIsland(float x, float z) {
   const auto column = static_cast<std::size_t>(std::lround(std::clamp(x / islandSpacing, 0.0F, static_cast<float>(gridSide - 1))));
   const auto row = static_cast<std::size_t>(std::lround(std::clamp(-z / islandSpacing, 0.0F, static_cast<float>(gridSide - 1))));
   return column * gridSide + row;
}

/** @brief Delivery progress; a stopped landing at the destination is required to unload. */
enum class Phase {
   ready,      ///< Cargo aboard, awaiting departure.
   flying,     ///< Airborne between airstrips.
   landed,     ///< Safely grounded after flight.
   delivered,  ///< Cargo unloaded after two stationary seconds.
   crashed     ///< Unsafe terrain, water or a hard touchdown ended this flight.
};

/** @brief ECS flight state; no renderer, audio or hidden simulation state is required. */
struct Flight {
   vve::Vec3 position{0.0F, aircraftGroundHeight, 240.0F}; ///< Aircraft origin in world metres.
   vve::Vec3 velocity{}; ///< World velocity in metres per second.
   float heading{};     ///< Radians clockwise from the runway's negative Z axis.
   float pitch{};       ///< Positive nose-up attitude in radians.
   float bank{};        ///< Positive right-bank attitude in radians.
   float throttle{};    ///< Smoothed engine power from zero to one.
   float elapsed{};     ///< Simulated mission duration in seconds.
   float stoppedSeconds{}; ///< Continuous stationary time inside the unloading area.
   float maxAltitude{}; ///< Highest aircraft origin reached, in metres above sea level.
   Phase phase{Phase::ready}; ///< Current mission phase.
   bool grounded{true}; ///< Wheels have safe contact with an airstrip.
   int takeoffs{};      ///< Actual ground-to-air transitions.
   int landings{};      ///< Safe air-to-ground transitions.
   float touchdownSpeed{}; ///< Downward speed at the most recent ground contact.
   std::size_t originIsland{}; ///< Island where this leg's cargo was collected.
   std::size_t targetIsland{1}; ///< Selected cargo destination.
   unsigned deliveries{}; ///< Completed unloading operations across all accepted jobs.
   int cargoTakeoffs{}; ///< Takeoff count at cargo acceptance, preventing delivery without a new flight.
   unsigned navigationStage{}; ///< Autopilot departure, northern fix, runway alignment, then final approach.
};

/** @brief Normalized pilot commands; positive pitch lifts the nose, roll and yaw turn right. */
struct Controls {
   float throttle{}; ///< Requested engine power in [0,1].
   float pitch{};    ///< Requested nose attitude in [-1,1].
   float roll{};     ///< Requested bank attitude in [-1,1].
   float yaw{};      ///< Rudder and ground steering in [-1,1].
   float brake{};    ///< Wheel brake pressure in [0,1].
};

/** @brief Returns the smooth island surface, with a flat central runway and adjoining apron. */
[[nodiscard]] float terrainHeight(float x, float z) {
      const auto id = nearestIsland(x, z);
      const auto centre = islandCenter(id);
      x -= centre.x;
      const float dz = z - centre.z;
      const float variation = id < 2 ? 1.0F : 0.85F + static_cast<float>((id * 37) % 31) * 0.01F;
      // Islands do not overlap, so only the nearest site's bounded surface needs evaluation.
      const float angle = std::atan2(dz / 750.0F, x / 650.0F);
      const float radius = std::sqrt(x * x / (650.0F * 650.0F) + dz * dz / (750.0F * 750.0F))
                         / (variation * (1.0F + 0.07F * std::sin(5.0F * angle) + 0.04F * std::cos(9.0F * angle)));
      if (radius >= 1.0F) { return 0.0F; }
      const float coast = std::clamp((1.0F - radius) * 5.0F, 0.0F, 1.0F);
      const float hills = 55.0F * variation * std::exp(-((x - 280.0F) * (x - 280.0F) / 65000.0F + dz * dz / 210000.0F));
      const float runwayBlend = std::clamp((std::abs(x) - 90.0F) / 90.0F, 0.0F, 1.0F);
      const float endBlend = std::clamp((std::abs(dz) - 420.0F) / 160.0F, 0.0F, 1.0F);
      return coast * (runwayHeight + hills * std::max(runwayBlend, endBlend));
}

/** @brief Returns the aircraft's unit nose direction, including its pitch attitude. */
[[nodiscard]] vve::Vec3 forward(const Flight &flight) {
   return {std::sin(flight.heading) * std::cos(flight.pitch), std::sin(flight.pitch),
           -std::cos(flight.heading) * std::cos(flight.pitch)};
}

/** @brief Returns horizontal distance to the unloading-area centre in metres. */
[[nodiscard]] float remainingDistance(const Flight &flight) {
   const auto centre = islandCenter(flight.targetIsland);
   return std::hypot(static_cast<float>(flight.position.x - centre.x), static_cast<float>(flight.position.z - centre.z) + 200.0F);
}

/** @brief Returns the current deterministic manifest label for map and cockpit displays. */
[[nodiscard]] std::string_view cargoName(const Flight &flight) {
   constexpr std::array<std::string_view, 6> names{"Medical supplies", "Fresh produce", "Aircraft spares", "Island mail", "Research equipment", "Food parcels"};
   return names[(flight.originIsland * 7 + flight.targetIsland + flight.deliveries) % names.size()];
}

/** @brief Suggests the following island while leaving destination selection to the player. */
[[nodiscard]] std::size_t suggestedTarget(const Flight &flight) {
   return (nearestIsland(flight.position.x, flight.position.z) + 1) % islandCount;
}

/** @brief Loads a chosen island's cargo only while safely stopped, preserving the aircraft and cumulative mission state. */
[[nodiscard]] bool acceptCargo(Flight &flight, std::size_t target) {
   const auto current = nearestIsland(flight.position.x, flight.position.z);
   if (target >= islandCount || target == current || !flight.grounded || vve::math::length(flight.velocity) >= 0.8F
       || (flight.phase != Phase::ready && flight.phase != Phase::delivered)) { return false; }
   flight.originIsland = current;
   flight.targetIsland = target;
   flight.phase = Phase::ready;
   flight.stoppedSeconds = 0.0F;
   flight.cargoTakeoffs = flight.takeoffs;
   flight.navigationStage = 0;
   return true;
}

/** @brief Human-readable phase labels for the HUD and deterministic test telemetry. */
[[nodiscard]] std::string_view phaseName(Phase phase) {
   constexpr std::array names{"Ready for departure", "Cargo in flight", "Taxi to unloading", "Delivery complete", "Aircraft lost"};
   return names[static_cast<std::size_t>(phase)];
}

/** @brief Advances rate-limited arcade flight, wheel contact and delivery using bounded time steps. */
void step(Flight &flight, Controls controls, float dt) {
   if (!std::isfinite(dt) || dt <= 0.0F || flight.phase == Phase::crashed || flight.phase == Phase::delivered) { return; }
   const auto command = [](float value, float low, float high) {
      return std::isfinite(value) ? std::clamp(value, low, high) : 0.0F;
   };
   controls = {command(controls.throttle, 0.0F, 1.0F), command(controls.pitch, -1.0F, 1.0F),
               command(controls.roll, -1.0F, 1.0F), command(controls.yaw, -1.0F, 1.0F), command(controls.brake, 0.0F, 1.0F)};
   dt = std::min(dt, 0.1F);
   const int subdivisions = static_cast<int>(std::ceil(dt * 120.0F));
   const float tick = dt / static_cast<float>(subdivisions);
   // Small physics steps prevent a long render frame from tunnelling through the island surface.
   for (int index = 0; index < subdivisions; ++index) {
      flight.elapsed += tick;
      flight.throttle += std::clamp(controls.throttle - flight.throttle, -0.7F * tick, 0.7F * tick);
      flight.pitch += (controls.pitch * 0.32F - flight.pitch) * std::min(1.0F, 2.5F * tick);
      flight.bank += (controls.roll * 0.65F - flight.bank) * std::min(1.0F, 2.0F * tick);
      float speed = std::hypot(static_cast<float>(flight.velocity.x), static_cast<float>(flight.velocity.z));
      const float resistance = 0.15F + 0.002F * speed * speed + (flight.grounded ? 0.45F + controls.brake * 9.0F : 0.0F);
      speed = std::max(0.0F, speed + (flight.throttle * 6.0F - resistance) * tick);
      const float turning = flight.grounded ? controls.yaw * std::min(speed / 5.0F, 1.0F) * 0.9F
                                          : flight.bank * 0.38F + controls.yaw * 0.16F;
      flight.heading = std::remainder(flight.heading + turning * tick, 2.0F * std::numbers::pi_v<float>);
      flight.velocity.x = std::sin(flight.heading) * speed;
      flight.velocity.z = -std::cos(flight.heading) * speed;
      // Rotation requires airspeed and an intentional nose-up command.
      if (flight.grounded && speed >= 27.0F && flight.pitch > 0.075F && controls.brake < 0.1F) {
         flight.grounded = false;
         flight.phase = Phase::flying;
         ++flight.takeoffs;
         flight.velocity.y = 1.0F;
      }
      if (!flight.grounded) {
         const float climb = std::sin(flight.pitch) * speed - std::max(0.0F, 27.0F - speed) * 0.65F;
         flight.velocity.y += (climb - flight.velocity.y) * std::min(1.0F, 1.6F * tick);
      }
      flight.position = vve::math::add(flight.position, vve::math::scale(flight.velocity, tick));
      const float floor = terrainHeight(flight.position.x, flight.position.z) + (aircraftGroundHeight - runwayHeight);
      const auto current = nearestIsland(flight.position.x, flight.position.z);
      const auto centre = islandCenter(current);
      const float x = flight.position.x - centre.x, z = flight.position.z - centre.z;
      const bool onRunway = std::abs(x) <= runwayHalfWidth && std::abs(z) <= runwayHalfLength;
      // Every airfield provides the same connected runway, taxiway, apron and unloading pavement.
      const bool onPavement = onRunway || (x >= 25.0F && x <= 77.0F && z >= -265.0F && z <= -135.0F)
                             || (x >= 17.0F && x <= 47.0F && z >= -240.0F && z <= -213.0F)
                             || (std::abs(x) <= 25.0F && z >= -300.0F && z <= -130.0F);
      // Touchdown requires the runway; grounded taxiing may also use the connected paved surfaces.
      if (flight.grounded || flight.position.y <= floor) {
         const bool touchdown = !flight.grounded;
         flight.touchdownSpeed = touchdown ? std::max(0.0F, -static_cast<float>(flight.velocity.y)) : flight.touchdownSpeed;
         if (!onPavement || (touchdown && (!onRunway || flight.touchdownSpeed > 4.0F || speed > 47.0F || std::abs(flight.bank) > 0.25F
                                      || std::abs(std::sin(flight.heading)) > 0.3F))) {
            flight.phase = Phase::crashed;
            flight.grounded = false;
            flight.stoppedSeconds = 0.0F;
            return;
         }
         flight.position.y = aircraftGroundHeight;
         flight.velocity.y = 0.0F;
         flight.grounded = true;
         if (touchdown) { flight.phase = Phase::landed; ++flight.landings; }
      }
      flight.maxAltitude = std::max(flight.maxAltitude, static_cast<float>(flight.position.y));
      const bool unloading = flight.grounded && flight.phase == Phase::landed && flight.takeoffs > flight.cargoTakeoffs && flight.landings > 0
                          && current == flight.targetIsland && std::abs(x) <= 25.0F && z >= -300.0F && z <= -130.0F
                          && speed < 0.8F;
      flight.stoppedSeconds = unloading ? flight.stoppedSeconds + tick : 0.0F;
      if (flight.stoppedSeconds >= 2.0F) { flight.phase = Phase::delivered; ++flight.deliveries; return; }
      // Navigation records only waypoint progress; it never changes aircraft position or flight dynamics.
      if (!flight.grounded) {
         const auto target = islandCenter(flight.targetIsland);
         const float targetX = flight.position.x - target.x, targetZ = flight.position.z - target.z;
         const auto origin = islandCenter(flight.originIsland);
         if (flight.navigationStage == 0 && flight.position.y > 45.0F && flight.position.z - origin.z < -400.0F) {
            flight.navigationStage = std::abs(targetX) < 20.0F && targetZ > 1200.0F && std::abs(flight.heading) < 0.1F ? 3U : 1U;
         }
         if (flight.navigationStage == 1 && std::hypot(targetX, targetZ - 2400.0F) < 250.0F) { flight.navigationStage = 2; }
         if (flight.navigationStage == 2) {
            if (std::abs(targetX) < 20.0F && std::abs(flight.heading) < 0.12F && targetZ > 1200.0F) { flight.navigationStage = 3; }
            else if (targetZ < 1000.0F) { flight.navigationStage = 1; }
         }
      }
   }
}

/** @brief Flies the real control path for demonstrations and automated mission verification; never moves the aircraft. */
[[nodiscard]] Controls autopilot(const Flight &flight) {
   const float speed = std::hypot(static_cast<float>(flight.velocity.x), static_cast<float>(flight.velocity.z));
   const auto target = islandCenter(flight.targetIsland);
   const auto origin = islandCenter(flight.originIsland);
   const float x = flight.position.x - target.x, z = flight.position.z - target.z;
   const bool final = flight.navigationStage == 3;
   float desiredHeading = -std::atan2(x, 250.0F);
   if (flight.navigationStage == 0) { desiredHeading = -std::atan2(static_cast<float>(flight.position.x - origin.x), 250.0F); }
   if (flight.navigationStage == 1) { desiredHeading = std::atan2(-x, z - 2400.0F); }
   Controls controls{};
   const float headingError = std::remainder(desiredHeading - flight.heading,
                                             2.0F * std::numbers::pi_v<float>);
   controls.yaw = std::clamp(headingError * 2.0F, -1.0F, 1.0F);
   // The destination ground roll proceeds to the unloading zone before applying full brakes.
   if (flight.grounded && flight.phase == Phase::landed && nearestIsland(flight.position.x, flight.position.z) == flight.targetIsland) {
      const float targetSpeed = z > -150.0F ? 12.0F : 0.0F;
      controls.throttle = std::clamp((targetSpeed - speed) * 0.25F, 0.0F, 0.65F);
      controls.brake = std::clamp((speed - targetSpeed) * 0.4F, 0.0F, 1.0F);
      return controls;
   }
   if (flight.grounded) {
      // Nose-wheel steering turns slowly toward the runway centre before allowing departure power.
      if (std::abs(headingError) > 0.1F) {
         const float runwayX = flight.position.x - origin.x;
         float steeringError = headingError;
         if (std::abs(flight.heading) > 2.6F && std::abs(runwayX) > 6.0F) {
            steeringError = std::copysign(std::abs(headingError), runwayX);
         }
         controls.yaw = std::clamp(steeringError * 3.0F, -1.0F, 1.0F);
         constexpr float taxiSpeed = 3.5F;
         controls.throttle = std::clamp((0.6F + 0.002F * taxiSpeed * taxiSpeed) / 6.0F + (taxiSpeed - speed) * 0.2F, 0.0F, 0.7F);
         controls.brake = std::clamp((speed - taxiSpeed) * 0.5F, 0.0F, 1.0F);
         return controls;
      }
      controls.throttle = 1.0F;
      controls.pitch = 0.65F;
      return controls;
   }
   const float targetSpeed = final && z < 1100.0F ? 34.0F : 50.0F;
   controls.throttle = std::clamp((0.15F + 0.002F * targetSpeed * targetSpeed) / 6.0F + (targetSpeed - speed) * 0.13F, 0.0F, 1.0F);
   const float glideHeight = aircraftGroundHeight + (z - 200.0F) * 0.08F;
   const float targetHeight = final ? std::min(80.0F, glideHeight) : 80.0F;
   const float descent = final && glideHeight < 80.0F ? -0.08F * speed : 0.0F;
   const float climb = std::clamp((targetHeight - static_cast<float>(flight.position.y)) * 0.8F + descent, -3.2F, 8.0F);
   controls.pitch = std::clamp(std::asin(std::clamp(climb / std::max(speed, 1.0F), -0.9F, 0.9F)) / 0.32F, -1.0F, 1.0F);
   controls.roll = std::clamp(headingError * 1.7F - flight.bank, -0.9F, 0.9F);
   return controls;
}

} // namespace island
