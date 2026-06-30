#include <imgui.h>

import std;
import VEEngine;

/**
 * @file
 * @brief Interactive example that drives the renderer through the public facade.
 */
namespace {

constexpr auto crateTextureRelativePath = "assets/game/crate0/diffuse.png";

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
		return result;
	}
	for (const vve::Vec3 center : std::array{vve::Vec3{-1.5F, cubeCenterY, -0.5F},
														  vve::Vec3{0.0F, cubeCenterY, 0.75F},
														  vve::Vec3{1.5F, cubeCenterY, -0.5F}}) {
		if (auto result = render.addTexturedCuboid(cubeMinimum, cubeMaximum, crateTexture,
																 vve::Transform{.translation = vve::Position{.value = center}});
			 !result) {
			return result;
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

	const auto pointPosition = vve::Position{.value = vve::Vec3{2.0F, 3.5F, -2.0F}};                    ///< Startup point light position.
	const auto pointColor = vve::LinearColor{.value = vve::Vec3{1.0F, 0.96F, 0.82F}};                  ///< Startup point light tint.
	const auto pointIntensity = vve::LightIntensity{.value = 3.0F};                                    ///< Startup point light strength.
	const auto pointRange = vve::LightRange{.value = 7.0F};                                            ///< Startup point light reach.
	const auto pointAmbient = vve::LinearColor{.value = vve::Vec3{0.18F, 0.18F, 0.18F}};               ///< Startup point ambient term.
	const auto directionalDirection = vve::Direction{.value = vve::Vec3{-0.45F, -0.8F, 0.35F}};        ///< Startup directional light direction.
	const auto directionalColor = vve::LinearColor{.value = vve::Vec3{0.95F, 0.98F, 1.0F}};            ///< Startup directional light tint.
	const auto directionalIntensity = vve::LightIntensity{.value = 1.4F};                              ///< Startup directional light strength.
	const auto directionalAmbient = vve::LinearColor{.value = vve::Vec3{0.06F, 0.06F, 0.06F}};         ///< Startup directional ambient term.
	const auto spotPosition = vve::Position{.value = vve::Vec3{0.0F, 4.0F, 3.0F}};                     ///< Startup spot light position.
	const auto spotDirection = vve::Direction{.value = vve::Vec3{0.0F, -0.85F, -0.45F}};               ///< Startup spot light direction.
	const auto spotColor = vve::LinearColor{.value = vve::Vec3{1.0F, 0.9F, 0.72F}};                    ///< Startup spot light tint.
	const auto spotIntensity = vve::LightIntensity{.value = 4.0F};                                     ///< Startup spot light strength.
	const auto spotRange = vve::LightRange{.value = 8.0F};                                             ///< Startup spot light reach.
	const auto spotCone = vve::SpotConeAngle{.radians = 0.65F};                                        ///< Startup spot light outer cone.
	const auto spotAmbient = vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}};                ///< Startup spot ambient term.
	const auto offAmbient = vve::LinearColor{.value = vve::Vec3{0.0F, 0.0F, 0.0F}};                    ///< Muted light ambient term.
	constexpr auto offIntensity = vve::LightIntensity{.value = 0.0F};                                  ///< Muted direct light strength.
	constexpr auto offRange = vve::LightRange{.value = 0.0F};                                          ///< Muted local light reach.
	bool spotLightEnabled = true;                                                                      ///< Tracks whether the spot light contributes.
	bool pointLightEnabled = true;                                                                     ///< Tracks whether the point light contributes.
	bool directionalLightEnabled = true;                                                               ///< Tracks whether the directional light contributes.
	auto applyLights = [&] {
		render.setSpotLight(spotPosition, spotDirection, spotColor, spotLightEnabled ? spotIntensity : offIntensity,
								  spotLightEnabled ? spotRange : offRange, spotCone, spotLightEnabled ? spotAmbient : offAmbient);
		render.setPointLight(pointPosition, pointColor, pointLightEnabled ? pointIntensity : offIntensity,
									pointLightEnabled ? pointRange : offRange, pointLightEnabled ? pointAmbient : offAmbient);
		render.setDirectionalLight(directionalDirection, directionalColor,
										 directionalLightEnabled ? directionalIntensity : offIntensity,
										 directionalLightEnabled ? directionalAmbient : offAmbient);
	};
	applyLights();

	const int maxFrames = frameLimit(argc, argv).value_or(0);
	int frame{};
	bool running = true;
	vve::Vec3 cameraEye{0.0F, 6.0F, 9.0F};                                                         ///< Matches the previous renderer default eye.
	const vve::Vec3 initialTarget{0.0F, 1.0F, 0.0F};                                                ///< Matches the previous renderer default target.
	const vve::Vec3 worldUp{0.0F, 1.0F, 0.0F};                                                      ///< Stable up axis for view and strafing.
	const vve::Vec3 initialForward = vve::math::normalize(vve::math::subtract(initialTarget, cameraEye)); ///< Default view direction.
	float cameraYaw = std::atan2(initialForward.x, -initialForward.z);                              ///< Horizontal look angle around Y.
	float cameraPitch = std::asin(initialForward.y);                                                ///< Vertical look angle from the XZ plane.
	constexpr float moveStep = 0.08F;                                                               ///< Small fixed per-frame movement distance.
	constexpr float turnStep = 0.025F;                                                              ///< Small fixed per-frame rotation angle.
	constexpr float maxPitch = 1.45F;                                                               ///< Keeps the view away from the up-axis singularity.
	engine.world().get<vve::GuiSystem>().draw([&frame, &activeRenderer, &spotLightEnabled, &pointLightEnabled,
															 &directionalLightEnabled, &cameraEye] {
		ImGui::Begin("Game");
		ImGui::Text("Frame: %d", frame);
		ImGui::Text("Renderer: %s", activeRenderer.value.c_str());
		ImGui::Text("Spot light: %s", spotLightEnabled ? "on" : "off");
		ImGui::Text("Point light: %s", pointLightEnabled ? "on" : "off");
		ImGui::Text("Directional light: %s", directionalLightEnabled ? "on" : "off");
		ImGui::Text("Camera: %.2f, %.2f, %.2f", cameraEye.x, cameraEye.y, cameraEye.z);
		ImGui::End();
	});
	while (running && (maxFrames == 0 || frame < maxFrames)) {
		const vve::Vec3 forward = vve::math::normalize(vve::Vec3{
			std::cos(cameraPitch) * std::sin(cameraYaw), std::sin(cameraPitch), -std::cos(cameraPitch) * std::cos(cameraYaw)});
		render.setCamera(vve::Camera::lookAt(vve::Position{.value = cameraEye},
														 vve::Position{.value = vve::math::add(cameraEye, forward)},
														 vve::Direction{.value = worldUp}),
							  vve::PixelExtent{.width = 960, .height = 540});

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
			spotLightEnabled = !spotLightEnabled;
			applyLights();
			std::cout << "[game] spot light " << (spotLightEnabled ? "on" : "off") << '\n';
		}
		if (input.wasKeyPressed(vve::Key::p)) {
			pointLightEnabled = !pointLightEnabled;
			applyLights();
			std::cout << "[game] point light " << (pointLightEnabled ? "on" : "off") << '\n';
		}
		if (input.wasKeyPressed(vve::Key::l)) {
			directionalLightEnabled = !directionalLightEnabled;
			applyLights();
			std::cout << "[game] directional light " << (directionalLightEnabled ? "on" : "off") << '\n';
		}
		if (input.isKeyDown(vve::Key::left)) { cameraYaw -= turnStep; }
		if (input.isKeyDown(vve::Key::right)) { cameraYaw += turnStep; }
		if (input.isKeyDown(vve::Key::up)) { cameraPitch -= turnStep; }
		if (input.isKeyDown(vve::Key::down)) { cameraPitch += turnStep; }
		cameraPitch = vve::math::clamp(cameraPitch, -maxPitch, maxPitch);

		const vve::Vec3 movedForward = vve::math::normalize(vve::Vec3{
			std::cos(cameraPitch) * std::sin(cameraYaw), std::sin(cameraPitch), -std::cos(cameraPitch) * std::cos(cameraYaw)});
		const vve::Vec3 right = vve::math::normalize(vve::math::cross(movedForward, worldUp));
		if (input.isKeyDown(vve::Key::w)) { cameraEye = vve::math::add(cameraEye, vve::math::scale(movedForward, moveStep)); }
		if (input.isKeyDown(vve::Key::s)) { cameraEye = vve::math::subtract(cameraEye, vve::math::scale(movedForward, moveStep)); }
		if (input.isKeyDown(vve::Key::a)) { cameraEye = vve::math::subtract(cameraEye, vve::math::scale(right, moveStep)); }
		if (input.isKeyDown(vve::Key::d)) { cameraEye = vve::math::add(cameraEye, vve::math::scale(right, moveStep)); }
	}

	std::cout << "[game] frames=" << frame << '\n';
	return 0;
}
