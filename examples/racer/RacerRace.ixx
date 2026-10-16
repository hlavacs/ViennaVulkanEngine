/**
 * @file
 * @brief Ring Racer game state shared by both user systems, and RaceSystem: rules, countdown and the fixed-step race.
 *
 * Functional objects:
 * - Constants: physics step, countdown, car files and names, music files and the sun direction of the sky photo.
 * - Options, Mode, Phase, View: command-line options, game mode, race phase and camera view.
 * - lapText: lap-time formatting for the HUD.
 * - RaceSystem: user system that keeps one RaceCar per car in the ECS and steps the race at 120 Hz.
 */
export module Racer.Race;
import std;
import VVEngine;
export import Racer.Rules;

export namespace racer {

constexpr float physicsStep{1.0F / 120.0F};	///< Fixed simulation step.
constexpr float countdownSeconds{3.0F};		///< Lights before the start.
constexpr std::array musicFiles{"dance.mp3", "ophelia.mp3", "getout.ogg"};	///< V2 soundtrack in assets/sounds.
constexpr std::array carNames{"You", "Future", "Sports", "Hatch"};			///< Names shown in the HUD and photo mode.
constexpr std::array carFiles{"race.glb", "race-future.glb", "sedan-sports.glb", "hatchback-sports.glb"};	///< Kenney cars in assets/racer/cars.
constexpr std::array carColors{vve::Vec3{0.9F, 0.1F, 0.05F}, vve::Vec3{0.2F, 0.5F, 1.0F}, vve::Vec3{0.2F, 0.8F, 0.3F},
	vve::Vec3{1.0F, 0.8F, 0.1F}};	///< Minimap dot per car.
constexpr vve::Vec3 sunDirection{-0.365F, -0.731F, 0.576F};	///< Light from the sun in assets/racer/sky.jpg (u 0.59, v 0.24).

/// @brief Command-line options.
struct Options {
	int frames{};								///< Bounded run length; zero runs until closed.
	bool autopilot{};							///< The computer drives the player car.
	bool night{};								///< Start in night mode.
	bool menu{};								///< Stay in the main menu (bounded menu capture).
	bool timeTrial{};							///< Bounded runs start a time trial instead of a race.
	int camera{};									///< Starting camera view: 0 chase, 1 hood, 2 top.
	std::filesystem::path output{};		///< Capture of the last bounded frame.
	std::filesystem::path verify{};		///< Verification file of a bounded run.
};

/// @brief Game mode chosen in the menu.
enum class Mode { race, timeTrial };
/// @brief Phase of one race.
enum class Phase { menu, countdown, racing, finished };
/// @brief Camera views, cycled with C.
enum class View { chase, hood, top };


/// @brief Formats a lap time as m:ss.cc, or a dash before the first lap.
std::string lapText(float seconds) {
	if (seconds <= 0.0F) { return "-:--.--"; }
	return std::format("{}:{:05.2f}", static_cast<int>(seconds) / 60, std::fmod(seconds, 60.0F));
}

/// @brief Rules and simulation: the menu choice, countdown, fixed-step physics, laps and the ghost recording.
struct RaceSystem {
	Options options{};							///< Command-line options.
	racer::Track track{racer::Track::build(racer::ringCircuit())};	///< The circuit.
	racer::VehicleParams params{};			///< Car tuning shared by all cars.
	Mode mode{Mode::race};						///< Current game mode.
	Phase phase{Phase::menu};				///< Current race phase.
	int laps{3};									///< Laps per race.
	int opponents{3};							///< Computer cars in a race.
	float raceTime{};							///< Seconds since the green light.
	float countdown{};							///< Seconds left before the green light.
	float accumulator{};						///< Frame time not yet simulated.
	bool paused{};								///< Simulation stopped (pause menu or photo mode).
	bool quit{};									///< Ends the main loop.
	int generation{};							///< Increases with every new race, so the view rebuilds its cars.
	racer::Ghost ghost{};						///< Best lap of the player.
	racer::Ghost recording{};					///< Lap being driven by the player.
	float minClearance{std::numeric_limits<float>::max()};	///< Closest any car corner came to a wall in this run.
	float maxOverlap{};							///< Deepest overlap of two cars seen after a step in this run.
	bool startPending{};						///< Bounded runs start on the first update, once the view knows the car sizes.
	std::vector<std::pair<float, float>> carSizes{};	///< Collider half length and width per car slot, from the car models.
	vve::ECS *ecs{};								///< Engine-owned component store holding one RaceCar per car.

	/// @brief Keeps the ECS; bounded runs go straight into a race, others start in the menu.
	template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
		ecs = &world.template get<vve::ECS>();
		startPending = options.frames > 0 && !options.menu;
		return {};
	}

	/// @brief Replaces all cars with a new grid and starts the countdown.
	void start(Mode chosen) {
		for (const auto entity : ecs->view<racer::RaceCar>()) { (void)ecs->erase(entity); }
		mode = chosen;
		std::size_t slot{};
		for (auto car : racer::startGrid(track, mode == Mode::race ? 1 + opponents : 1, 2026U)) {
			if (!carSizes.empty()) { std::tie(car.vehicle.halfLength, car.vehicle.halfWidth) = carSizes[slot++ % carSizes.size()]; }
			(void)ecs->add(ecs->create(), car);
		}
		phase = Phase::countdown;
		countdown = countdownSeconds;
		raceTime = 0.0F;
		accumulator = 0.0F;
		paused = false;
		recording.frames.clear();
		++generation;
	}

	/// @brief Reads the keyboard into controls for the player car.
	[[nodiscard]] static racer::Controls keyboard(const vve::InputState &input) {
		const auto down = [&input](vve::Key a, vve::Key b) { return input.isKeyDown(a) || input.isKeyDown(b) ? 1.0F : 0.0F; };
		return {.throttle = down(vve::Key::w, vve::Key::up), .brake = down(vve::Key::s, vve::Key::down),
			.steer = down(vve::Key::d, vve::Key::right) - down(vve::Key::a, vve::Key::left), .handbrake = input.isKeyDown(vve::Key::space)};
	}

	/// @brief Advances the race by the frame time in fixed physics steps.
	template <typename TWorld> std::expected<void, vve::Error> update(TWorld &world, const vve::FrameContext &frame) {
		const auto input = world.template get<vve::WindowSystem>().input();
		if (startPending) { start(options.timeTrial ? Mode::timeTrial : Mode::race); startPending = false; }
		if (phase == Phase::menu || phase == Phase::finished) { return {}; }
		if (input.wasKeyPressed(vve::Key::escape)) { paused = !paused; }
		if (paused) { return {}; }
		const float frameTime = options.frames > 0 ? 1.0F / 60.0F : std::clamp(static_cast<float>(frame.delta_time.seconds), 0.0F, 0.05F);
		auto &ecs = world.template get<vve::ECS>();
		const auto entities = ecs.template view<racer::RaceCar>();
		std::vector<racer::RaceCar> cars{};
		for (const auto entity : entities) { cars.push_back(ecs.template get<racer::RaceCar>(entity).value()); }
		if (cars.empty()) { return {}; }
		// R puts the player back onto the centre line, facing forwards.
		if (input.wasKeyPressed(vve::key('r')) && !options.autopilot) {
			auto &player = cars.front().vehicle;
			const auto place = track.at(track.locate(player.position).distance);
			player = {.position = place.position, .heading = std::atan2(place.tangent.y, place.tangent.x)};
		}
		accumulator += frameTime;
		std::vector<racer::Controls> controls(cars.size());
		while (accumulator >= physicsStep) {
			accumulator -= physicsStep;
			if (phase == Phase::countdown) {
				countdown -= physicsStep;
				if (countdown <= 0.0F) { phase = Phase::racing; }
				continue;
			}
			for (std::size_t i{}; i < cars.size(); ++i) {
				const bool human = cars[i].player && !options.autopilot;
				controls[i] = cars[i].progress.finished ? racer::Controls{.brake = 1.0F}
					: human ? keyboard(input) : racer::driveAi(cars[i].vehicle, cars[i].ai, track, params);
			}
			const int lapsBefore = cars.front().progress.completedLaps;
			const float bestBefore = cars.front().progress.bestLap;
			racer::stepRace(cars, controls, track, params, raceTime, laps, physicsStep);
			raceTime += physicsStep;
			// The player's lap recording becomes the ghost when it was the best lap.
			const auto &player = cars.front();
			recording.frames.push_back({.position = player.vehicle.position, .heading = player.vehicle.heading,
				.steer = player.vehicle.steer, .wheelAngle = player.vehicle.wheelAngle});
			if (player.progress.completedLaps != lapsBefore) {
				if (player.progress.bestLap != bestBefore) { ghost = recording; }
				recording.frames.clear();
			}
			for (std::size_t i{}; i < cars.size(); ++i) {
				minClearance = std::min(minClearance, racer::wallClearance(cars[i].vehicle, track));
				for (std::size_t j{i + 1U}; j < cars.size(); ++j) {
					if (const auto contact = racer::overlap(cars[i].vehicle.box(), cars[j].vehicle.box())) { maxOverlap = std::max(maxOverlap, contact->depth); }
				}
			}
			if (player.progress.finished) { phase = Phase::finished; }
		}
		for (std::size_t i{}; i < entities.size(); ++i) { (void)ecs.put(entities[i], cars[i]); }
		return {};
	}

	/// @brief Car indices ordered by race position: finished cars by time, then by distance covered.
	[[nodiscard]] std::vector<std::size_t> standings(const std::vector<racer::RaceCar> &cars) const {
		std::vector<std::size_t> order(cars.size());
		std::iota(order.begin(), order.end(), 0U);
		std::ranges::stable_sort(order, [&](std::size_t a, std::size_t b) {
			const auto &pa = cars[a].progress, &pb = cars[b].progress;
			if (pa.finished != pb.finished) { return pa.finished; }
			if (pa.finished) { return pa.finishTime < pb.finishTime; }
			return pa.total(track.length()) > pb.total(track.length());
		});
		return order;
	}
};

} // namespace racer
