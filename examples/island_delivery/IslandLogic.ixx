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
   float height{};
   // Combine two coastlines, leaving their intervening channel at sea level.
   for (const float centre : {0.0F, destinationZ}) {
      const float dz = z - centre;
      const float angle = std::atan2(dz / 750.0F, x / 650.0F);
      const float radius = std::sqrt(x * x / (650.0F * 650.0F) + dz * dz / (750.0F * 750.0F))
                         / (1.0F + 0.07F * std::sin(5.0F * angle) + 0.04F * std::cos(9.0F * angle));
      const float coast = std::clamp((1.0F - radius) * 5.0F, 0.0F, 1.0F);
      const float hills = 55.0F * std::exp(-((x - 280.0F) * (x - 280.0F) / 65000.0F + dz * dz / 210000.0F));
      const float runwayBlend = std::clamp((std::abs(x) - 90.0F) / 90.0F, 0.0F, 1.0F);
      const float endBlend = std::clamp((std::abs(dz) - 420.0F) / 160.0F, 0.0F, 1.0F);
      height = std::max(height, coast * (runwayHeight + hills * std::max(runwayBlend, endBlend)));
   }
   return height;
}

/** @brief Returns the aircraft's unit nose direction, including its pitch attitude. */
[[nodiscard]] vve::Vec3 forward(const Flight &flight) {
   return {std::sin(flight.heading) * std::cos(flight.pitch), std::sin(flight.pitch),
           -std::cos(flight.heading) * std::cos(flight.pitch)};
}

/** @brief Returns horizontal distance to the unloading-area centre in metres. */
[[nodiscard]] float remainingDistance(const Flight &flight) {
   return std::hypot(static_cast<float>(flight.position.x), static_cast<float>(flight.position.z) - (destinationZ - 200.0F));
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
      const float turning = flight.grounded ? controls.yaw * std::min(speed / 12.0F, 1.0F) * 0.35F
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
      const bool onRunway = std::abs(flight.position.x) <= runwayHalfWidth
                         && std::min(std::abs(flight.position.z), std::abs(flight.position.z - destinationZ)) <= runwayHalfLength;
      bool onPavement = onRunway;
      // Ground steering can use the actual taxiway and side apron at either airfield.
      for (const float centre : {0.0F, destinationZ}) {
         const float dz = flight.position.z - centre;
         onPavement = onPavement || (flight.position.x >= 25.0F && flight.position.x <= 77.0F && dz >= -265.0F && dz <= -135.0F)
                                || (flight.position.x >= 17.0F && flight.position.x <= 47.0F && dz >= -240.0F && dz <= -213.0F)
                                || (centre == destinationZ && std::abs(flight.position.x) <= 25.0F && dz >= -300.0F && dz <= -130.0F);
      }
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
      const bool unloading = flight.grounded && flight.phase == Phase::landed && flight.takeoffs > 0 && flight.landings > 0
                          && std::abs(flight.position.x) <= 25.0F && flight.position.z >= -2700.0F && flight.position.z <= -2530.0F
                          && speed < 0.8F;
      flight.stoppedSeconds = unloading ? flight.stoppedSeconds + tick : 0.0F;
      if (flight.stoppedSeconds >= 2.0F) { flight.phase = Phase::delivered; return; }
   }
}

/** @brief Flies the real control path for demonstrations and automated mission verification; never moves the aircraft. */
[[nodiscard]] Controls autopilot(const Flight &flight) {
   const float speed = std::hypot(static_cast<float>(flight.velocity.x), static_cast<float>(flight.velocity.z));
   Controls controls{};
   const float headingError = std::remainder(-std::atan2(static_cast<float>(flight.position.x), 250.0F) - flight.heading,
                                             2.0F * std::numbers::pi_v<float>);
   controls.yaw = std::clamp(headingError * 2.0F, -1.0F, 1.0F);
   // The destination ground roll proceeds to the unloading zone before applying full brakes.
   if (flight.grounded && flight.position.z < -1500.0F) {
      const float targetSpeed = flight.position.z > -2550.0F ? 12.0F : 0.0F;
      controls.throttle = std::clamp((targetSpeed - speed) * 0.25F, 0.0F, 0.65F);
      controls.brake = std::clamp((speed - targetSpeed) * 0.4F, 0.0F, 1.0F);
      return controls;
   }
   if (flight.grounded) { controls.throttle = 1.0F; controls.pitch = 0.65F; return controls; }
   const float targetSpeed = flight.position.z < -1300.0F ? 34.0F : 50.0F;
   controls.throttle = std::clamp((0.15F + 0.002F * targetSpeed * targetSpeed) / 6.0F + (targetSpeed - speed) * 0.13F, 0.0F, 1.0F);
   const float glideHeight = aircraftGroundHeight + (static_cast<float>(flight.position.z) + 2200.0F) * 0.08F;
   const float targetHeight = std::min(80.0F, glideHeight);
   const float descent = glideHeight < 80.0F ? -0.08F * speed : 0.0F;
   const float climb = std::clamp((targetHeight - static_cast<float>(flight.position.y)) * 0.8F + descent, -3.2F, 8.0F);
   controls.pitch = std::clamp(std::asin(std::clamp(climb / std::max(speed, 1.0F), -0.9F, 0.9F)) / 0.32F, -1.0F, 1.0F);
   controls.roll = std::clamp(headingError * 1.5F - flight.bank, -0.5F, 0.5F);
   return controls;
}

} // namespace island
