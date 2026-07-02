import std;
import VEEngine;

/**
 * @file
 * @brief End-to-end executable for the selected forward renderer through the public facade.
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
	return 3;
}

/// @brief Returns the stable image path used by automatic render verification.
[[nodiscard]] std::filesystem::path capturePath(int argc, char **argv) {
	const auto executable_path = argc > 0 && argv[0] != nullptr ? std::filesystem::path{argv[0]} : std::filesystem::path{};
	const auto executable_directory = executable_path.has_parent_path() ? executable_path.parent_path() : std::filesystem::current_path();
	return executable_directory.parent_path() / "verify" / "simple_forward_demo_capture.png";
}

} // namespace

/**
 * @brief Exercises the selected renderer through the official facade.
 *
 * @return Zero after the configured frame count, non-zero with the failing facade stage.
 */
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::println("simple_forward_demo engine={}", vve::engineImplementationNamespaceName);

	auto engine = vve::EngineBuilder<>{}
						 .applicationName("simple_forward_demo")
						 .addWindow(vve::WindowSetup{}
										 .id("main")
										 .title("VVE Simple Forward Demo")
										 .extent(vve::PixelExtent{.width = 800, .height = 600})
										 .renderer(vve::RendererId{.value = "forward"}))
						 .build();

	if (const auto result = engine.init(); !result) {
		std::cerr << "simple_forward_demo failed: stage=engine init, error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}
	std::println("simple_forward_demo engine init");

	auto render_system = engine.world().get<vve::RenderSystem>();
	if (const auto result = render_system.loadSampleScene(); !result) {
		std::cerr << "simple_forward_demo failed: stage=sample scene load, error=" << vve::errorName(result.error()) << '\n';
		return 2;
	}
	std::println("simple_forward_demo scene loaded before frame step");

	const int max_frames = frameLimit(argc, argv);
	const std::filesystem::path capture_path = capturePath(argc, argv);
	const std::string capture_path_text = capture_path.string();
	bool captured{};

	for (int frame{}; frame < max_frames; ++frame) {
		const auto status = engine.step();
		if (!status) {
			std::cerr << "simple_forward_demo failed: stage=frame, frame=" << (frame + 1)
						 << ", error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		std::println("simple_forward_demo frame {} drawn", frame + 1);

		// Capture the first completed frame so verification observes the same point in the run.
		if (!captured) {
			const std::expected<void, vve::Error> capture_result = render_system.captureFrameToPng(capture_path);
			if (!capture_result) {
				std::cerr << "simple_forward_demo failed: stage=readback capture, error="
							 << vve::errorName(capture_result.error()) << ", path=" << capture_path_text << '\n';
				return 4;
			}
			captured = true;
			std::println("simple_forward_demo readback png_written=true, path={}", capture_path_text);
		}

		if (*status == vve::FrameStatus::stopped) { break; }
	}

	std::println("simple_forward_demo frames={}", max_frames);
	std::println("simple_forward_demo cleanup done");
	return 0;
}
