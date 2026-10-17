module;
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

module VVEngine.Simple;
import std;
import :Audio;

/// @file
/// @brief Owns SDL audio lifetimes; SDL's device callback performs mixing independently of drawing.
namespace vve::simple {
	/// @brief Converts world coordinates once; SDL provides speaker panning and inverse-distance attenuation.
	auto audioSpatialParameters(const AudioListener &listener, const AudioSource &source)
		-> std::expected<std::array<float, 4>, Error> {
		const auto finite = [](const Vec3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
		if (!finite(listener.position.value) || !finite(listener.forward.value) || !finite(listener.up.value)
			|| !finite(listener.velocity) || !finite(source.position.value) || !finite(source.velocity)
			|| !std::isfinite(source.reference_distance) || !std::isfinite(source.max_distance)
			|| source.reference_distance <= 0 || source.max_distance < source.reference_distance) {
			return std::unexpected(Error::invalid_argument);
		}
		const auto right = math::cross(listener.forward.value, listener.up.value);
		if (!finite(right) || !std::isfinite(math::lengthSquared(listener.forward.value))
			|| !std::isfinite(math::lengthSquared(right)) || math::lengthSquared(listener.forward.value) < 1e-12
			|| math::lengthSquared(right) < 1e-12) {
			return std::unexpected(Error::invalid_argument);
		}
		const auto forward = math::normalize(listener.forward.value), side = math::normalize(right);
		const auto up = math::cross(side, forward), delta = math::subtract(source.position.value, listener.position.value);
		const auto distance = math::length(delta);
		if (!std::isfinite(distance)) { return std::unexpected(Error::invalid_argument); }
		// Clamp attenuation at max_distance and suppress undefined direction/Doppler at coincidence.
		const auto direction = distance > 1e-6 ? math::scale(delta, one() / distance) : zeroVec3();
		const auto scaled = math::scale(direction, std::min(distance, static_cast<Scalar>(source.max_distance))
			/ source.reference_distance);
		constexpr Scalar speed = 343; // Speed of sound in metres per second; radial speeds remain subsonic.
		const auto radial = [&](const Vec3 &v) { return std::clamp(math::dot(v, direction), -speed * 0.9F, speed * 0.9F); };
		const auto doppler = std::clamp((speed + radial(listener.velocity)) / (speed + radial(source.velocity)),
			static_cast<Scalar>(0.5), static_cast<Scalar>(2));
		const std::array<float, 4> result{static_cast<float>(math::dot(scaled, side)), static_cast<float>(math::dot(scaled, up)),
			static_cast<float>(-math::dot(scaled, forward)), static_cast<float>(doppler)};
		if (!std::ranges::all_of(result, [](float value) { return std::isfinite(value); })) {
			return std::unexpected(Error::invalid_argument);
		}
		return result;
	}

	/// @brief Releases the device and all audio owned by this system.
	AudioSystem::~AudioSystem() { shutdown(); }

	/// @brief Stores a diagnostic alongside the stable public error category.
	auto AudioSystem::fail(Error error, std::string message) const -> std::unexpected<Error> {
		last_error_ = std::move(message);
		return std::unexpected(error);
	}

	/// @brief Rejects NaN, infinity and out-of-range volume before entering SDL.
	bool AudioSystem::validVolume(AudioVolume volume) {
		return std::isfinite(volume.value) && volume.value >= 0.0F && volume.value <= 1.0F;
	}

	/// @brief Initializes only on explicit request; each owner balances its MIX_Init reference.
	auto AudioSystem::init() -> std::expected<void, Error> {
		if (mixer_) { return {}; }
		if (!MIX_Init()) { return fail(Error::platform_error, SDL_GetError()); }
		mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (!mixer_) {
			const auto error = fail(Error::platform_error, SDL_GetError());
			MIX_Quit();
			return error;
		}
		return {};
	}

	/// @brief Destroys tracks before their audio, then closes the mixer and releases its init reference.
	void AudioSystem::shutdown() {
		if (!mixer_) { return; }
		stopAll();
		for (const auto &[handle, audio] : sounds_) { MIX_DestroyAudio(audio); }
		sounds_.clear();
		MIX_DestroyMixer(mixer_);
		mixer_ = nullptr;
		listener_ = {};
		MIX_Quit();
	}

	/// @brief Reports successful device initialization without consulting global SDL state.
	bool AudioSystem::initialized() const { return mixer_ != nullptr; }

	/// @brief Loads one reusable audio resource with either upfront or playback-time decoding.
	auto AudioSystem::loadSound(const std::filesystem::path &path, AudioLoadMode mode)
		-> std::expected<SoundHandle, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Call audio.init() before loading sounds."); }
		if (path.empty() || (mode != AudioLoadMode::decoded && mode != AudioLoadMode::on_demand)) {
			return fail(Error::invalid_argument, "Supply an audio path and a valid load mode.");
		}
		const auto utf8 = path.u8string();
		// SDL uses UTF-8 even on Windows, whose native filesystem paths are UTF-16.
		auto *audio = MIX_LoadAudio(mixer_, reinterpret_cast<const char *>(utf8.c_str()), mode == AudioLoadMode::decoded);
		if (!audio) { return fail(Error::io_error, SDL_GetError()); }
		const auto handle = makeCounterHandle<SoundHandle>();
		sounds_.emplace(handle, audio);
		return handle;
	}

	/// @brief Removes the dependent tracks before releasing a loaded sound.
	auto AudioSystem::unloadSound(SoundHandle sound) -> std::expected<void, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Audio is not initialized."); }
		const auto found = sounds_.find(sound);
		if (found == sounds_.end()) { return fail(Error::invalid_handle, "Unknown sound handle."); }
		for (auto voice = voices_.begin(); voice != voices_.end();) {
			if (voice->second.sound == sound) {
				MIX_DestroyTrack(voice->second.track);
				voice = voices_.erase(voice);
			} else { ++voice; }
		}
		MIX_DestroyAudio(found->second);
		sounds_.erase(found);
		return {};
	}

	/// @brief Creates a separate track for every play, so effects overlap and music can loop.
	auto AudioSystem::play(SoundHandle sound, AudioPlaybackOptions options)
		-> std::expected<AudioPlaybackHandle, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Audio is not initialized."); }
		if (!validVolume(options.volume)) { return fail(Error::invalid_argument, "Volume must be finite and in [0, 1]."); }
		const auto found = sounds_.find(sound);
		if (found == sounds_.end()) { return fail(Error::invalid_handle, "Unknown sound handle."); }
		collectFinished();
		auto *voice = MIX_CreateTrack(mixer_);
		if (!voice) { return fail(Error::platform_error, SDL_GetError()); }
		if (!MIX_SetTrackAudio(voice, found->second) || !MIX_SetTrackGain(voice, options.volume.value)) {
			const auto error = fail(Error::platform_error, SDL_GetError());
			MIX_DestroyTrack(voice);
			return error;
		}
		// Set looping before playback starts; changing it afterwards races the device callback.
		SDL_PropertiesID properties = 0;
		if (options.loop) {
			properties = SDL_CreateProperties();
			if (!properties || !SDL_SetNumberProperty(properties, MIX_PROP_PLAY_LOOPS_NUMBER, -1)) {
				const auto error = fail(Error::platform_error, SDL_GetError());
				if (properties) { SDL_DestroyProperties(properties); }
				MIX_DestroyTrack(voice);
				return error;
			}
		}
		const bool played = MIX_PlayTrack(voice, properties);
		// Properties are read synchronously by PlayTrack and are not retained.
		if (properties) { SDL_DestroyProperties(properties); }
		if (!played) {
			const auto error = fail(Error::platform_error, SDL_GetError());
			MIX_DestroyTrack(voice);
			return error;
		}
		const auto handle = makeCounterHandle<AudioPlaybackHandle>();
		voices_.emplace(handle, Voice{.sound = sound, .track = voice});
		return handle;
	}

	/// @brief Looks up an engine-owned voice without accepting another engine's handles.
	auto AudioSystem::track(AudioPlaybackHandle voice) const -> std::expected<MIX_Track *, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Audio is not initialized."); }
		const auto found = voices_.find(voice);
		if (found == voices_.end()) { return fail(Error::invalid_handle, "Unknown or expired playback handle."); }
		return found->second.track;
	}

	/// @brief Track destruction stops playback and synchronizes with the mixer's audio callback.
	auto AudioSystem::stop(AudioPlaybackHandle voice) -> std::expected<void, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		MIX_DestroyTrack(*found);
		voices_.erase(voice);
		return {};
	}

	/// @brief Suspends one track while retaining its playback cursor.
	auto AudioSystem::pause(AudioPlaybackHandle voice) -> std::expected<void, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		if (!MIX_PauseTrack(*found)) { return fail(Error::platform_error, SDL_GetError()); }
		return {};
	}

	/// @brief Continues a previously paused track.
	auto AudioSystem::resume(AudioPlaybackHandle voice) -> std::expected<void, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		if (!MIX_ResumeTrack(*found)) { return fail(Error::platform_error, SDL_GetError()); }
		return {};
	}

	/// @brief Applies validated linear gain to a single voice.
	auto AudioSystem::setVolume(AudioPlaybackHandle voice, AudioVolume volume) -> std::expected<void, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		if (!validVolume(volume)) { return fail(Error::invalid_argument, "Volume must be finite and in [0, 1]."); }
		if (!MIX_SetTrackGain(*found, volume.value)) { return fail(Error::platform_error, SDL_GetError()); }
		return {};
	}

	/// @brief Reads the configured per-voice gain independently of the master gain.
	auto AudioSystem::volume(AudioPlaybackHandle voice) const -> std::expected<AudioVolume, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		return AudioVolume{.value = MIX_GetTrackGain(*found)};
	}

	/// @brief Changes the playback speed, and with it the pitch, of a single voice.
	auto AudioSystem::setPitch(AudioPlaybackHandle voice, AudioPitch pitch) -> std::expected<void, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		if (!std::isfinite(pitch.value) || pitch.value < 0.01F || pitch.value > 100.0F) {
			return fail(Error::invalid_argument, "Pitch must be finite and in [0.01, 100].");
		}
		auto &state = voices_.at(voice);
		const auto previous = state.pitch;
		state.pitch = pitch;
		if (const auto result = applySpatial(state, listener_); !result) { state.pitch = previous; return result; }
		return {};
	}

	/// @brief Reads the playback speed ratio of a single voice.
	auto AudioSystem::pitch(AudioPlaybackHandle voice) const -> std::expected<AudioPitch, Error> {
		const auto found = track(voice);
		if (!found) { return std::unexpected(found.error()); }
		return voices_.at(voice).pitch;
	}

	/// @brief Applies spatial parameters without changing the user-selected gain or pitch values.
	auto AudioSystem::applySpatial(Voice &voice, const AudioListener &listener) -> std::expected<void, Error> {
		std::array<float, 4> parameters{0, 0, 0, 1};
		if (voice.source) {
			const auto result = audioSpatialParameters(listener, *voice.source);
			if (!result) { return fail(result.error(), "Use finite positions, velocities and independent listener directions; distances must be positive and ordered."); }
			parameters = *result;
		}
		const MIX_Point3D position{parameters[0], parameters[1], parameters[2]};
		if ((voice.source && !MIX_SetTrack3DPosition(voice.track, &position))
			|| !MIX_SetTrackFrequencyRatio(voice.track, std::clamp(voice.pitch.value * parameters[3], 0.01F, 100.0F))) {
			return fail(Error::platform_error, SDL_GetError());
		}
		return {};
	}

	/// @brief Updates positional tracks immediately; the listener is independent of any window or camera.
	auto AudioSystem::setListener(AudioListener listener) -> std::expected<void, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Audio is not initialized."); }
		if (!audioSpatialParameters(listener, AudioSource{})) { return fail(Error::invalid_argument, "Invalid audio listener."); }
		for (auto &[handle, voice] : voices_) {
			if (voice.source) { if (const auto result = applySpatial(voice, listener); !result) { return result; } }
		}
		listener_ = listener;
		return {};
	}

	/// @brief Enables or updates one positional voice while preserving its cursor and user controls.
	auto AudioSystem::setSource(AudioPlaybackHandle voice, AudioSource source) -> std::expected<void, Error> {
		if (const auto result = track(voice); !result) { return std::unexpected(result.error()); }
		auto &state = voices_.at(voice);
		const auto previous = state.source;
		state.source = source;
		if (const auto result = applySpatial(state, listener_); !result) { state.source = previous; return result; }
		return {};
	}

	/// @brief Disables spatialization; SDL resumes the sound's original channel layout.
	auto AudioSystem::clearSource(AudioPlaybackHandle voice) -> std::expected<void, Error> {
		if (const auto result = track(voice); !result) { return std::unexpected(result.error()); }
		auto &state = voices_.at(voice);
		if (!MIX_SetTrack3DPosition(state.track, nullptr)) { return fail(Error::platform_error, SDL_GetError()); }
		state.source.reset();
		return applySpatial(state, listener_);
	}

	/// @brief Applies validated linear gain to the entire mixer.
	auto AudioSystem::setMasterVolume(AudioVolume volume) -> std::expected<void, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Audio is not initialized."); }
		if (!validVolume(volume)) { return fail(Error::invalid_argument, "Volume must be finite and in [0, 1]."); }
		if (!MIX_SetMixerGain(mixer_, volume.value)) { return fail(Error::platform_error, SDL_GetError()); }
		return {};
	}

	/// @brief Returns the current mixer gain or its initialization default.
	AudioVolume AudioSystem::masterVolume() const { return AudioVolume{.value = mixer_ ? MIX_GetMixerGain(mixer_) : 1.0F}; }

	/// @brief Releases all voices while preserving the loaded sound catalog.
	void AudioSystem::stopAll() {
		for (const auto &[handle, voice] : voices_) { MIX_DestroyTrack(voice.track); }
		voices_.clear();
	}

	/// @brief Queries active playback; expired and unknown handles are simply inactive.
	bool AudioSystem::isPlaying(AudioPlaybackHandle voice) const {
		const auto found = voices_.find(voice);
		return found != voices_.end() && MIX_TrackPlaying(found->second.track);
	}

	/// @brief Distinguishes paused voices from finished voices during cleanup.
	bool AudioSystem::isPaused(AudioPlaybackHandle voice) const {
		const auto found = voices_.find(voice);
		return found != voices_.end() && MIX_TrackPaused(found->second.track);
	}

	/// @brief Copies the most recent diagnostic for callers handling a public Error.
	std::string AudioSystem::lastError() const { return last_error_; }

	/// @brief Reclaims naturally finished tracks; called by engine.step and before creating another voice.
	void AudioSystem::collectFinished() {
		for (auto voice = voices_.begin(); voice != voices_.end();) {
			if (!MIX_TrackPlaying(voice->second.track) && !MIX_TrackPaused(voice->second.track)) {
				MIX_DestroyTrack(voice->second.track);
				voice = voices_.erase(voice);
			} else { ++voice; }
		}
	}
}
