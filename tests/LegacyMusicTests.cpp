/** @file @brief Verify the original V2 music decodes and plays through the public V3 audio facade. */
#include <array>
#include <chrono>
#include <cmath>
#include <expected>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

import VVEngine;

namespace {
std::size_t checks{}; ///< Successful encoded-asset and playback contract checks.

/** @brief Keep failures attributable to the actual track and decoding mode under test. */
void check(bool condition, std::string_view context, std::string_view operation) {
   if (!condition) { throw std::runtime_error(std::string{context} + ": " + std::string{operation}); }
   ++checks;
}

/** @brief Unwrap asset operations with their decoder diagnostic when they fail. */
template<typename T>
T value(const std::expected<T, vve::Error> &result, const vve::AudioSystem &audio,
        std::string_view context, std::string_view operation) {
   check(result.has_value(), context, std::string{operation} + ": " + audio.lastError());
   return *result;
}

/** @brief Allow insignificant floating-point conversion differences in playback gains. */
bool near(vve::AudioVolume actual, float expected) { return std::abs(actual.value - expected) < 0.0001F; }

/** @brief Exercise a real encoded file in one loading mode without waiting for its full duration. */
void testTrack(vve::AudioSystem &audio, const std::filesystem::path &directory,
               std::string_view filename, vve::AudioLoadMode mode) {
   using namespace vve;
   const std::string_view modeName = mode == AudioLoadMode::decoded ? "decoded" : "on_demand";
   const auto context = std::string{filename} + " mode=" + std::string{modeName};
   const auto before = checks;
   const auto path = directory / filename;
   check(std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) > 0,
         context, "original encoded asset exists");
   const auto sound = value(audio.loadSound(path, mode), audio, context, "load encoded track");
   const AudioPlaybackOptions options{.loop = true, .volume = AudioVolume{.value = 0.025F}};
   const auto first = value(audio.play(sound, options), audio, context, "start looping music");
   const auto overlap = value(audio.play(sound, options), audio, context, "start overlapping music voice");
   check(first != overlap, context, "overlap has an independent playback handle");
   // Give the SDL dummy device several callback periods to consume encoded data in on-demand mode.
   std::this_thread::sleep_for(std::chrono::milliseconds{120});
   check(audio.isPlaying(first) && audio.isPlaying(overlap), context, "both encoded voices remain active");
   check(audio.setVolume(first, AudioVolume{.value = 0.05F}).has_value(), context, "adjust encoded voice gain");
   check(near(value(audio.volume(first), audio, context, "query adjusted gain"), 0.05F),
         context, "adjusted gain is retained");
   check(near(value(audio.volume(overlap), audio, context, "query overlapping gain"), 0.025F),
         context, "overlapping voice retains its independent gain");
   check(audio.pause(first).has_value(), context, "pause encoded music");
   check(audio.isPaused(first) && !audio.isPaused(overlap) && audio.isPlaying(overlap),
         context, "pause preserves overlapping encoded voice");
   check(audio.resume(first).has_value(), context, "resume encoded music");
   check(audio.isPlaying(first) && !audio.isPaused(first), context, "resumed encoded voice is active");
   check(audio.stop(first).has_value(), context, "stop encoded music voice");
   check(!audio.isPlaying(first) && audio.isPlaying(overlap), context, "stop preserves the other encoded voice");
   check(audio.unloadSound(sound).has_value(), context, "unload encoded track while remaining voice plays");
   check(!audio.isPlaying(overlap), context, "unload releases the remaining encoded voice");
   std::cout << "legacy_music_track=" << filename << " mode=" << modeName
             << " result=pass checks=" << checks - before << '\n';
}
} // namespace

/** @brief Run both MP3 tracks and the OGG track through both public loading modes. */
int main() {
   try {
      const std::filesystem::path directory{VVE_TEST_V2_MUSIC_DIR};
      vve::Engine engine;
      auto world = engine.world();
      auto &audio = world.get<vve::AudioSystem>();
      const auto initialized = audio.init();
      check(initialized.has_value(), "legacy music", "initialize dummy audio: " + audio.lastError());
      check(audio.setMasterVolume(vve::AudioVolume{.value = 0.1F}).has_value(), "legacy music", "set quiet master gain");
      check(near(audio.masterVolume(), 0.1F), "legacy music", "quiet master gain retained");
      constexpr std::array tracks{"dance.mp3", "ophelia.mp3", "getout.ogg"};
      constexpr std::array modes{vve::AudioLoadMode::decoded, vve::AudioLoadMode::on_demand};
      // Real legacy files, rather than synthetic WAVs, exercise the packaged MP3 and Vorbis decoders.
      for (const auto filename : tracks) {
         for (const auto mode : modes) { testTrack(audio, directory, filename, mode); }
      }
      audio.shutdown();
      check(!audio.initialized(), "legacy music", "release mixer after all encoded tracks");
      std::cout << "legacy_music_result=pass tracks=3 modes=2 checks=" << checks << '\n';
      return 0;
   } catch (const std::exception &exception) {
      std::cerr << "legacy_music_result=fail checks=" << checks << " reason=" << exception.what() << '\n';
      return 1;
   }
}
