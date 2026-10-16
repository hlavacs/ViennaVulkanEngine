/**
 * @file
 * @brief Ring Racer SoundSystem: music, engines that rise with speed, tyre squeal, crashes, race signals, menu clicks,
 * birdsong and the grandstand crowd.
 *
 * Kenney sounds (assets/racer/kenney, CC0, added by hand) and the generated signals and crowd (assets/racer/sounds,
 * written by make_sounds.sh) are optional: a sound that does not load stays silent. Every sound except the music fades
 * with the distance from the camera, which the ViewSystem reports each frame as `listener`.
 *
 * Functional objects:
 * - enginePitch: playback speed of the engine sample for a car speed, through five gears.
 * - SoundSystem: user system that loads, starts and steers all voices from the race state.
 */
export module Racer.Sound;
import std;
import VVEngine;
import Racer.Race;
import Racer.World;

export namespace racer {

/// @brief Engine sample speed for a car speed: each of five gears sweeps upwards, and an upshift drops the pitch again.
[[nodiscard]] inline float enginePitch(float speed) {
	constexpr std::array tops{12.0F, 21.0F, 30.0F, 39.0F, 52.0F};	// Gear limits in m/s.
	float low{};
	for (std::size_t gear{}; gear < tops.size(); ++gear) {
		if (speed < tops[gear] || gear + 1U == tops.size()) {
			const float rev = std::clamp((speed - low) / (tops[gear] - low), 0.0F, 1.0F);
			return 0.7F + 0.1F * static_cast<float>(gear) + 0.75F * rev;
		}
		low = tops[gear];
	}
	return 1.0F;
}

/// @brief Everything heard in the game.
struct SoundSystem {
	std::filesystem::path root{};				///< Asset root.
	vve::Vec3 listener{};						///< Camera position, set by the ViewSystem every frame.
	bool night{};								///< Night: the birds are quieter.
	int track{};								///< Selected music track.
	float volume{0.35F};						///< Master volume.
	vve::AudioSystem *audio{};				///< Null without an audio device; the game is then silent.

	/// @brief Loaded sounds; an invalid handle is a file that did not load.
	struct Sounds {
		vve::SoundHandle engine, skid, impact, ambience, click, crowd, beep, go, lap, finish;
	} sounds{};
	std::array<vve::SoundHandle, musicFiles.size()> songs{};	///< Music tracks.
	vve::AudioPlaybackHandle music{}, skid{}, ambience{}, crowd{};	///< Looping voices.
	std::vector<vve::AudioPlaybackHandle> engines{};			///< One looping engine per car of the current race.
	std::vector<std::uint64_t> lastImpact{};					///< Frame of each car's last crash sound.
	vve::Vec3 crowdSpot{};											///< Where the grandstand crowd sits.
	int generation{-1};											///< Race the engines belong to.
	int lastCount{};												///< Countdown number last beeped.
	int lastLaps{};												///< Player laps already chimed.
	bool finishPlayed{};											///< The finish fanfare of this race has played.
	Phase lastPhase{Phase::menu};								///< Phase of the previous frame.
	std::uint64_t frames{};										///< Frames since start.

	/// @brief Opens the audio device and loads all sounds; without a device the game stays silent.
	template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
		auto &device = world.template get<vve::AudioSystem>();
		if (!device.init()) { return {}; }
		audio = &device;
		for (std::size_t i{}; i < musicFiles.size(); ++i) {
			songs[i] = audio->loadSound(root / "assets/sounds" / musicFiles[i], vve::AudioLoadMode::on_demand).value_or(vve::SoundHandle{});
		}
		const auto load = [this](const char *file) { return audio->loadSound(root / "assets/racer" / file).value_or(vve::SoundHandle{}); };
		sounds = {.engine = load("kenney/racing/audio/engine.ogg"), .skid = load("kenney/racing/audio/skid.ogg"),
			.impact = load("kenney/racing/audio/impact.ogg"), .ambience = load("kenney/city/sounds/ambience.ogg"),
			.click = load("kenney/city/sounds/placement-a.ogg"), .crowd = load("sounds/crowd.wav"), .beep = load("sounds/beep.wav"),
			.go = load("sounds/go.wav"), .lap = load("sounds/lap.wav"), .finish = load("sounds/finish.wav")};
		const auto spot = grandstandCentre(world.template get<RaceSystem>().track);
		crowdSpot = vve::Vec3{spot.x, 3.0F, spot.y};
		applyVolume();
		playMusic();
		skid = loop(sounds.skid);
		ambience = loop(sounds.ambience);
		crowd = loop(sounds.crowd);
		return {};
	}

	/// @brief Starts the selected music track in a loop.
	void playMusic() {
		if (audio == nullptr || !songs[static_cast<std::size_t>(track)].valid()) { return; }
		if (music.valid()) { (void)audio->stop(music); }
		music = audio->play(songs[static_cast<std::size_t>(track)], vve::AudioPlaybackOptions{.loop = true, .volume = {.value = 0.7F}})
			.value_or(vve::AudioPlaybackHandle{});
	}

	/// @brief Applies the master volume slider.
	void applyVolume() {
		if (audio != nullptr) { (void)audio->setMasterVolume(vve::AudioVolume{.value = volume}); }
	}

	/// @brief Short click for a menu button.
	void click() { play(sounds.click, 0.6F); }

	/// @brief Steers every voice from the race state: engines, squeal, crashes, signals, birds and crowd.
	template <typename TWorld> std::expected<void, vve::Error> update(TWorld &world, const vve::FrameContext &) {
		if (audio == nullptr) { return {}; }
		++frames;
		auto &race = world.template get<RaceSystem>();
		auto &ecs = world.template get<vve::ECS>();
		std::vector<RaceCar> cars{};
		for (const auto entity : ecs.template view<RaceCar>()) { cars.push_back(ecs.template get<RaceCar>(entity).value()); }
		const bool driving = !cars.empty() && race.phase != Phase::menu && !race.paused;
		const auto closeness = [this](Vec2f position, float range) {
			const float distance = vve::math::length(vve::math::subtract(listener, vve::Vec3{position.x, 1.0F, position.y}));
			return 1.0F / (1.0F + distance / range);
		};
		// A new race gets one engine per car, and the race signals start again.
		if (generation != race.generation) {
			for (const auto voice : engines) { (void)audio->stop(voice); }
			engines.clear();
			for (std::size_t i{}; i < cars.size(); ++i) { engines.push_back(loop(sounds.engine)); }
			lastImpact.assign(cars.size(), 0U);
			lastLaps = 0;
			finishPlayed = false;
			generation = race.generation;
		}
		for (std::size_t i{}; i < cars.size() && i < engines.size(); ++i) {
			const auto &car = cars[i].vehicle;
			set(engines[i], driving ? 0.9F * closeness(car.position, 12.0F) : 0.0F, enginePitch(std::abs(car.forwardSpeed())));
			// Crashes: the strongest impact of the frame, at most one sound per car every quarter second.
			const float impact = i < race.impacts.size() ? race.impacts[i] : 0.0F;
			if (driving && impact > 2.0F && frames - lastImpact[i] > 15U) {
				play(sounds.impact, std::clamp(impact / 12.0F, 0.2F, 1.0F) * closeness(car.position, 15.0F), 0.85F + 0.03F * static_cast<float>(frames % 10U));
				lastImpact[i] = frames;
			}
		}
		// The player's tyres squeal while sliding on asphalt or braking hard.
		if (driving) {
			const auto &player = cars.front().vehicle;
			const float slide = std::clamp((player.slip - 1.5F) / 5.0F, 0.0F, 1.0F);
			const float squeal = player.offTrack ? 0.0F : std::max(slide, player.braking && player.forwardSpeed() > 15.0F ? 0.35F : 0.0F);
			set(skid, 1.2F * squeal * closeness(player.position, 15.0F), 0.9F + 0.2F * slide);
		} else {
			set(skid, 0.0F, 1.0F);
		}
		// Race signals: a beep for every countdown number, the start tone, a chime per lap (higher for a best lap), the fanfare.
		if (race.phase == Phase::countdown) {
			const int count = static_cast<int>(std::ceil(race.countdown));
			if (count != lastCount && count > 0) { play(sounds.beep, 0.7F); }
			lastCount = count;
		} else {
			lastCount = 0;
		}
		if (lastPhase == Phase::countdown && race.phase == Phase::racing) { play(sounds.go, 0.8F); }
		if (!cars.empty()) {
			const auto &progress = cars.front().progress;
			if (progress.completedLaps > lastLaps && progress.completedLaps > 0 && !progress.finished) {
				play(sounds.lap, 0.6F, progress.lastLap == progress.bestLap ? 1.25F : 1.0F);
			}
			lastLaps = std::max(lastLaps, progress.completedLaps);
			if (progress.finished && !finishPlayed) { play(sounds.finish, 0.8F); finishPlayed = true; }
		}
		lastPhase = race.phase;
		// Birds everywhere (fewer at night) and the crowd near the grandstand.
		set(ambience, night ? 0.12F : 0.45F, 1.0F);
		const float toCrowd = vve::math::length(vve::math::subtract(listener, crowdSpot));
		set(crowd, 0.8F / (1.0F + toCrowd / 25.0F), 1.0F);
		return {};
	}

	/// @brief Starts a silent looping voice; an unloaded sound gives an invalid voice.
	vve::AudioPlaybackHandle loop(vve::SoundHandle sound) {
		if (audio == nullptr || !sound.valid()) { return {}; }
		return audio->play(sound, vve::AudioPlaybackOptions{.loop = true, .volume = {.value = 0.0F}}).value_or(vve::AudioPlaybackHandle{});
	}

	/// @brief Plays a sound once at a volume and pitch.
	void play(vve::SoundHandle sound, float gain, float pitch = 1.0F) {
		if (audio == nullptr || !sound.valid()) { return; }
		const auto voice = audio->play(sound, vve::AudioPlaybackOptions{.volume = {.value = std::clamp(gain, 0.0F, 1.0F)}});
		if (voice && pitch != 1.0F) { (void)audio->setPitch(*voice, vve::AudioPitch{.value = pitch}); }
	}

	/// @brief Sets the volume and pitch of a looping voice.
	void set(vve::AudioPlaybackHandle voice, float gain, float pitch) {
		if (audio == nullptr || !voice.valid()) { return; }
		(void)audio->setVolume(voice, vve::AudioVolume{.value = std::clamp(gain, 0.0F, 1.0F)});
		(void)audio->setPitch(voice, vve::AudioPitch{.value = std::clamp(pitch, 0.01F, 100.0F)});
	}
};

} // namespace racer
