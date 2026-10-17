import std;
import VVEngine;
import VVE.ExampleSupport;
import Racer.Race;
import Racer.View;
import Racer.Scene;
import Racer.Sound;

/**
 * @file
 * @brief Ring Racer: race three computer cars for three laps, or chase the ghost of your best lap.
 *
 * The game runs as three user systems on the facade engine. RaceSystem owns the rules and the fixed-step simulation and
 * keeps one racer::RaceCar component per car in the ECS. ViewSystem reads those components and drives everything the
 * player sees: circuit and countryside, car rigs, camera, lights, smoke and HUD. SoundSystem plays the music and the
 * engines, tyres, crashes, race signals, birds and crowd.
 *
 * Controls: W/S or Up/Down throttle and brake, A/D or Left/Right steer, Space handbrake, C camera, R back to the
 * track, N night, P photo mode (hold the right mouse button to orbit, click a car), F11 fullscreen, Esc pause.
 * Options: --frames N (bounded run, fixed 60 Hz frames, capture and verify at the end), --autopilot (the computer
 * drives the player car), --night, --menu (stay in the main menu), --time-trial, --camera chase|hood|top, --output <png>,
 * --verify <txt>.
 */
namespace {

/// @brief Reads the value after an option, or returns a fallback.
std::filesystem::path option(int argc, char **argv, std::string_view name, std::filesystem::path fallback) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (std::string_view{argv[index]} == name) { return argv[index + 1]; }
	}
	return fallback;
}

} // namespace

/// @brief Parses options, runs the frame loop and, for bounded runs, writes the capture and the verification file.
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	racer::Options options{.frames = vve::example::frameLimit(argc, argv)};
	for (int i = 1; i < argc; ++i) {
		const std::string_view argument{argv[i]};
		options.autopilot = options.autopilot || argument == "--autopilot";
		options.night = options.night || argument == "--night";
		options.menu = options.menu || argument == "--menu";
		options.timeTrial = options.timeTrial || argument == "--time-trial";
	}
	const auto camera = option(argc, argv, "--camera", "chase").string();
	options.camera = camera == "hood" ? 1 : camera == "top" ? 2 : 0;
	const auto verifyDir = std::filesystem::path{argc > 0 ? argv[0] : "."}.parent_path().parent_path() / "verify";
	options.output = option(argc, argv, "--output", verifyDir / "racer.png");
	options.verify = option(argc, argv, "--verify", verifyDir / "racer.txt");
	const auto root = vve::example::assetRoot(argc > 0 ? argv[0] : nullptr, "assets/racer/cars/race.glb");

	auto engine = vve::EngineBuilder<racer::RaceSystem, racer::ViewSystem, racer::SoundSystem>{
		vve::makeUserSystems(racer::RaceSystem{.options = options}, racer::ViewSystem{.root = root}, racer::SoundSystem{.root = root})}
		.applicationName("racer")
		.addWindow(vve::WindowSetup{}.id("main").title("VVE Ring Racer").extent(vve::PixelExtent{.width = 1280, .height = 720}))
		.build();
	auto world = engine.world();
	auto &view = world.get<racer::ViewSystem>();
	if (const auto result = engine.init(); !result) {
		std::cerr << "[racer] failed: stage=init, error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}
	auto &race = world.get<racer::RaceSystem>();
	auto &render = world.get<vve::RenderSystem>();
	auto &ecs = world.get<vve::ECS>();

	std::ofstream verify{};
	if (options.frames > 0) {
		std::filesystem::create_directories(options.verify.parent_path());
		verify.open(options.verify);
		verify << "frame,phase,player_x,player_z,speed,lap,distance,position\n";
	}
	for (int frame{}; !race.quit && (options.frames == 0 || frame < options.frames); ++frame) {
		const auto status = engine.step();
		if (!status) {
			std::cerr << "[racer] failed: stage=frame, frame=" << frame << ", error=" << vve::errorName(status.error()) << '\n';
			return 2;
		}
		if (*status == vve::FrameStatus::stopped) { break; }
		if (verify.is_open() && frame % 60 == 0 && !view.cars.empty()) {
			const auto &player = view.cars.front();
			const auto order = race.standings(view.cars);
			verify << std::format("{},{},{:.3f},{:.3f},{:.3f},{},{:.2f},{}\n", frame, static_cast<int>(race.phase), player.vehicle.position.x,
				player.vehicle.position.y, player.vehicle.forwardSpeed(), player.progress.completedLaps, player.progress.distance,
				std::ranges::find(order, 0U) - order.begin() + 1);
		}
	}
	if (options.frames == 0) { return 0; }

	// Bounded run: capture, then check that every car drove, none entered a wall, and picking finds the player.
	if (const auto result = render.captureFrameToPng(options.output); !result) {
		std::cerr << "[racer] failed: stage=capture, error=" << vve::errorName(result.error()) << '\n';
		return 3;
	}
	std::size_t moving{};
	for (const auto entity : ecs.view<racer::RaceCar>()) {
		const auto car = ecs.get<racer::RaceCar>(entity).value();
		moving += car.progress.total(race.track.length()) > 50.0F ? 1U : 0U;
	}
	bool pickedPlayer = options.menu;
	if (!view.cars.empty() && !view.rigs.empty()) {
		const auto target = racer::world(view.cars.front().vehicle.position, 0.5F);
		// Look down from 4 m, below the checkpoint gate bars, so only the car can be in the way.
		render.setCamera(vve::Camera::lookAt(vve::Position{.value = vve::math::add(target, vve::Vec3{0.0F, 4.0F, 0.01F})}, vve::Position{.value = target}));
		const auto ray = render.screenRay(view.window, vve::Vec2{0.5F, 0.5F});
		const auto hit = ray ? render.raycast(ray->first, ray->second, 100.0F) : std::nullopt;
		// On autopilot the ghost drives exactly the player's line, so its equal box may be the one hit.
		pickedPlayer = hit && (std::ranges::contains(view.rigs.begin()->second.parts, hit->object) ||
			std::ranges::contains(view.ghostRig.parts, hit->object));
	}
	// The front-left wheel must point along heading + steering angle. Its axle (+X) does not spin, so the rolling
	// direction is the axle crossed with up.
	float steerError{1.0F}, steerAngle{};	// Stays failed when no wheel part is found.
	if (!view.cars.empty() && !view.rigs.empty()) {
		const auto &player = view.cars.front().vehicle;
		steerAngle = player.steer;
		for (const auto part : view.rigs.begin()->second.parts) {
			if (render.objectSourceNode(part) != view.models.front().wheels[0]) { continue; }
			const auto q = render.objectTransform(part).value_or(vve::Transform{}).rotation.value;
			const vve::Vec3 axis{q.x, q.y, q.z}, x{1, 0, 0};
			const auto t = vve::math::scale(vve::math::cross(axis, x), 2.0F);
			const auto axle = vve::math::add(vve::math::add(x, vve::math::scale(t, q.w)), vve::math::cross(axis, t));
			const vve::Vec3 pointing{-axle.z, 0.0F, axle.x};
			const float expected = player.heading + player.steer;
			steerError = std::abs(std::remainder(std::atan2(pointing.z, pointing.x) - expected, 2.0F * std::numbers::pi_v<float>));
		}
	}
	const bool insideWalls = race.minClearance >= -0.01F && race.maxOverlap <= 0.05F;
	const bool drove = options.menu || moving == view.cars.size();
	// A time trial must have recorded its first lap as the ghost.
	const bool ghostReady = !options.timeTrial || (!race.ghost.empty() && view.ghostRig.instance.valid());
	const bool pass = insideWalls && drove && pickedPlayer && ghostReady && (options.menu || steerError < 0.01F) && view.sceneryCount > 300U &&
		view.countryPieces > 20U && view.spectators > 150U;
	verify << std::format("cars,{}\nmoving_cars,{}\nmin_wall_clearance,{:.3f}\nmax_car_overlap,{:.3f}\npicked_player,{}\nsmoke_active,{}\nghost_frames,{}\nbest_lap,{:.3f}\nsteer,{:.4f}\nfront_wheel_error,{:.5f}\nscenery,{}\ncountryside,{}\nspectators,{}\nresult,{}\n",
		view.cars.size(), moving, race.minClearance, race.maxOverlap, pickedPlayer, view.smoke.active(), race.ghost.frames.size(),
		view.cars.empty() ? 0.0F : view.cars.front().progress.bestLap, steerAngle, steerError, view.sceneryCount, view.countryPieces, view.spectators,
		pass ? "PASS" : "FAIL");
	std::println("[racer] frames={} cars={} moving={} min_wall_clearance={:.3f} max_car_overlap={:.3f} picked_player={} {}", options.frames,
		view.cars.size(), moving, race.minClearance, race.maxOverlap, pickedPlayer, pass ? "PASS" : "FAIL");
	return pass ? 0 : 10;
}
