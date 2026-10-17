/** @file @brief Recorded aircraft, coastal ambience and landing foley through VVE's audio facade. */
export module Island.Sound;
import std;
import Island.Logic;

export namespace island {

/** @brief Engine-owned recordings and frame-driven mixing; gains and pitches expose deterministic telemetry. */
struct Soundscape {
   vve::AudioSystem *audio{}; ///< Non-owning device; null when muted or no output device is available.
   float volume{0.65F}; ///< User-selected master gain, from zero to one.
   unsigned loadedSounds{}; ///< Successfully loaded distinct recordings for verification.
   bool deviceUnavailable{}; ///< Device initialization failed; flight remains playable without sound.
   std::array<vve::SoundHandle, 8> sounds{}; ///< Propeller, cabin, wind, coast, wheels, touchdown, crash and cargo.
   std::array<vve::AudioPlaybackHandle, 6> loops{}; ///< Low/high engine, cabin, airflow, coast and wheel layers.
   std::array<vve::AudioPlaybackHandle, 3> effects{}; ///< Latest touchdown, crash and unloading one-shots.
   std::array<float, 6> gains{}; ///< Last unattenuated loop gains, independent of audio hardware.
   std::array<float, 6> pitches{}; ///< Last playback speed ratios, independent of audio hardware.
   vve::Vec3 listenerPosition{}; ///< Previous camera position for listener Doppler velocity.
   float lastElapsed{}; ///< Previous simulated time; pause and restarts never invent listener motion.
   int lastLandings{}; ///< Previous safe contact count, preventing repeated touchdown sounds.
   Phase lastPhase{Phase::ready}; ///< Previous mission phase for one-shot transitions.
   bool sampled{}; ///< At least one flight frame has established transition history.
   bool wasPaused{}; ///< Playback cursors stay fixed while the simulation is paused.

   auto init(vve::AudioSystem &system, std::filesystem::path root, bool muted) -> std::expected<void, vve::Error>;
   auto update(const Flight &flight, vve::Camera camera, bool cockpit, bool paused) -> std::expected<void, vve::Error>;
};

/** @brief Loads configure-time downloads; absent devices are optional, corrupt or missing recordings are errors. */
auto Soundscape::init(vve::AudioSystem &system, std::filesystem::path root, bool muted)
   -> std::expected<void, vve::Error> {
   if (audio || muted) { return {}; }
   if (!system.init()) { deviceUnavailable = true; return {}; }
   constexpr std::array files{"propeller.ogg", "cockpit.mp3", "wind.mp3", "coast.mp3", "rolling.mp3",
                              "touchdown.wav", "crash.ogg", "unload.mp3"};
   // Keep every recording independently addressable; no synthesis or external audio API runs in the game.
   for (const auto index : std::views::iota(std::size_t{}, sounds.size())) {
      const auto sound = system.loadSound(root / "sounds" / files[index], vve::AudioLoadMode::decoded);
      if (!sound) {
         std::cerr << "island_audio_load_failed path=" << (root / "sounds" / files[index])
                   << " error=" << system.lastError() << '\n';
         // Release a partial catalog so a caller can repair the path and retry initialization.
         for (const auto loaded : sounds) { if (loaded.valid()) { (void)system.unloadSound(loaded); } }
         sounds = {}; loadedSounds = 0;
         return std::unexpected(sound.error());
      }
      sounds[index] = *sound;
      ++loadedSounds;
   }
   constexpr std::array<std::size_t, 6> layers{0, 0, 1, 2, 3, 4};
   // Start silent loops together; the first frame sets their power, viewpoint and airspeed mix.
   for (const auto index : std::views::iota(std::size_t{}, loops.size())) {
      const auto voice = system.play(sounds[layers[index]], {.loop = true, .volume = {.value = 0.0F}});
      if (!voice) {
         // Unloading also stops partially started loops, leaving initialization safely retryable.
         for (const auto sound : sounds) { (void)system.unloadSound(sound); }
         sounds = {}; loops = {}; loadedSounds = 0;
         return std::unexpected(voice.error());
      }
      loops[index] = *voice;
   }
   audio = &system; deviceUnavailable = false;
   return {};
}

/** @brief Crossfades RPM layers, moves positional sources, and triggers contact/cargo sounds exactly once. */
auto Soundscape::update(const Flight &flight, vve::Camera camera, bool cockpit, bool paused)
   -> std::expected<void, vve::Error> {
   const float speed = static_cast<float>(vve::math::length(flight.velocity));
   const float power = std::clamp(flight.throttle, 0.0F, 1.0F);
   const float blend = std::clamp((power - 0.18F) / 0.72F, 0.0F, 1.0F);
   const float engine = flight.phase == Phase::crashed ? 0.0F : 0.40F + 0.32F * power;
   const float exterior = cockpit ? 0.10F : 1.0F;
   const float airflow = std::clamp(speed / 65.0F, 0.0F, 1.0F);
   const float coast = 0.20F / (1.0F + std::max(0.0F, static_cast<float>(camera.position.value.y)) / 80.0F);
   gains = {engine * std::sqrt(1.0F - blend) * exterior, engine * std::sqrt(blend) * exterior,
            cockpit ? engine * 0.85F : 0.0F, airflow * airflow * (cockpit ? 0.10F : 0.32F),
            coast * (cockpit ? 0.25F : 1.0F), flight.grounded ? airflow * 0.42F : 0.0F};
   pitches = {0.72F + 0.42F * power, 0.96F + 0.38F * power, 0.85F + 0.28F * power,
              0.90F + 0.25F * airflow, 1.0F, 0.65F + 0.85F * airflow};
   if (paused) { gains.fill(0.0F); }

   // Telemetry remains available on machines without speakers and in the explicit muted mode.
   if (audio) {
      if (const auto result = audio->setMasterVolume({.value = std::clamp(volume, 0.0F, 1.0F)}); !result) { return result; }
      const float dt = flight.elapsed - lastElapsed;
      const auto velocity = sampled && !paused && dt > 0.0F && dt < 0.5F
         ? vve::math::scale(vve::math::subtract(camera.position.value, listenerPosition), 1.0F / dt) : vve::zeroVec3();
      if (const auto result = audio->setListener({.position = camera.position, .forward = camera.forward,
                                                .velocity = velocity}); !result) { return result; }
      // Propeller and wheels follow the aircraft; the cabin and airflow retain their enclosed, listener-relative mix.
      const auto nose = vve::math::add(flight.position, vve::math::scale(forward(flight), 2.2F));
      const vve::AudioSource aircraft{.position = {.value = nose}, .velocity = flight.velocity,
                                      .reference_distance = 18.0F};
      const float nearestIsland = static_cast<float>(flight.position.z) < destinationZ * 0.5F ? destinationZ : 0.0F;
      const vve::AudioSource shoreline{.position = {.value = {580.0F, 0.0F, nearestIsland}},
                                       .reference_distance = 500.0F};
      for (const auto index : std::views::iota(std::size_t{}, loops.size())) {
         if (const auto result = audio->setVolume(loops[index], {.value = gains[index]}); !result) { return result; }
         if (const auto result = audio->setPitch(loops[index], {.value = pitches[index]}); !result) { return result; }
         if (index == 0 || index == 1 || index == 5) {
            if (const auto result = audio->setSource(loops[index], aircraft); !result) { return result; }
         }
         if (index == 4) {
            if (const auto result = audio->setSource(loops[index], shoreline); !result) { return result; }
         }
         if (paused != wasPaused) {
            const auto result = paused ? audio->pause(loops[index]) : audio->resume(loops[index]);
            if (!result) { return result; }
         }
      }
      // Finished one-shots may already have been collected by VVE; only operate on active or paused voices.
      for (const auto effect : effects) {
         if (paused && audio->isPlaying(effect)) {
            if (const auto result = audio->pause(effect); !result) { return result; }
         } else if (!paused && audio->isPaused(effect)) {
            if (const auto result = audio->resume(effect); !result) { return result; }
         }
      }
      const bool continuing = sampled && flight.elapsed >= lastElapsed && !paused;
      const std::array events{continuing && flight.landings > lastLandings,
                             continuing && flight.phase == Phase::crashed && lastPhase != Phase::crashed,
                             continuing && flight.phase == Phase::delivered && lastPhase != Phase::delivered};
      // Contact strength affects the landing thump; cargo uses recorded wooden-crate movement.
      const std::array effectGains{std::clamp(0.25F + flight.touchdownSpeed * 0.10F, 0.25F, 0.85F), 0.85F, 0.70F};
      for (const auto index : std::views::iota(std::size_t{}, effects.size())) {
         if (!events[index]) { continue; }
         const auto voice = audio->play(sounds[index + 5], {.volume = {.value = effectGains[index]}});
         if (!voice) { return std::unexpected(voice.error()); }
         effects[index] = *voice;
         if (const auto result = audio->setSource(*voice, {.position = {.value = flight.position},
                                                          .reference_distance = 20.0F}); !result) { return result; }
      }
   }
   listenerPosition = camera.position.value;
   lastElapsed = flight.elapsed; lastLandings = flight.landings; lastPhase = flight.phase;
   sampled = true; wasPaused = paused;
   return {};
}

} // namespace island
