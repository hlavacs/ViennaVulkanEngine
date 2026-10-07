export module VVEngine:Audio;
import std;
import :Implementation;
import VVEngine.Types;

/// @file
/// @brief Audio facade: games use handles without depending on SDL3_mixer.
export namespace vve {
	/// @brief Engine-owned sound catalog and overlapping playback controls.
	/// Call on the main thread. Copies share the same engine-owned audio system.
	class AudioSystem {
	public:
		AudioSystem(const AudioSystem &) = default;
		AudioSystem(AudioSystem &&) noexcept = default;
		AudioSystem &operator=(const AudioSystem &) = delete;
		AudioSystem &operator=(AudioSystem &&) = delete;

		/// @brief Opens the default output device; repeated calls succeed without reopening it.
		[[nodiscard]] auto init() -> std::expected<void, Error> { return impl_.init(); }
		/// @brief Stops voices, unloads sounds and closes audio; safe to repeat.
		void shutdown() { impl_.shutdown(); }
		/// @brief Reports whether a device was successfully opened.
		[[nodiscard]] bool initialized() const { return impl_.initialized(); }
		/// @brief Loads an audio file after init; paths are converted to UTF-8 for SDL.
		[[nodiscard]] auto loadSound(const std::filesystem::path &path, AudioLoadMode mode = AudioLoadMode::decoded)
			-> std::expected<SoundHandle, Error> { return impl_.loadSound(path, mode); }
		/// @brief Stops every voice using this sound, then releases the resource and its handle.
		[[nodiscard]] auto unloadSound(SoundHandle sound) -> std::expected<void, Error> {
			return impl_.unloadSound(sound);
		}
		/// @brief Starts a new voice; the same sound can play several times simultaneously.
		[[nodiscard]] auto play(SoundHandle sound, AudioPlaybackOptions options = {})
			-> std::expected<AudioPlaybackHandle, Error> { return impl_.play(sound, options); }
		/// @brief Stops and releases one voice; its handle becomes invalid.
		[[nodiscard]] auto stop(AudioPlaybackHandle voice) -> std::expected<void, Error> { return impl_.stop(voice); }
		/// @brief Pauses one voice without discarding its current position.
		[[nodiscard]] auto pause(AudioPlaybackHandle voice) -> std::expected<void, Error> { return impl_.pause(voice); }
		/// @brief Continues a paused voice from its saved position.
		[[nodiscard]] auto resume(AudioPlaybackHandle voice) -> std::expected<void, Error> { return impl_.resume(voice); }
		/// @brief Sets a finite per-voice volume in [0, 1].
		[[nodiscard]] auto setVolume(AudioPlaybackHandle voice, AudioVolume volume) -> std::expected<void, Error> {
			return impl_.setVolume(voice, volume);
		}
		/// @brief Returns the per-voice volume or invalid_handle if the voice was released.
		[[nodiscard]] auto volume(AudioPlaybackHandle voice) const -> std::expected<AudioVolume, Error> {
			return impl_.volume(voice);
		}
		/// @brief Sets the master volume in [0, 1], multiplying all voice volumes.
		[[nodiscard]] auto setMasterVolume(AudioVolume volume) -> std::expected<void, Error> {
			return impl_.setMasterVolume(volume);
		}
		/// @brief Returns the master volume, or its default of one before init.
		[[nodiscard]] AudioVolume masterVolume() const { return impl_.masterVolume(); }
		/// @brief Stops and releases every voice; loaded sounds remain available.
		void stopAll() { impl_.stopAll(); }
		/// @brief True only while contributing audio; paused, finished and expired handles return false.
		[[nodiscard]] bool isPlaying(AudioPlaybackHandle voice) const { return impl_.isPlaying(voice); }
		/// @brief True only while paused; unknown and expired handles return false.
		[[nodiscard]] bool isPaused(AudioPlaybackHandle voice) const { return impl_.isPaused(voice); }
		/// @brief Returns the latest failure description; a successful call does not erase it.
		[[nodiscard]] std::string lastError() const { return impl_.lastError(); }

	private:
		template <typename... TSystems> friend class Engine;
		using Impl = detail::AudioSystemImpl; ///< Selected implementation of the audio contract.
		/// @brief Binds this non-owning facade to the engine's audio system.
		explicit AudioSystem(Impl &implementation) noexcept : impl_{implementation} {}
		Impl &impl_; ///< Audio owner; wrappers and Worlds must not outlive the engine.
	};
}
