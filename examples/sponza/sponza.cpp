import std;
import VEEngine;

/**
 * @file
 * @brief Sponza example shell running through the public engine facade.
 */
namespace {

constexpr auto sponzaSceneRelativePath = "assets/sea_keep_lonely_watcher/scene.gltf";

/// @brief Finds the repository-style asset root from either the cwd or executable location.
[[nodiscard]] std::filesystem::path assetRoot(char *argv0) {
	auto containsSponzaScene = [](const std::filesystem::path &candidate) {
		return std::filesystem::exists(candidate / sponzaSceneRelativePath);
	};
	if (const auto cwd = std::filesystem::current_path(); containsSponzaScene(cwd)) {
		return cwd;
	}
	if (argv0 == nullptr) {
		return {};
	}
	auto executable = std::filesystem::absolute(std::filesystem::path{argv0});
	if (std::filesystem::exists(executable)) {
		executable = std::filesystem::weakly_canonical(executable);
	}
	for (auto candidate = executable.parent_path(); !candidate.empty(); candidate = candidate.parent_path()) {
		if (containsSponzaScene(candidate)) {
			return candidate;
		}
		if (candidate == candidate.root_path()) {
			break;
		}
	}
	return {};
}

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
	std::cout << "[sponza] engine=" << vve::engineImplementationNamespaceName << '\n';

	auto engine = vve::EngineBuilder<>{}
						 .applicationName("sponza")
						 .addWindow(vve::WindowSetup{}
										 .id("main")
										 .title("VVE Sponza")
										 .extent(vve::PixelExtent{.width = 1280, .height = 720})
										 .renderer(vve::RendererId{.value = "forward"}))
						 .build();

	if (const auto result = engine.init(); !result) {
		std::cerr << "[sponza] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto assets = engine.world().get<vve::AssetSystem>();
	auto render_system = engine.world().get<vve::RenderSystem>();

	const auto scene_path = assetRoot(argc > 0 ? argv[0] : nullptr) / sponzaSceneRelativePath;
	const std::expected<vve::SceneHandle, vve::Error> scene = assets.loadScene(scene_path);
	if (!scene) {
		std::cerr << "[sponza] scene load failed: path=" << scene_path << " error=" << vve::errorName(scene.error()) << '\n';
		return 2;
	}

	const vve::SceneInstantiationOptions options{}; ///< Default bridge options instantiate imported geometry.
	const std::expected<vve::RenderSceneInstanceHandle, vve::Error> instance = render_system.instantiateScene(*scene, options);
	if (!instance) {
		std::cerr << "[sponza] scene instantiation failed: error=" << vve::errorName(instance.error()) << '\n';
		return 3;
	}
	std::cout << "[sponza] scene=" << scene->value << " instance=" << instance->value << '\n';

	const int max_frames = frameLimit(argc, argv);
	for (int frame{}; frame < max_frames; ++frame) {
		const auto status = engine.step();
		if (!status) {
			std::cerr << "[sponza] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 4;
		}
		if (*status == vve::FrameStatus::stopped) { break; }
	}

	std::cout << "[sponza] frames=" << max_frames << '\n';
	return 0;
}
