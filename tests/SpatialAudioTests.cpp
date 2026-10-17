/** @file @brief Deterministic spatial policy and actual offline SDL mixer output; no speakers or device needed. */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
import VVEngine.Simple;

namespace {
std::size_t checks{}; ///< Successful deterministic checks, printed for automated diagnosis.

/** @brief Name every failed assertion in the executable's diagnostic output. */
void check(bool condition, std::string_view message) {
   if (!condition) { throw std::runtime_error(std::string{message}); }
   ++checks;
}

/** @brief Compare coordinate transforms and ratios with normal floating-point tolerance. */
bool near(float actual, float expected, float tolerance = 0.0001F) { return std::abs(actual - expected) < tolerance; }

/** @brief Exercise coordinate conventions, Doppler signs and invalid inputs without SDL initialization. */
void policy() {
   using namespace vve;
   AudioListener listener;
   AudioSource source{.position = Position{.value = Vec3(10, 0, 0)}, .reference_distance = 2, .max_distance = 8};
   auto result = simple::audioSpatialParameters(listener, source);
   check(result && near((*result)[0], 4) && near((*result)[3], 1), "distance clamps in reference-distance units");
   listener.position.value = Vec3(10, 0, 0);
   result = simple::audioSpatialParameters(listener, source);
   check(result && near((*result)[0], 0) && near((*result)[3], 1), "coincident listener/source remains finite");
   listener = {};
   listener.forward.value = Vec3(1, 0, 0);
   result = simple::audioSpatialParameters(listener, source);
   check(result && near((*result)[0], 0) && near((*result)[2], -4), "listener orientation rotates source ahead");
   source.velocity = Vec3(-50, 0, 0);
   check((*simple::audioSpatialParameters(listener, source))[3] > 1, "approaching source raises pitch");
   source.velocity = Vec3(50, 0, 0);
   check((*simple::audioSpatialParameters(listener, source))[3] < 1, "departing source lowers pitch");
   listener.velocity = source.velocity;
   check(near((*simple::audioSpatialParameters(listener, source))[3], 1), "equal radial velocities cancel Doppler");
   listener.velocity = Vec3(1000, 0, 0);
   source.velocity = Vec3(-1000, 0, 0);
   check(near((*simple::audioSpatialParameters(listener, source))[3], 2), "extreme velocities bound Doppler");
   source.reference_distance = 0;
   check(!simple::audioSpatialParameters(listener, source), "reject zero reference distance");
   source = {};
   source.max_distance = 0.5F;
   check(!simple::audioSpatialParameters(listener, source), "reject reversed distance limits");
   source = {};
   source.position.value.x = std::numeric_limits<float>::quiet_NaN();
   check(!simple::audioSpatialParameters(listener, source), "reject nonfinite source");
   source = {};
   listener.forward.value = zeroVec3();
   check(!simple::audioSpatialParameters(listener, source), "reject zero listener direction");
   listener = {};
   listener.up = listener.forward;
   check(!simple::audioSpatialParameters(listener, source), "reject parallel listener directions");
}

/** @brief Render known mono PCM through the same native position/frequency calls used by AudioSystem. */
std::array<float, 3> measure(MIX_Mixer *mixer, MIX_Audio *audio, const vve::AudioListener &listener,
                            const vve::AudioSource &source) {
   const auto parameters = vve::simple::audioSpatialParameters(listener, source);
   check(parameters.has_value(), "valid offline placement");
   const MIX_Point3D point{(*parameters)[0], (*parameters)[1], (*parameters)[2]};
   auto *track = MIX_CreateTrack(mixer);
   check(track && MIX_SetTrackAudio(track, audio), "create offline track");
   check(MIX_SetTrack3DPosition(track, &point) && MIX_SetTrackFrequencyRatio(track, (*parameters)[3]), "apply native spatial mix");
   check(MIX_PlayTrack(track, 0), "start offline track");
   std::array<float, 16384> pcm{};
   const auto generated = MIX_Generate(mixer, pcm.data(), static_cast<int>(sizeof(pcm)));
   std::cout << "offline_bytes=" << generated << " requested=" << sizeof(pcm) << " pitch=" << (*parameters)[3] << '\n';
   check(generated > 0 && generated <= static_cast<int>(sizeof(pcm)),
         std::string{"generate offline stereo PCM: "} + SDL_GetError());
   const auto samples = static_cast<std::size_t>(generated) / sizeof(float);
   check(samples > 256 && samples % 2 == 0, "complete stereo frames beyond resampling startup");
   float left = 0, right = 0, crossings = 0;
   // MIX_Generate may append silence: measure only real output after resampling startup.
   const auto channel = point.x < 0 ? 0U : 1U;
   for (std::size_t sample = 256; sample < samples; sample += 2) {
      left += pcm[sample] * pcm[sample]; right += pcm[sample + 1] * pcm[sample + 1];
      crossings += pcm[sample + channel - 2] <= 0 && pcm[sample + channel] > 0;
   }
   MIX_DestroyTrack(track);
   const auto frames = static_cast<float>((samples - 256) / 2);
   return {std::sqrt(left / frames), std::sqrt(right / frames), crossings * 48000 / frames};
}

/** @brief Verify native panning, attenuation and audible Doppler with entirely synchronous mixing. */
void offline() {
   using namespace vve;
   check(MIX_Init(), "initialize offline mixer");
   const SDL_AudioSpec mono{SDL_AUDIO_F32, 1, 48000}, stereo{SDL_AUDIO_F32, 2, 48000};
   auto *mixer = MIX_CreateMixer(&stereo);
   check(mixer != nullptr, "create mixer without audio device");
   std::vector<float> tone(48000);
   // One second at 440 Hz leaves enough input for every synchronous measurement.
   for (std::size_t sample = 0; sample < tone.size(); ++sample) {
      tone[sample] = 0.2F * std::sin(6.28318530718F * 440 * static_cast<float>(sample) / 48000);
   }
   auto *audio = MIX_LoadRawAudio(mixer, tone.data(), tone.size() * sizeof(float), &mono);
   check(audio != nullptr, "load generated mono PCM");
   AudioListener listener;
   AudioSource source{.position = Position{.value = Vec3(1, 0, 0)}};
   const auto right = measure(mixer, audio, listener, source);
   source.position.value.x = -1;
   const auto left = measure(mixer, audio, listener, source);
   check(right[1] > right[0] * 5 && left[0] > left[1] * 5, "actual PCM pans to the correct speaker");
   source.position.value.x = 10;
   const auto far = measure(mixer, audio, listener, source);
   check(near(far[1] / right[1], 0.1F, 0.005F), "actual PCM inverse-distance attenuation");
   source.max_distance = 2;
   const auto clamped = measure(mixer, audio, listener, source);
   check(near(clamped[1] / right[1], 0.5F, 0.005F), "actual PCM attenuation clamps at max distance");
   source.position.value.x = 1;
   source.velocity = Vec3(-50, 0, 0);
   const auto approaching = measure(mixer, audio, listener, source);
   check(near(approaching[2] / right[2], 343.0F / 293.0F, 0.04F), "actual PCM frequency rises with Doppler");
   std::cout << "right_energy=" << right[1] << " far_ratio=" << far[1] / right[1]
             << " doppler_ratio=" << approaching[2] / right[2] << '\n';
   MIX_DestroyAudio(audio); MIX_DestroyMixer(mixer); MIX_Quit();
}
} // namespace

/** @brief Emit compact measurements and deterministic pass/fail output for the calling controller. */
int main() {
   try {
      policy(); offline();
      std::cout << "spatial_audio_result=pass checks=" << checks << '\n';
      return 0;
   } catch (const std::exception &exception) {
      std::cerr << "spatial_audio_result=fail checks=" << checks << " reason=" << exception.what() << '\n';
      return 1;
   }
}
