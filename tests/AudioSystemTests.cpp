/** @file @brief Verify sound ownership and independent playback through the public audio facade. */
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

import VVEngine;

namespace {
std::size_t checks{}; ///< Count of successful public contract checks.

/** @brief Report the failed operation rather than an unexplained nonzero exit code. */
void check(bool condition, std::string_view description) {
   if (!condition) { throw std::runtime_error(std::string{description}); }
   ++checks;
}

/** @brief Verify precise facade errors without exposing any selected backend. */
template<typename T>
void expectError(const std::expected<T, vve::Error> &result, vve::Error error, std::string_view description) {
   check(!result && result.error() == error, description);
}

/** @brief Unwrap success while retaining the name of the failing operation. */
template<typename T>
T value(const std::expected<T, vve::Error> &result, std::string_view description) {
   check(result.has_value(), description);
   return *result;
}

/** @brief Write WAV integers in little-endian order on every supported platform. */
void integer(std::ofstream &out, std::uint32_t number, int bytes) {
   for (int byte = 0; byte < bytes; ++byte) { out.put(static_cast<char>((number >> (8 * byte)) & 0xffU)); }
}

/** @brief Create nonzero PCM data without downloaded assets or a separate authoring tool. */
void writeWave(const std::filesystem::path &path) {
   constexpr std::uint32_t rate = 22050, samples = rate / 4, dataBytes = samples * 2;
   std::ofstream out{path, std::ios::binary};
   out.write("RIFF", 4); integer(out, 36 + dataBytes, 4); out.write("WAVEfmt ", 8);
   integer(out, 16, 4); integer(out, 1, 2); integer(out, 1, 2); // PCM mono format.
   integer(out, rate, 4); integer(out, rate * 2, 4); integer(out, 2, 2); integer(out, 16, 2);
   out.write("data", 4); integer(out, dataBytes, 4);
   // A quarter-second tone is short enough for bounded asynchronous completion checks.
   for (std::uint32_t sample = 0; sample < samples; ++sample) {
      const auto amplitude = static_cast<std::int16_t>(4000.0 * std::sin(6.283185307179586 * 440.0 * sample / rate));
      integer(out, static_cast<std::uint16_t>(amplitude), 2);
   }
   out.close();
   check(out.good(), "create PCM WAV fixture");
}

/** @brief Allow insignificant conversion differences in public gains. */
bool near(vve::AudioVolume actual, float expected) { return std::abs(actual.value - expected) < 0.0001F; }

/** @brief Bound device-driven playback completion without requiring physical speakers. */
void waitUntilFinished(vve::AudioSystem &audio, vve::AudioPlaybackHandle voice) {
   const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
   while (audio.isPlaying(voice) && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
   }
   check(!audio.isPlaying(voice), "finite clip finishes within five seconds");
   check(!audio.isPaused(voice), "completed clip is not paused");
}

/** @brief Exercise controls and ownership using only the public engine and audio facades. */
void testAudio(const std::filesystem::path &decodedPath, const std::filesystem::path &demandPath,
               const std::filesystem::path &badPath) {
   using namespace vve;
   Engine engine;
   auto world = engine.world();
   auto &audio = world.get<AudioSystem>();
   const AudioPlaybackOptions loop{.loop = true, .volume = AudioVolume{.value = 0.5F}};
   check(!audio.initialized(), "audio does not open a device at engine construction");
   check(near(audio.masterVolume(), 1.0F), "uninitialized master gain is one");
   expectError(audio.loadSound(decodedPath), Error::not_initialized, "load requires init");
   expectError(audio.play(SoundHandle{}), Error::not_initialized, "play requires init");
   expectError(audio.unloadSound(SoundHandle{}), Error::not_initialized, "unload requires init");
   expectError(audio.stop(AudioPlaybackHandle{}), Error::not_initialized, "stop requires init");
   expectError(audio.pause(AudioPlaybackHandle{}), Error::not_initialized, "pause requires init");
   expectError(audio.resume(AudioPlaybackHandle{}), Error::not_initialized, "resume requires init");
   expectError(audio.volume(AudioPlaybackHandle{}), Error::not_initialized, "volume query requires init");
   expectError(audio.setVolume(AudioPlaybackHandle{}, AudioVolume{}), Error::not_initialized, "voice gain requires init");
   expectError(audio.setMasterVolume(AudioVolume{}), Error::not_initialized, "master gain requires init");
   audio.shutdown(); audio.stopAll();
   check(!audio.isPlaying(AudioPlaybackHandle{}) && !audio.isPaused(AudioPlaybackHandle{}), "empty voice is inactive");
   const auto initialized = audio.init();
   check(initialized.has_value(), std::string{"initialize dummy device: "} + audio.lastError());
   check(audio.initialized(), "init opens audio device");
   check(audio.init().has_value(), "init is idempotent");
   expectError(audio.loadSound(decodedPath.parent_path() / "missing.wav"), Error::io_error, "missing file reports io_error");
   check(!audio.lastError().empty(), "failed load provides a diagnostic");
   expectError(audio.loadSound(badPath), Error::io_error, "malformed file reports io_error");
   expectError(audio.loadSound(std::filesystem::path{}), Error::invalid_argument, "empty path reports invalid_argument");
   expectError(audio.loadSound(decodedPath, static_cast<AudioLoadMode>(99)), Error::invalid_argument,
               "invalid decode mode reports invalid_argument");
   const auto decoded = value(audio.loadSound(decodedPath), "load decoded PCM");
   const auto demand = value(audio.loadSound(demandPath, AudioLoadMode::on_demand), "load on-demand PCM");
   check(decoded.valid() && demand.valid(), "sounds have valid typed handles");
   expectError(audio.play(SoundHandle{}), Error::invalid_handle, "empty sound cannot play");
   expectError(audio.unloadSound(SoundHandle{}), Error::invalid_handle, "empty sound cannot unload");
   expectError(audio.stop(AudioPlaybackHandle{}), Error::invalid_handle, "empty voice cannot stop");
   expectError(audio.pause(AudioPlaybackHandle{}), Error::invalid_handle, "empty voice cannot pause");
   expectError(audio.resume(AudioPlaybackHandle{}), Error::invalid_handle, "empty voice cannot resume");
   expectError(audio.volume(AudioPlaybackHandle{}), Error::invalid_handle, "empty voice has no volume");
   const auto first = value(audio.play(decoded, loop), "play first loop");
   const auto second = value(audio.play(decoded, loop), "overlap same sound");
   const auto demandVoice = value(audio.play(demand, loop), "play on-demand loop");
   check(first.valid() && second.valid() && first != second, "overlap creates distinct voice handles");
   check(audio.isPlaying(first) && audio.isPlaying(second) && audio.isPlaying(demandVoice), "all three voices are active");
   check(near(value(audio.volume(first), "initial voice gain"), 0.5F), "play applies initial gain");
   check(audio.pause(first).has_value(), "pause first voice");
   check(audio.isPaused(first), "paused state is visible");
   check(audio.isPlaying(second) && !audio.isPaused(second), "pausing leaves overlapping voice running");
   // Delay beyond the clip duration so looping and pause retention are both observable.
   std::this_thread::sleep_for(std::chrono::milliseconds{400});
   check(audio.isPlaying(second), "loop survives clip duration");
   check(audio.isPaused(first), "paused voice survives clip duration");
   check(audio.resume(first).has_value(), "resume first voice");
   check(audio.isPlaying(first) && !audio.isPaused(first), "resumed voice is active");
   check(audio.setVolume(first, AudioVolume{.value = 0.25F}).has_value(), "set voice gain");
   check(near(value(audio.volume(first), "changed gain"), 0.25F), "voice gain retained");
   check(near(value(audio.volume(second), "other gain"), 0.5F), "voice gains are independent");
   check(audio.setMasterVolume(AudioVolume{.value = 0.3F}).has_value(), "set master gain");
   check(near(audio.masterVolume(), 0.3F), "master gain retained");
   check(near(value(audio.volume(first), "gain under master"), 0.25F), "master leaves voice gain unchanged");
   check(audio.setVolume(first, AudioVolume{.value = 0.0F}).has_value(), "voice gain accepts zero");
   check(audio.setVolume(first, AudioVolume{.value = 1.0F}).has_value(), "voice gain accepts one");
   check(audio.setMasterVolume(AudioVolume{.value = 0.0F}).has_value(), "master gain accepts zero");
   check(audio.setMasterVolume(AudioVolume{.value = 1.0F}).has_value(), "master gain accepts one");
   // Invalid gains cannot change existing settings or create a voice.
   for (const float invalid : {-0.1F, 1.1F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
      expectError(audio.setVolume(first, AudioVolume{.value = invalid}), Error::invalid_argument, "reject invalid voice gain");
      expectError(audio.setMasterVolume(AudioVolume{.value = invalid}), Error::invalid_argument, "reject invalid master gain");
      expectError(audio.play(decoded, AudioPlaybackOptions{.volume = AudioVolume{.value = invalid}}),
                  Error::invalid_argument, "play rejects invalid gain");
   }
   check(near(value(audio.volume(first), "gain after invalid input"), 1.0F), "invalid input preserves voice gain");
   check(near(audio.masterVolume(), 1.0F), "invalid input preserves master gain");
   check(audio.stop(first).has_value(), "stop first voice");
   check(!audio.isPlaying(first) && !audio.isPaused(first), "stopped voice is inactive");
   expectError(audio.resume(first), Error::invalid_handle, "stop invalidates voice");
   check(audio.isPlaying(second), "stop preserves overlapping voice");
   check(audio.unloadSound(decoded).has_value(), "unload playing sound");
   check(!audio.isPlaying(second), "unload stops sound's voices");
   expectError(audio.volume(second), Error::invalid_handle, "unload invalidates voices");
   expectError(audio.play(decoded), Error::invalid_handle, "unloaded sound cannot play");
   expectError(audio.unloadSound(decoded), Error::invalid_handle, "sound cannot unload twice");
   check(audio.isPlaying(demandVoice), "unload preserves different sound");
   audio.stopAll();
   check(!audio.isPlaying(demandVoice), "stopAll stops remaining voice");
   expectError(audio.pause(demandVoice), Error::invalid_handle, "stopAll invalidates voice handles");
   const auto finite = value(audio.play(demand), "play finite clip");
   waitUntilFinished(audio, finite);
   const auto replacement = value(audio.play(demand, loop), "play after finite completion");
   check(replacement != finite, "new playback has a fresh handle");
   expectError(audio.volume(finite), Error::invalid_handle, "next play reclaims completed voice");
   // Mixer lifetimes and handle ownership remain isolated when two engines coexist.
   {
      Engine otherEngine;
      auto otherWorld = otherEngine.world();
      auto &other = otherWorld.get<AudioSystem>();
      const auto otherInitialized = other.init();
      check(otherInitialized.has_value(), std::string{"initialize second mixer: "} + other.lastError());
      const auto otherSound = value(other.loadSound(decodedPath), "second mixer loads sound");
      const auto otherVoice = value(other.play(otherSound, loop), "second mixer plays sound");
      expectError(other.play(demand), Error::invalid_handle, "reject foreign sound in second mixer");
      expectError(other.pause(replacement), Error::invalid_handle, "reject foreign voice in second mixer");
      expectError(audio.play(otherSound), Error::invalid_handle, "reject foreign sound in first mixer");
      expectError(audio.stop(otherVoice), Error::invalid_handle, "reject foreign voice in first mixer");
      other.shutdown();
      check(!other.initialized(), "second mixer independently shuts down");
      check(audio.isPlaying(replacement), "first mixer survives second shutdown");
      check(other.init().has_value(), "second mixer reinitializes");
      expectError(other.play(otherSound), Error::invalid_handle, "shutdown invalidates old sound");
      const auto reloaded = value(other.loadSound(decodedPath), "load after second reinit");
      check(other.play(reloaded, loop).has_value(), "second mixer plays after reinit");
   }
   check(audio.isPlaying(replacement), "first mixer survives other engine destruction");
   audio.shutdown(); audio.shutdown();
   check(!audio.initialized(), "shutdown is idempotent");
   check(!audio.isPlaying(replacement) && !audio.isPaused(replacement), "shutdown stops old voices");
   check(near(audio.masterVolume(), 1.0F), "shutdown restores uninitialized master gain");
   check(audio.init().has_value(), "first mixer reinitializes");
   expectError(audio.play(demand), Error::invalid_handle, "reinit cannot revive old sound");
   expectError(audio.pause(replacement), Error::invalid_handle, "reinit cannot revive old voice");
   const auto finalSound = value(audio.loadSound(decodedPath), "load after first reinit");
   const auto finalVoice = value(audio.play(finalSound, loop), "play after first reinit");
   check(audio.isPlaying(finalVoice), "reinitialized first mixer operational");
   check(audio.unloadSound(finalSound).has_value(), "unload final sound");
}

/** @brief Verify that device initialization failure leaves no partially owned mixer. */
void testInitFailure() {
   vve::Engine engine;
   auto world = engine.world();
   auto &audio = world.get<vve::AudioSystem>();
   // CTest selects an intentionally nonexistent SDL audio driver for this process.
   for (int attempt = 0; attempt < 2; ++attempt) {
      expectError(audio.init(), vve::Error::platform_error, "unavailable driver reports platform_error");
      check(!audio.initialized(), "failed init leaves mixer uninitialized");
      check(!audio.lastError().empty(), "failed init provides a diagnostic");
      check(near(audio.masterVolume(), 1.0F), "failed init retains default master gain");
      expectError(audio.play(vve::SoundHandle{}), vve::Error::not_initialized, "failed init cannot play");
      audio.shutdown(); audio.shutdown();
   }
}
} // namespace

/** @brief Emit deterministic counts and operation names suitable for automated diagnosis. */
int main(int argc, char **argv) {
   try {
      if (argc == 2 && std::string_view{argv[1]} == "--expect-init-failure") {
         testInitFailure();
         std::cout << "audio_init_failure_result=pass checks=" << checks << '\n';
         return 0;
      }
      const std::filesystem::path directory{VVE_TEST_TMP_DIR};
      std::filesystem::create_directories(directory);
      const auto decoded = directory / "decoded-tone.wav", demand = directory / "demand-tone.wav";
      const auto bad = directory / "malformed.wav";
      writeWave(decoded); writeWave(demand);
      {
         std::ofstream invalid{bad, std::ios::binary};
         invalid << "This is deliberately not audio.";
         check(invalid.good(), "create malformed fixture");
      }
      testAudio(decoded, demand, bad);
      std::cout << "audio_test_result=pass checks=" << checks << '\n';
      return 0;
   } catch (const std::exception &exception) {
      std::cerr << "audio_test_result=fail checks=" << checks << " reason=" << exception.what() << '\n';
      return 1;
   }
}
