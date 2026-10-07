module;
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

module VVEngine.Simple;
import std;
import :Audio;

/// @file
/// @brief Owns SDL audio lifetimes; SDL's device callback performs mixing independently of drawing.
namespace vve::simple {
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
			if (voice->second.first == sound) {
				MIX_DestroyTrack(voice->second.second);
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
		voices_.emplace(handle, std::pair{sound, voice});
		return handle;
	}

	/// @brief Looks up an engine-owned voice without accepting another engine's handles.
	auto AudioSystem::track(AudioPlaybackHandle voice) const -> std::expected<MIX_Track *, Error> {
		if (!mixer_) { return fail(Error::not_initialized, "Audio is not initialized."); }
		const auto found = voices_.find(voice);
		if (found == voices_.end()) { return fail(Error::invalid_handle, "Unknown or expired playback handle."); }
		return found->second.second;
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
		for (const auto &[handle, voice] : voices_) { MIX_DestroyTrack(voice.second); }
		voices_.clear();
	}

	/// @brief Queries active playback; expired and unknown handles are simply inactive.
	bool AudioSystem::isPlaying(AudioPlaybackHandle voice) const {
		const auto found = voices_.find(voice);
		return found != voices_.end() && MIX_TrackPlaying(found->second.second);
	}

	/// @brief Distinguishes paused voices from finished voices during cleanup.
	bool AudioSystem::isPaused(AudioPlaybackHandle voice) const {
		const auto found = voices_.find(voice);
		return found != voices_.end() && MIX_TrackPaused(found->second.second);
	}

	/// @brief Copies the most recent diagnostic for callers handling a public Error.
	std::string AudioSystem::lastError() const { return last_error_; }

	/// @brief Reclaims naturally finished tracks; called by engine.step and before creating another voice.
	void AudioSystem::collectFinished() {
		for (auto voice = voices_.begin(); voice != voices_.end();) {
			if (!MIX_TrackPlaying(voice->second.second) && !MIX_TrackPaused(voice->second.second)) {
				MIX_DestroyTrack(voice->second.second);
				voice = voices_.erase(voice);
			} else { ++voice; }
		}
	}
}
