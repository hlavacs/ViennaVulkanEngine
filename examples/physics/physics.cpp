import std;
import VEEngine;

/**
 * @file
 * @brief Minimal physics shell running through the public engine facade.
 */
namespace {

/// @brief Reads the optional frame count used by automated example runs.
[[nodiscard]] int frameLimit(int argc, char **argv) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] == nullptr || argv[index + 1] == nullptr) {
			continue;
		}
		if (std::string_view{argv[index]} != "--frames") {
			continue;
		}
		int value{};
		const std::string_view text{argv[index + 1]};
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec == std::errc{} && value >= 0) {
			return value;
		}
	}
	return 1;
}

} // namespace

int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[physics] engine=" << vve::engineImplementationNamespaceName << '\n';

	auto engine = vve::EngineBuilder<>{}
						 .applicationName("physics")
						 .addWindow(vve::WindowSetup{}
										 .id("main")
										 .title("VVE Physics")
										 .extent(vve::PixelExtent{.width = 800, .height = 450})
										 .renderer(vve::RendererId{.value = "forward"}))
						 .build();

	if (const auto result = engine.init(); !result) {
		std::cerr << "[physics] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto render_system = engine.world().get<vve::RenderSystem>();
	if (const auto result = render_system.loadSampleScene(); !result) {
		std::cerr << "[physics] sample scene load failed: error=" << vve::errorName(result.error()) << '\n';
		return 2;
	}

	const int max_frames = frameLimit(argc, argv);
	for (int frame{}; frame < max_frames; ++frame) {
		const auto status = engine.step();
		if (!status) {
			std::cerr << "[physics] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		if (*status == vve::FrameStatus::stopped) { break; }
	}

	std::cout << "[physics] frames=" << max_frames << '\n';
	return 0;
}
