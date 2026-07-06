#include <imgui.h>

import std;
import VEEngine;

/**
 * @file
 * @brief Interactive example that drives the renderer through the public facade.
 */
namespace {

constexpr auto crateTextureRelativePath = "assets/game/crate0/diffuse.png";
constexpr std::size_t maxGameDirectionalLights{10U}; ///< Current simple forward renderer directional-light demo cap.
constexpr std::size_t maxGamePointLights{10U};       ///< Current simple forward renderer point-light demo cap.
constexpr std::size_t maxGameSpotLights{10U};        ///< Current simple forward renderer spot-light demo cap.

/// @brief Finds the repository-style asset root from either the cwd or executable location.
[[nodiscard]] std::filesystem::path assetRoot(char *argv0) {
	auto containsGameAssets = [](const std::filesystem::path &candidate) {
		return std::filesystem::exists(candidate / crateTextureRelativePath);
	};
	if (const auto cwd = std::filesystem::current_path(); containsGameAssets(cwd)) {
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
		if (containsGameAssets(candidate)) {
			return candidate;
		}
		if (candidate == candidate.root_path()) {
			break;
		}
	}
	return {};
}

/// @brief Reads the optional frame count used by automated example runs.
[[nodiscard]] std::optional<int> frameLimit(int argc, char **argv) {
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
	return std::nullopt;
}

/// @brief Adds the game floor and three crate cubes through facade scene authoring calls.
[[nodiscard]] std::expected<void, vve::Error> loadGameScene(vve::RenderSystem render, const std::filesystem::path &root) {
	constexpr vve::Vec3 cubeMinimum{-0.5F, -0.5F, -0.5F}; ///< Unit cube lower corner.
	constexpr vve::Vec3 cubeMaximum{0.5F, 0.5F, 0.5F};    ///< Unit cube upper corner.
	constexpr float cubeCenterY = 0.5F;                   ///< Unit cube bottom sits on the y=0 ground plane.
	const auto crateTexture = root / crateTextureRelativePath; ///< Crate diffuse texture.

	render.clearScene();
	if (auto result = render.addPlane(vve::Vec2{6.0F, 4.0F}, vve::LinearColor{.value = vve::Vec3{0.1F, 0.6F, 0.2F}});
		 !result) {
		return std::unexpected(result.error());
	}
	for (const vve::Vec3 center : std::array{vve::Vec3{-1.5F, cubeCenterY, -0.5F},
														  vve::Vec3{0.0F, cubeCenterY, 0.75F},
														  vve::Vec3{1.5F, cubeCenterY, -0.5F}}) {
		if (auto result = render.addTexturedCuboid(cubeMinimum, cubeMaximum, crateTexture,
																 vve::Transform{.translation = vve::Position{.value = center}});
			 !result) {
			return std::unexpected(result.error());
		}
	}
	return {};
}

} // namespace

/**
 * @brief Runs a small interactive scene using only the public VVE facade.
 */
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[game] engine=" << vve::engineImplementationNamespaceName << '\n';

	const auto activeRenderer = vve::RendererId{.value = "forward"}; ///< Renderer id selected through the facade.
	auto engine = vve::EngineBuilder<>{}
						 .applicationName("game")
						 .addWindow(vve::WindowSetup{}
										 .id("main")
										 .title("VVE Simple Game")
										 .extent(vve::PixelExtent{.width = 960, .height = 540})
										 .renderer(activeRenderer)
										 .resizable(true))
						 .build();

	if (const auto result = engine.init(); !result) {
		std::cerr << "[game] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto render = engine.world().get<vve::RenderSystem>();
	if (const auto result = loadGameScene(render, assetRoot(argc > 0 ? argv[0] : nullptr)); !result) {
		std::cerr << "[game] scene load failed: error=" << vve::errorName(result.error()) << '\n';
		return 2;
	}

	const auto pointPositions = std::array{
		vve::Position{.value = vve::Vec3{2.0F, 3.5F, -2.0F}},
		vve::Position{.value = vve::Vec3{-2.0F, 3.0F, 2.0F}},
		vve::Position{.value = vve::Vec3{0.0F, 2.8F, -3.0F}},
		vve::Position{.value = vve::Vec3{0.0F, 2.6F, 3.0F}},
		vve::Position{.value = vve::Vec3{-3.2F, 2.9F, -0.5F}},
		vve::Position{.value = vve::Vec3{3.2F, 2.9F, -0.5F}},
		vve::Position{.value = vve::Vec3{-2.4F, 2.4F, -2.7F}},
		vve::Position{.value = vve::Vec3{2.4F, 2.4F, -2.7F}},
		vve::Position{.value = vve::Vec3{-2.6F, 2.5F, 2.4F}},
		vve::Position{.value = vve::Vec3{2.6F, 2.5F, 2.4F}},
	};																													///< Ten facade point positions exercise every demo slot.
	const auto pointColors = std::array{
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.96F, 0.82F}},
		vve::LinearColor{.value = vve::Vec3{0.55F, 0.78F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.55F, 0.42F}},
		vve::LinearColor{.value = vve::Vec3{0.62F, 1.0F, 0.54F}},
		vve::LinearColor{.value = vve::Vec3{0.86F, 0.62F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.78F, 0.48F}},
		vve::LinearColor{.value = vve::Vec3{0.48F, 0.94F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.48F, 0.78F}},
		vve::LinearColor{.value = vve::Vec3{0.74F, 1.0F, 0.66F}},
		vve::LinearColor{.value = vve::Vec3{0.68F, 0.72F, 1.0F}},
	};																													///< Distinct colors make active point slots visible.
	const auto pointIntensities = std::array{
		vve::LightIntensity{.value = 3.0F},
		vve::LightIntensity{.value = 2.4F},
		vve::LightIntensity{.value = 1.5F},
		vve::LightIntensity{.value = 1.4F},
		vve::LightIntensity{.value = 1.2F},
		vve::LightIntensity{.value = 1.2F},
		vve::LightIntensity{.value = 1.0F},
		vve::LightIntensity{.value = 1.0F},
		vve::LightIntensity{.value = 0.9F},
		vve::LightIntensity{.value = 0.9F},
	};																													///< Point strengths keep the combined scene readable.
	const auto pointRanges = std::array{
		vve::LightRange{.value = 7.0F},
		vve::LightRange{.value = 6.0F},
		vve::LightRange{.value = 4.8F},
		vve::LightRange{.value = 4.8F},
		vve::LightRange{.value = 4.5F},
		vve::LightRange{.value = 4.5F},
		vve::LightRange{.value = 4.0F},
		vve::LightRange{.value = 4.0F},
		vve::LightRange{.value = 3.8F},
		vve::LightRange{.value = 3.8F},
	};																													///< Point ranges bound each local light volume.
	const auto pointAmbients = std::array{
		vve::LinearColor{.value = vve::Vec3{0.18F, 0.18F, 0.18F}},
		vve::LinearColor{.value = vve::Vec3{0.06F, 0.08F, 0.1F}},
		vve::LinearColor{.value = vve::Vec3{0.025F, 0.015F, 0.012F}},
		vve::LinearColor{.value = vve::Vec3{0.014F, 0.025F, 0.012F}},
		vve::LinearColor{.value = vve::Vec3{0.018F, 0.012F, 0.026F}},
		vve::LinearColor{.value = vve::Vec3{0.024F, 0.018F, 0.010F}},
		vve::LinearColor{.value = vve::Vec3{0.010F, 0.022F, 0.026F}},
		vve::LinearColor{.value = vve::Vec3{0.026F, 0.010F, 0.020F}},
		vve::LinearColor{.value = vve::Vec3{0.016F, 0.024F, 0.014F}},
		vve::LinearColor{.value = vve::Vec3{0.014F, 0.016F, 0.025F}},
	};																													///< Per-light ambient terms mirror the other light controls.
	const auto directionalDirections = std::array{
		vve::Direction{.value = vve::Vec3{-0.45F, -0.8F, 0.35F}},
		vve::Direction{.value = vve::Vec3{0.55F, -0.72F, 0.12F}},
		vve::Direction{.value = vve::Vec3{-0.12F, -0.9F, -0.42F}},
		vve::Direction{.value = vve::Vec3{0.28F, -0.82F, -0.38F}},
		vve::Direction{.value = vve::Vec3{-0.72F, -0.58F, -0.20F}},
		vve::Direction{.value = vve::Vec3{0.74F, -0.55F, -0.22F}},
		vve::Direction{.value = vve::Vec3{-0.34F, -0.62F, 0.70F}},
		vve::Direction{.value = vve::Vec3{0.36F, -0.64F, 0.68F}},
		vve::Direction{.value = vve::Vec3{-0.08F, -0.98F, 0.18F}},
		vve::Direction{.value = vve::Vec3{0.12F, -0.96F, -0.25F}},
	};																													///< Ten facade directional vectors exercise every demo slot.
	const auto directionalColors = std::array{
		vve::LinearColor{.value = vve::Vec3{0.95F, 0.98F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.78F, 0.58F}},
		vve::LinearColor{.value = vve::Vec3{0.58F, 0.86F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{0.72F, 1.0F, 0.64F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.62F, 0.70F}},
		vve::LinearColor{.value = vve::Vec3{0.62F, 1.0F, 0.88F}},
		vve::LinearColor{.value = vve::Vec3{0.82F, 0.70F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.92F, 0.55F}},
		vve::LinearColor{.value = vve::Vec3{0.68F, 0.78F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{0.86F, 1.0F, 0.72F}},
	};																													///< Distinct colors make active directional slots visible.
	const auto directionalIntensities = std::array{
		vve::LightIntensity{.value = 1.05F},
		vve::LightIntensity{.value = 0.45F},
		vve::LightIntensity{.value = 0.35F},
		vve::LightIntensity{.value = 0.28F},
		vve::LightIntensity{.value = 0.22F},
		vve::LightIntensity{.value = 0.22F},
		vve::LightIntensity{.value = 0.18F},
		vve::LightIntensity{.value = 0.18F},
		vve::LightIntensity{.value = 0.14F},
		vve::LightIntensity{.value = 0.14F},
	};																													///< Directional strengths keep the combined scene readable.
	const auto directionalAmbients = std::array{
		vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}},
		vve::LinearColor{.value = vve::Vec3{0.015F, 0.012F, 0.01F}},
		vve::LinearColor{.value = vve::Vec3{0.01F, 0.014F, 0.018F}},
		vve::LinearColor{.value = vve::Vec3{0.01F, 0.016F, 0.01F}},
		vve::LinearColor{.value = vve::Vec3{0.012F, 0.008F, 0.010F}},
		vve::LinearColor{.value = vve::Vec3{0.008F, 0.012F, 0.010F}},
		vve::LinearColor{.value = vve::Vec3{0.010F, 0.008F, 0.012F}},
		vve::LinearColor{.value = vve::Vec3{0.012F, 0.011F, 0.007F}},
		vve::LinearColor{.value = vve::Vec3{0.007F, 0.009F, 0.012F}},
		vve::LinearColor{.value = vve::Vec3{0.009F, 0.012F, 0.007F}},
	};																													///< Per-light ambient terms mirror the spot-light muting model.
	const auto spotIntensity = vve::LightIntensity{.value = 4.0F};                                     ///< Startup spot light strength.
	const auto spotRange = vve::LightRange{.value = 8.0F};                                             ///< Startup spot light reach.
	const auto spotCone = vve::SpotConeAngle{.radians = 0.65F};                                        ///< Startup spot light outer cone.
	const auto spotAmbient = vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}};                ///< Startup spot ambient term.
	const auto spotPositions = std::array{
		vve::Position{.value = vve::Vec3{0.0F, 4.0F, 3.0F}},
		vve::Position{.value = vve::Vec3{-2.4F, 3.8F, -1.2F}},
		vve::Position{.value = vve::Vec3{2.4F, 3.8F, -1.2F}},
		vve::Position{.value = vve::Vec3{0.0F, 4.3F, -3.0F}},
		vve::Position{.value = vve::Vec3{-3.1F, 3.5F, 1.6F}},
		vve::Position{.value = vve::Vec3{3.1F, 3.5F, 1.6F}},
		vve::Position{.value = vve::Vec3{-3.0F, 3.3F, -3.0F}},
		vve::Position{.value = vve::Vec3{3.0F, 3.3F, -3.0F}},
		vve::Position{.value = vve::Vec3{-1.0F, 5.0F, 0.0F}},
		vve::Position{.value = vve::Vec3{1.0F, 5.0F, 0.0F}},
	};																													///< Ten facade spot positions exercise every demo slot.
	const auto spotDirections = std::array{
		vve::Direction{.value = vve::Vec3{0.0F, -0.85F, -0.45F}},
		vve::Direction{.value = vve::Vec3{0.35F, -0.85F, 0.2F}},
		vve::Direction{.value = vve::Vec3{-0.35F, -0.85F, 0.2F}},
		vve::Direction{.value = vve::Vec3{0.0F, -0.9F, 0.35F}},
		vve::Direction{.value = vve::Vec3{0.72F, -0.76F, -0.30F}},
		vve::Direction{.value = vve::Vec3{-0.72F, -0.76F, -0.30F}},
		vve::Direction{.value = vve::Vec3{0.55F, -0.72F, 0.55F}},
		vve::Direction{.value = vve::Vec3{-0.55F, -0.72F, 0.55F}},
		vve::Direction{.value = vve::Vec3{0.18F, -0.98F, 0.04F}},
		vve::Direction{.value = vve::Vec3{-0.18F, -0.98F, 0.04F}},
	};																													///< Each spot aims at the crate group from a different side.
	const auto spotColors = std::array{
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.9F, 0.72F}},
		vve::LinearColor{.value = vve::Vec3{0.6F, 0.85F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.55F, 0.45F}},
		vve::LinearColor{.value = vve::Vec3{0.65F, 1.0F, 0.58F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.72F, 0.95F}},
		vve::LinearColor{.value = vve::Vec3{0.72F, 1.0F, 0.95F}},
		vve::LinearColor{.value = vve::Vec3{0.92F, 0.78F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.92F, 0.62F}},
		vve::LinearColor{.value = vve::Vec3{0.72F, 0.78F, 1.0F}},
		vve::LinearColor{.value = vve::Vec3{0.82F, 1.0F, 0.72F}},
	};																													///< Distinct colors make active spot slots visible.
	const auto offAmbient = vve::LinearColor{.value = vve::Vec3{0.0F, 0.0F, 0.0F}};                    ///< Muted light ambient term.
	constexpr auto offIntensity = vve::LightIntensity{.value = 0.0F};                                  ///< Muted direct light strength.
	constexpr auto minimumShadowPointIntensity = vve::LightIntensity{.value = 2.0F};                  ///< Minimum enabled point brightness for visible shadows.
	constexpr auto offRange = vve::LightRange{.value = 0.0F};                                          ///< Muted local light reach.
	auto directionalLightsEnabled = std::array<bool, maxGameDirectionalLights>{};                    ///< Tracks each capped directional light.
	auto pointLightsEnabled = std::array<bool, maxGamePointLights>{};                                ///< Tracks each capped point light.
	auto spotLightsEnabled = std::array<bool, maxGameSpotLights>{};                                  ///< Tracks each capped spot light.
	directionalLightsEnabled.fill(true);                                                            ///< Start with every directional slot active.
	pointLightsEnabled.fill(true);                                                                  ///< Start with every point slot active.
	spotLightsEnabled.fill(true);                                                                   ///< Start with every spot slot active.
	auto applyLights = [&] {
		const auto applyDirectional = [&](std::size_t index, bool first) {
			const auto intensity = directionalLightsEnabled[index] ? directionalIntensities[index] : offIntensity;
			const auto ambient = directionalLightsEnabled[index] ? directionalAmbients[index] : offAmbient;
			if (first) {
				render.setDirectionalLight(directionalDirections[index], directionalColors[index], intensity, ambient);
			} else {
				render.addDirectionalLight(directionalDirections[index], directionalColors[index], intensity, ambient);
			}
		};																												// First directional resets the set; the rest fill the capped slots.
		applyDirectional(0U, true);
		for (std::size_t index{1U}; index < directionalLightsEnabled.size(); ++index) { applyDirectional(index, false); }
		const auto applySpot = [&](std::size_t index, bool first) {
			const auto intensity = spotLightsEnabled[index] ? spotIntensity : offIntensity;
			const auto range = spotLightsEnabled[index] ? spotRange : offRange;
			const auto ambient = spotLightsEnabled[index] ? spotAmbient : offAmbient;
			if (first) {
				render.setSpotLight(spotPositions[index], spotDirections[index], spotColors[index], intensity, range, spotCone, ambient);
			} else {
				render.addSpotLight(spotPositions[index], spotDirections[index], spotColors[index], intensity, range, spotCone, ambient);
			}
		};																												// First spot resets the set; the rest fill the capped slots.
		applySpot(0U, true);
		for (std::size_t index{1U}; index < spotLightsEnabled.size(); ++index) { applySpot(index, false); }
		bool anyPointLightEnabled{};																				// Disabled fallback keeps the legacy point slot valid.
		for (std::size_t index{}; index < pointLightsEnabled.size(); ++index) {
			if (!pointLightsEnabled[index]) { continue; }
			const auto pointIntensity = vve::LightIntensity{
				.value = std::max(pointIntensities[index].value, minimumShadowPointIntensity.value)}; // Every enabled point light can cast a readable shadow.
			if (!anyPointLightEnabled) {
				render.setPointLight(pointPositions[index], pointColors[index], pointIntensity, pointRanges[index],
										 pointAmbients[index]);
			} else {
				render.addPointLight(pointPositions[index], pointColors[index], pointIntensity, pointRanges[index],
										 pointAmbients[index]);
			}
			anyPointLightEnabled = true;
		}
		if (!anyPointLightEnabled) {
			render.setPointLight(pointPositions.front(), pointColors.front(), offIntensity, offRange, offAmbient);
		}
	};
	applyLights();

	const int maxFrames = frameLimit(argc, argv).value_or(0);
	int frame{};
	bool running = true;
	bool lightsDirty = false;                                                                        ///< GUI changes are applied after the frame callback returns.
	double renderFps{};                                                                              ///< Render-system FPS, not the ImGui/display estimate.
	vve::DefaultCameraController cameraController{};                                                ///< Facade camera motion shared by examples and applications.
	cameraController.eye = vve::Position{.value = vve::Vec3{0.0F, 6.0F, 9.0F}};
	const auto startupForward =
		vve::math::normalize(vve::math::subtract(vve::Vec3{0.0F, 1.0F, 0.0F}, cameraController.eye.value));
	cameraController.yaw = std::atan2(startupForward.x, -startupForward.z);
	cameraController.pitch = std::asin(startupForward.y);
	engine.world().get<vve::GuiSystem>().draw([&frame, &activeRenderer, &directionalLightsEnabled, &pointLightsEnabled,
															 &spotLightsEnabled, &cameraController, &lightsDirty, &renderFps] {
		ImGui::Begin("Game");
		ImGui::Text("Frame: %d", frame);
		ImGui::Text("Render FPS: %.1f", renderFps);
		ImGui::Text("Renderer: %s", activeRenderer.value.c_str());
		ImGui::Text("Directional lights: %zu", directionalLightsEnabled.size());
		for (std::size_t index{}; index < directionalLightsEnabled.size(); ++index) {
			const auto label = std::format("Directional {}", index + 1U);
			if (ImGui::Checkbox(label.c_str(), &directionalLightsEnabled[index])) { lightsDirty = true; }
		}
		ImGui::Text("Spot lights: %zu", spotLightsEnabled.size());
		for (std::size_t index{}; index < spotLightsEnabled.size(); ++index) {
			const auto label = std::format("Spot {}", index + 1U);
			if (ImGui::Checkbox(label.c_str(), &spotLightsEnabled[index])) { lightsDirty = true; }
		}
		ImGui::Text("Point lights: %zu", pointLightsEnabled.size());
		for (std::size_t index{}; index < pointLightsEnabled.size(); ++index) {
			const auto label = std::format("Point {}", index + 1U);
			if (ImGui::Checkbox(label.c_str(), &pointLightsEnabled[index])) { lightsDirty = true; }
		}
		ImGui::Text("Camera: %.2f, %.2f, %.2f", cameraController.eye.value.x, cameraController.eye.value.y,
						cameraController.eye.value.z);
		ImGui::End();
	});
	while (running && (maxFrames == 0 || frame < maxFrames)) {
		const auto frameInput = engine.world().get<vve::WindowSystem>().input();
		render.setCamera(cameraController.update(frameInput), vve::PixelExtent{.width = 960, .height = 540});
		renderFps = render.renderingFramesPerSecond();

		const auto status = engine.step();
		if (!status) {
			std::cerr << "[game] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		++frame;
		if (*status == vve::FrameStatus::stopped) { break; }

		auto input = engine.world().get<vve::WindowSystem>().input();
		if (input.wasKeyPressed(vve::Key::escape)) { running = false; }
		if (input.wasKeyPressed(vve::Key::o)) {
			const bool enabled = !std::ranges::any_of(spotLightsEnabled, std::identity{});
			spotLightsEnabled.fill(enabled);
			applyLights();
			std::cout << "[game] spot lights " << (enabled ? "on" : "off") << '\n';
		}
		if (input.wasKeyPressed(vve::Key::p)) {
			const bool enabled = !std::ranges::any_of(pointLightsEnabled, std::identity{});
			pointLightsEnabled.fill(enabled);
			applyLights();
			std::cout << "[game] point lights " << (enabled ? "on" : "off") << '\n';
		}
		if (input.wasKeyPressed(vve::Key::l)) {
			const bool enabled = !std::ranges::any_of(directionalLightsEnabled, std::identity{});
			directionalLightsEnabled.fill(enabled);
			applyLights();
			std::cout << "[game] directional lights " << (enabled ? "on" : "off") << '\n';
		}
		if (lightsDirty) {
			applyLights();
			lightsDirty = false;
		}
	}

	std::cout << "[game] frames=" << frame << '\n';
	return 0;
}
