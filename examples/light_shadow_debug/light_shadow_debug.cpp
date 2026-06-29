import std;
import VEEngine;

/**
 * @file
 * @brief Deterministic facade-renderer smoke scene for light/shadow example wiring.
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

/// @brief Returns the stable text path used by automatic render verification.
[[nodiscard]] std::filesystem::path outputPath(int argc, char **argv) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] != nullptr && argv[index + 1] != nullptr && std::string_view{argv[index]} == "--output") {
			return std::filesystem::path{argv[index + 1]};
		}
	}
	if (argc > 0 && argv[0] != nullptr) {
		const auto executableDirectory = std::filesystem::absolute(std::filesystem::path{argv[0]}).parent_path();
		return executableDirectory.parent_path() / "verify" / "light_shadow_debug.txt";
	}
	return std::filesystem::path{"verify"} / "light_shadow_debug.txt";
}

/// @brief Returns the stable image path next to the verification text file.
[[nodiscard]] std::filesystem::path pngOutputPath(const std::filesystem::path &textPath) {
	const auto baseDirectory = textPath.has_parent_path() ? textPath.parent_path() : std::filesystem::current_path();
	const auto verifyDirectory = baseDirectory.filename() == "verify" ? baseDirectory : baseDirectory / "verify";
	return verifyDirectory / "light_shadow_debug.png";
}

/// @brief Adds the debug floor, shadow caster, light marker, and lights through the facade.
[[nodiscard]] std::expected<void, vve::Error> loadShadowTestScene(vve::RenderSystem renderSystem) {
	const vve::Vec3 lightPosition{2.0F, 3.7F, -2.4F}; // Visible marker matches the active point light.
	renderSystem.clearScene();
	if (auto result = renderSystem.addPlane(vve::Vec2{4.0F, 4.0F}, vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}});
		 !result) {
		return result;
	}
	if (auto result = renderSystem.addCuboid(vve::Vec3{-0.4F, -1.0F, -0.4F}, vve::Vec3{0.4F, 1.0F, 0.4F},
														  vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}},
														  vve::Transform{.translation = vve::Position{.value = vve::Vec3{0.0F, 1.0F, -0.75F}}});
		 !result) {
		return result;
	}
	if (auto result = renderSystem.addCuboid(vve::Vec3{-0.125F, -0.125F, -0.125F}, vve::Vec3{0.125F, 0.125F, 0.125F},
														  vve::LinearColor{.value = vve::Vec3{1.0F, 0.9F, 0.1F}},
														  vve::Transform{.translation = vve::Position{.value = lightPosition}});
		 !result) {
		return result;
	}
	renderSystem.setPointLight(vve::Position{.value = lightPosition}, vve::LinearColor{.value = vve::Vec3{1.0F, 0.92F, 0.55F}},
										vve::LightIntensity{.value = 4.5F}, vve::LightRange{.value = 7.5F},
										vve::LinearColor{.value = vve::Vec3{0.08F, 0.08F, 0.08F}});
	renderSystem.setDirectionalLight(vve::Direction{.value = vve::Vec3{-0.55F, -0.78F, 0.30F}},
											  vve::LinearColor{.value = vve::Vec3{0.65F, 0.82F, 1.0F}},
											  vve::LightIntensity{.value = 0.75F},
											  vve::LinearColor{.value = vve::Vec3{0.025F, 0.025F, 0.025F}});
	renderSystem.setSpotLight(vve::Position{.value = vve::Vec3{1.45F, 4.8F, -1.45F}},
										vve::Direction{.value = vve::Vec3{0.10F, -0.98F, -0.16F}},
										vve::LinearColor{.value = vve::Vec3{1.0F, 0.58F, 0.38F}},
										vve::LightIntensity{.value = 2.2F}, vve::LightRange{.value = 5.8F},
										vve::SpotConeAngle{.radians = 0.58F},
										vve::LinearColor{.value = vve::Vec3{0.02F, 0.02F, 0.02F}});
	return {};
}

} // namespace

int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[light_shadow_debug] engine=" << vve::engineImplementationNamespaceName << '\n';

	auto engine = vve::makeEngine(
		vve::ApplicationName{"light_shadow_debug"},
		vve::WindowSetups{vve::WindowSetup{}
								.id("main")
								.title("VVE Simple Light Shadow Debug")
								.extent(vve::PixelExtent{.width = 960, .height = 540})
								.renderer(vve::RendererId{.value = "forward"})});

	if (const auto result = engine.init(); !result) {
		std::cerr << "[light_shadow_debug] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto renderSystem = engine.world().get<vve::RenderSystem>();
	if (const auto result = loadShadowTestScene(renderSystem); !result) {
		std::cerr << "[light_shadow_debug] scene load failed: error=" << vve::errorName(result.error()) << '\n';
		return 2;
	}

	const int maxFrames = frameLimit(argc, argv);
	const auto path = outputPath(argc, argv);
	const auto pngPath = pngOutputPath(path);
	bool pngWritten{};
	for (int frame{}; frame < maxFrames; ++frame) {
		const auto status = engine.step();
		if (!status) {
			std::cerr << "[light_shadow_debug] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		if (!pngWritten) {
			const auto captureResult = renderSystem.captureFrameToPng(pngPath);
			if (!captureResult) {
				std::cerr << "[light_shadow_debug] capture failed: error=" << vve::errorName(captureResult.error())
							 << ", path=" << pngPath.string() << '\n';
				return 4;
			}
			pngWritten = true;
			std::cout << "[light_shadow_debug] png_written=" << pngWritten << " path=" << pngPath.string() << '\n';
		}
		if (*status == vve::FrameStatus::stopped) { break; }
	}

	if (auto parent = path.parent_path(); !parent.empty()) {
		std::filesystem::create_directories(parent);
	}
	std::ofstream output{path, std::ios::trunc};
	if (!output) {
		std::cerr << "[light_shadow_debug] could not write output: " << path.string() << '\n';
		return 5;
	}
	output << "engine=" << vve::engineImplementationNamespaceName << '\n';
	output << "frames=" << maxFrames << '\n';
	output << "renderer=facade-forward\n";
	output << "scene_instances=" << renderSystem.sceneInstanceCount() << '\n';
	output << "scene_meshes=" << renderSystem.sceneMeshCount() << '\n';
	output << "directional_light=" << renderSystem.hasSceneDirectionalLight() << '\n';
	output << "point_light=" << renderSystem.hasScenePointLight() << '\n';
	output << "spot_light=" << renderSystem.hasSceneSpotLight() << '\n';
	output << "png_written=" << pngWritten << '\n';
	std::cout << "[light_shadow_debug] frames=" << maxFrames << '\n';
	return 0;
}
