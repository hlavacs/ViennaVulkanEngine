/** @file @brief Island Air: a Full HD Vulkan-backed VVE example with deterministic flight and landing deliveries. */
import std;
import VVEngine;
import Island.Game;

namespace {

/** @brief Reads a positive integer without exceptions or silently accepting malformed command-line values. */
bool positive(std::string_view value, int &output) {
	const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
	return result.ec == std::errc{} && result.ptr == value.data() + value.size() && output > 0;
}

/** @brief Documents interactive controls and bounded capture options before opening a window or audio device. */
void usage() {
	std::println("Island Air - VVE island cargo delivery\n"
		"  W/S throttle; arrows pitch/bank; A/D rudder; Space brakes\n"
		"  C camera; P autopilot; R restart; Esc pause; F11 fullscreen\n"
		"  --autopilot --mute --hidden --no-msaa --require-delivery\n"
		"  --frames N --steps-per-frame N --width N --height N\n"
		"  --camera chase|cockpit|showcase --output image.png --verify flight.csv\n"
		"  --asset-dir DIRECTORY (otherwise configure-time downloads beside the executable)");
}

} // namespace

/** @brief Runs a facade-only game; bounded runs save an image and compact, automatically checkable flight telemetry. */
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	island::GameSystem game{};
	int width{1920}, height{1080};
	bool hidden{}, requireDelivery{};
	std::filesystem::path output{}, verification{};
	const auto executable = std::filesystem::absolute(argc > 0 ? argv[0] : "island_delivery");
	game.assetRoot = executable.parent_path() / "assets/island_delivery";
	if (!std::filesystem::is_directory(game.assetRoot)) { game.assetRoot = VVE_ISLAND_ASSET_DIR; }
	// Reject unrecognized arguments so a misspelled verification option cannot become an endless interactive run.
	for (int index{1}; index < argc; ++index) {
		const std::string_view argument{argv[index]};
		if (argument == "--help" || argument == "-h") { usage(); return 0; }
		if (argument == "--autopilot") { game.automatic = true; continue; }
		if (argument == "--mute") { game.muted = true; continue; }
		if (argument == "--hidden") { hidden = true; continue; }
		if (argument == "--no-msaa") { game.antialias = false; continue; }
		if (argument == "--require-delivery") { requireDelivery = true; continue; }
		if (index + 1 >= argc) { std::cerr << "[island] missing value: " << argument << '\n'; return 1; }
		const std::string_view value{argv[++index]};
		const std::array integers{std::pair{"--frames", &game.frameLimit}, std::pair{"--steps-per-frame", &game.stepsPerFrame},
			std::pair{"--width", &width}, std::pair{"--height", &height}};
		const auto integer = std::ranges::find_if(integers, [argument](const auto &entry) { return argument == entry.first; });
		if (integer != integers.end()) {
			if (!positive(value, *integer->second)) { std::cerr << "[island] invalid positive integer: " << argument << '\n'; return 1; }
			continue;
		}
		if (argument == "--output") { output = value; continue; }
		if (argument == "--verify") { verification = value; continue; }
		if (argument == "--asset-dir") { game.assetRoot = value; continue; }
		if (argument == "--camera") {
			constexpr std::array names{"chase", "cockpit", "showcase"};
			const auto found = std::ranges::find(names, value);
			if (found == names.end()) { std::cerr << "[island] invalid camera\n"; return 1; }
			game.cameraMode = static_cast<int>(found - names.begin());
			continue;
		}
		std::cerr << "[island] unknown option: " << argument << '\n'; return 1;
	}
	if (game.stepsPerFrame > 1200 || width > 7680 || height > 4320 || (hidden && game.frameLimit == 0)
		|| (requireDelivery && game.frameLimit == 0)) {
		std::cerr << "[island] invalid bounds: hidden/verification runs need --frames; maximum 1200 steps/frame and 7680x4320\n";
		return 1;
	}
	if (game.frameLimit > 0) {
		const auto directory = executable.parent_path().parent_path() / "verify/island_delivery";
		if (output.empty()) { output = directory / "island_delivery.png"; }
		if (verification.empty()) { verification = directory / "island_delivery.csv"; }
	}
	std::println("[island] resolution={}x{} assets={} autopilot={}", width, height, game.assetRoot.string(), game.automatic);
	auto engine = vve::EngineBuilder<island::GameSystem>{vve::makeUserSystems(std::move(game))}
		.applicationName("Island Air")
		.addWindow(vve::WindowSetup{}.id("main").title("Island Air - VVE Cargo Delivery")
			.extent(vve::PixelExtent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)}).visible(!hidden))
		.build();
	if (const auto result = engine.init(); !result) {
		std::cerr << "[island] stage=init error=" << vve::errorName(result.error()) << '\n'; return 2;
	}
	auto world = engine.world();
	auto &session = world.get<island::GameSystem>();
	auto &render = world.get<vve::RenderSystem>();
	std::ofstream report{};
	if (!verification.empty()) {
		if (!verification.parent_path().empty()) { std::filesystem::create_directories(verification.parent_path()); }
		report.open(verification);
		if (!report) { std::cerr << "[island] cannot write verification file\n"; return 3; }
		report << "frame,seconds,x,y,z,speed,throttle,grounded,takeoffs,landings,phase\n";
	}
	int rendered{};
	// All mission state evolves inside the user system; the application only controls lifetime and verification.
	while (!session.quit && (session.frameLimit == 0 || rendered < session.frameLimit)) {
		const auto result = engine.step();
		if (!result) {
			std::cerr << "[island] stage=frame frame=" << rendered << " error=" << vve::errorName(result.error()) << '\n'; return 4;
		}
		if (*result == vve::FrameStatus::stopped) { break; }
		++rendered;
		const auto &flight = session.displayed;
		if (report) {
			report << std::format("{},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{},{},{},{}\n", rendered, flight.elapsed,
				flight.position.x, flight.position.y, flight.position.z, vve::math::length(flight.velocity), flight.throttle,
				flight.grounded, flight.takeoffs, flight.landings, static_cast<int>(flight.phase));
		}
	}
	if (session.frameLimit == 0) { return 0; }
	if (!output.parent_path().empty()) { std::filesystem::create_directories(output.parent_path()); }
	if (const auto captured = render.captureFrameToPng(output); !captured) {
		std::cerr << "[island] stage=capture error=" << vve::errorName(captured.error()) << '\n'; return 5;
	}
	const auto &flight = session.displayed;
	const bool passed = flight.phase != island::Phase::crashed && rendered == session.frameLimit
		&& (!requireDelivery || (flight.phase == island::Phase::delivered && flight.takeoffs > 0 && flight.landings > 0));
	const auto summary = std::format("passed={} delivered={} takeoffs={} landings={} max_altitude={:.3f} loaded_sounds={} audio_unavailable={} width={} height={} objects={} meshes={} textures={}\n",
		passed, flight.phase == island::Phase::delivered, flight.takeoffs, flight.landings, flight.maxAltitude,
		session.sound.loadedSounds, session.sound.deviceUnavailable, width, height, render.sceneInstanceCount(), render.sceneMeshCount(), render.sceneTextureCount());
	if (report) { report << summary; }
	std::cout << "[island] " << summary << "[island] capture=" << output << " verification=" << verification << '\n';
	return passed ? 0 : 6;
}
