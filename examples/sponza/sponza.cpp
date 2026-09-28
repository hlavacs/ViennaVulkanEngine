import std;
import VEEngine;
import VVE.ExampleSupport;

/**
 * @file
 * @brief Sponza example shell running through the public engine facade.
 */
namespace {

constexpr auto sponzaSceneRelativePath = "assets/sea_keep_lonely_watcher/scene.gltf";

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

	const auto scene_path = vve::example::assetRoot(argc > 0 ? argv[0] : nullptr) / sponzaSceneRelativePath;
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

	const int max_frames = vve::example::frameLimit(argc, argv);
	int frame{};
	bool running = true;
	vve::DefaultCameraController cameraController{};
	cameraController.eye = vve::Position{.value = vve::Vec3{0.0F, 6.0F, 9.0F}};
	const auto startupForward =
		vve::math::normalize(vve::math::subtract(vve::Vec3{0.0F, 1.0F, 0.0F}, cameraController.eye.value));
	cameraController.yaw = std::atan2(startupForward.x, -startupForward.z);
	cameraController.pitch = std::asin(startupForward.y);
	while (running && (max_frames == 0 || frame < max_frames)) {
		const auto frameInput = engine.world().get<vve::WindowSystem>().input();
		render_system.setCamera(cameraController.update(frameInput, engine.frameContext().delta_time));

		const auto status = engine.step();
		if (!status) {
			std::cerr << "[sponza] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 4;
		}
		++frame;
		if (*status == vve::FrameStatus::stopped) { break; }

		const auto input = engine.world().get<vve::WindowSystem>().input();
		if (input.wasKeyPressed(vve::Key::escape)) { running = false; }
	}

	std::cout << "[sponza] frames=" << frame << '\n';
	return 0;
}
