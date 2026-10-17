/** @file @brief Aircraft mixing contracts and actual downloaded recording playback through VVE's public audio API. */
import std;
import VVEngine;
import Island.Sound;
import Island.Logic;

namespace {
int checks{}; ///< Successful assertions emitted for automatic diagnosis.

/** @brief Names failed expectations so recorded output identifies the broken mixing rule. */
void check(bool condition, std::string_view name) {
	if (!condition) { throw std::runtime_error(std::string{name}); }
	++checks;
}

/** @brief Exercises RPM, airspeed, cockpit and pause mixing with no sound device or timing dependence. */
void mixing() {
	island::Soundscape sound;
	island::Flight flight;
	auto camera = vve::Camera::lookAt(vve::Position{{0, 10, 25}}, vve::Position{{0, 5, 0}});
	check(sound.update(flight, camera, false, false).has_value(), "initial_mix");
	const float idlePitch = sound.pitches[0];
	check(sound.gains[0] > 0 && sound.gains[1] == 0 && sound.gains[2] == 0, "idle_exterior_layers");
	flight.throttle = 1;
	check(sound.update(flight, camera, false, false).has_value(), "full_power_mix");
	check(sound.gains[1] > sound.gains[0] && sound.pitches[0] > idlePitch, "rpm_changes_crossfade_and_pitch");
	const float exterior = sound.gains[1];
	check(sound.update(flight, camera, true, false).has_value(), "cockpit_mix");
	check(sound.gains[2] > 0 && sound.gains[1] < exterior * 0.2F, "cockpit_replaces_external_propeller");
	flight.grounded = false;
	flight.velocity = {0, 0, -50};
	check(sound.update(flight, camera, false, false).has_value(), "airborne_mix");
	check(sound.gains[3] > 0 && sound.gains[5] == 0, "airflow_without_wheels");
	flight.grounded = true;
	check(sound.update(flight, camera, false, false).has_value() && sound.gains[5] > 0, "rolling_wheels");
	check(sound.update(flight, camera, false, true).has_value(), "pause_mix");
	check(std::ranges::all_of(sound.gains, [](float gain) { return gain == 0; }), "pause_is_silent");
	flight.phase = island::Phase::crashed;
	check(sound.update(flight, camera, true, false).has_value(), "crash_mix");
	check(sound.gains[0] == 0 && sound.gains[1] == 0 && sound.gains[2] == 0, "crash_cuts_engine");
}

/** @brief Decodes every configured download, starts loops, and verifies their real pause/resume and effect handles. */
void playback() {
	vve::Engine<> engine;
	auto world = engine.world();
	auto &audio = world.get<vve::AudioSystem>();
	island::Soundscape sound;
	const auto initialized = sound.init(audio, VVE_ISLAND_ASSET_DIR, false);
	check(initialized.has_value() && sound.audio != nullptr && sound.loadedSounds == 8,
		std::string{"decode_eight_downloaded_recordings: "} + audio.lastError());
	island::Flight flight;
	auto camera = vve::Camera::lookAt(vve::Position{{0, 10, 25}}, vve::Position{{0, 5, 0}});
	check(sound.update(flight, camera, false, false).has_value(), "play_all_layers");
	for (const auto voice : sound.loops) { check(audio.isPlaying(voice), "loop_is_playing"); }
	check(sound.update(flight, camera, true, true).has_value(), "pause_actual_voices");
	for (const auto voice : sound.loops) { check(audio.isPaused(voice), "loop_cursor_paused"); }
	flight.elapsed = 1;
	flight.landings = 1;
	flight.phase = island::Phase::landed;
	check(sound.update(flight, camera, true, false).has_value(), "resume_and_touchdown");
	for (const auto voice : sound.loops) { check(audio.isPlaying(voice), "loop_resumed"); }
	check(sound.effects[0].valid(), "touchdown_played_once");
	const auto touchdown = sound.effects[0];
	flight.elapsed = 2;
	check(sound.update(flight, camera, false, false).has_value() && sound.effects[0] == touchdown, "no_repeated_touchdown");
	flight.phase = island::Phase::delivered;
	check(sound.update(flight, camera, false, false).has_value() && sound.effects[2].valid(), "cargo_unloading_recording");
	audio.stopAll();
}
} // namespace

/** @brief Produces deterministic rule counts and exits nonzero on any decoding or playback failure. */
int main() {
	try {
		mixing();
		playback();
		std::println("[IslandDeliverySoundTests] passed=true checks={} recordings=8", checks);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "[IslandDeliverySoundTests] passed=false checks=" << checks << " error=" << error.what() << '\n';
		return 1;
	}
}
