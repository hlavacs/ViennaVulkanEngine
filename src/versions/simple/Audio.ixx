module;
#include <SDL3_mixer/SDL_mixer.h>

export module VVEngine.Simple:Audio;
import std;
import VVEngine.Types;

/// @file
/// @brief SDL3_mixer implementation of the engine's audio contract.
export namespace vve::simple {
	/// @brief Validated listener-relative x/y/z in reference-distance units and bounded Doppler ratio; no device needed.
	[[nodiscard]] auto audioSpatialParameters(const AudioListener &listener, const AudioSource &source)
		-> std::expected<std::array<float, 4>, Error>;

	/// @brief Owns one mixer, loaded audio and one SDL track per independent playback.
	class AudioSystem {
	public:
		AudioSystem() = default;
		~AudioSystem();
		AudioSystem(const AudioSystem &) = delete;
		AudioSystem(AudioSystem &&) = delete;
		AudioSystem &operator=(const AudioSystem &) = delete;
		AudioSystem &operator=(AudioSystem &&) = delete;
		[[nodiscard]] auto init() -> std::expected<void, Error>;
		void shutdown();
		[[nodiscard]] bool initialized() const;
		[[nodiscard]] auto loadSound(const std::filesystem::path &path, AudioLoadMode mode = AudioLoadMode::decoded)
			-> std::expected<SoundHandle, Error>;
		[[nodiscard]] auto unloadSound(SoundHandle sound) -> std::expected<void, Error>;
		[[nodiscard]] auto play(SoundHandle sound, AudioPlaybackOptions options = {})
			-> std::expected<AudioPlaybackHandle, Error>;
		[[nodiscard]] auto stop(AudioPlaybackHandle voice) -> std::expected<void, Error>;
		[[nodiscard]] auto pause(AudioPlaybackHandle voice) -> std::expected<void, Error>;
		[[nodiscard]] auto resume(AudioPlaybackHandle voice) -> std::expected<void, Error>;
		[[nodiscard]] auto setVolume(AudioPlaybackHandle voice, AudioVolume volume) -> std::expected<void, Error>;
		[[nodiscard]] auto volume(AudioPlaybackHandle voice) const -> std::expected<AudioVolume, Error>;
		[[nodiscard]] auto setPitch(AudioPlaybackHandle voice, AudioPitch pitch) -> std::expected<void, Error>;
		[[nodiscard]] auto pitch(AudioPlaybackHandle voice) const -> std::expected<AudioPitch, Error>;
		[[nodiscard]] auto setListener(AudioListener listener) -> std::expected<void, Error>;
		[[nodiscard]] auto setSource(AudioPlaybackHandle voice, AudioSource source) -> std::expected<void, Error>;
		[[nodiscard]] auto clearSource(AudioPlaybackHandle voice) -> std::expected<void, Error>;
		[[nodiscard]] auto setMasterVolume(AudioVolume volume) -> std::expected<void, Error>;
		[[nodiscard]] AudioVolume masterVolume() const;
		void stopAll();
		[[nodiscard]] bool isPlaying(AudioPlaybackHandle voice) const;
		[[nodiscard]] bool isPaused(AudioPlaybackHandle voice) const;
		[[nodiscard]] std::string lastError() const;
		void collectFinished();

	private:
		/// @brief Keeps user pitch separate from Doppler while SDL owns mixing and track synchronization.
		struct Voice {
			SoundHandle sound{}; ///< Loaded resource retained for the voice lifetime.
			MIX_Track *track{nullptr}; ///< Independent SDL playback track.
			AudioPitch pitch{}; ///< User-selected pitch before Doppler.
			std::optional<AudioSource> source{}; ///< Placement is absent for ordinary 2D playback.
		};
		[[nodiscard]] auto fail(Error error, std::string message) const -> std::unexpected<Error>;
		[[nodiscard]] auto track(AudioPlaybackHandle voice) const -> std::expected<MIX_Track *, Error>;
		[[nodiscard]] auto applySpatial(Voice &voice, const AudioListener &listener) -> std::expected<void, Error>;
		static bool validVolume(AudioVolume volume);
		MIX_Mixer *mixer_{nullptr}; ///< Default-device mixer, also marks successful MIX_Init ownership.
		std::map<SoundHandle, MIX_Audio *> sounds_{}; ///< Resources reused by any number of tracks.
		std::map<AudioPlaybackHandle, Voice> voices_{}; ///< Live or finished voices.
		AudioListener listener_{}; ///< Common world-space listener, independent of render cameras.
		mutable std::string last_error_{}; ///< Latest diagnostic copied before SDL cleanup can change it.
	};
}
