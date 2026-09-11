#include <imgui.h>
#include <VVPPL.h>

import std;
import VEEngine;

/**
 * @file
 * @brief Adopted from testscene. Post Processing Demo.
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
	std::cout << "[postprocessing] engine=" << vve::engineImplementationNamespaceName << '\n';

	const auto activeRenderer = vve::RendererId{.value = "forward"}; ///< Renderer id selected through the facade.
	auto engine = vve::EngineBuilder<>{}
						 .applicationName("postprocessing")
						 .addWindow(vve::WindowSetup{}
										 .id("main")
										 .title("VVE Post Processing")
										 .extent(vve::PixelExtent{.width = 960, .height = 540})
										 .renderer(activeRenderer)
										 .resizable(true))
						 .build();

	if (const auto result = engine.init(); !result) {
		std::cerr << "[postprocessing] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto render = engine.world().get<vve::RenderSystem>();

	// Keep access to the library's effect settings.
	vvppl::TonemapSettings *tonemap{nullptr};
	vvppl::ChromaticSettings *chromatic{nullptr};
	vvppl::GreyscaleSettings *greyscale{nullptr};
	vvppl::VignetteSettings *vignette{nullptr};
	vvppl::FilmGrainSettings *grain{nullptr};
	vvppl::ColorGradeSettings *grade{nullptr};
	vvppl::DitherSettings *dither{nullptr};

	// Configure the effects when the renderer creates the chain.
	render.setPostProcessSetup(
    	[&tonemap, &chromatic, &greyscale, &vignette, &grain, &grade, &dither](vvppl::PostProcessing &pp) {
			tonemap = &pp.addTonemap();
			chromatic = &pp.addChromatic();
			greyscale = &pp.addGreyscale();
			vignette = &pp.addVignette();
			grain = &pp.addFilmGrain();
			grade = &pp.addColorGrade();
			dither = &pp.addDither();

			tonemap->exposure = 0.6F;
			chromatic->intensity = 0.02F;
			greyscale->strength = 0.2F;
			vignette->intensity = 0.8F;
			vignette->radius = 0.4F;
			vignette->smoothness = 0.6F;
			grain->intensity = 0.05F;
    });

	if (const auto result = loadGameScene(render, assetRoot(argc > 0 ? argv[0] : nullptr)); !result) {
		std::cerr << "[postprocessing] scene load failed: error=" << vve::errorName(result.error()) << '\n';
		return 2;
	}

	// Simple Lights
	const auto white = vve::LinearColor{.value = vve::Vec3{1.0F, 1.0F, 1.0F}};
	const auto ambient = vve::LinearColor{.value = vve::Vec3{0.05F, 0.05F, 0.05F}};
	render.setDirectionalLight(vve::Direction{.value = vve::Vec3{-0.5F, -1.0F, 0.5F}}, white,
							vve::LightIntensity{.value = 1.0F}, ambient);
	render.setPointLight(vve::Position{.value = vve::Vec3{2.0F, 4.0F, 2.0F}}, white,
						vve::LightIntensity{.value = 3.0F}, vve::LightRange{.value = 8.0F}, ambient);

	const int maxFrames = frameLimit(argc, argv).value_or(0);
	int frame{};
	bool running = true;                                                                 ///< GUI changes are applied after the frame callback returns.
	double renderFps{};                                                                              ///< Render-system FPS, not the ImGui/display estimate.
	vve::DefaultCameraController cameraController{};                                                ///< Facade camera motion shared by examples and applications.
	cameraController.eye = vve::Position{.value = vve::Vec3{-2.0F, 1.5F, 6.0F}};
	const auto startupForward =
		vve::math::normalize(vve::math::subtract(vve::Vec3{0.0F, 1.0F, 0.0F}, cameraController.eye.value));
	cameraController.yaw = std::atan2(startupForward.x, -startupForward.z);
	cameraController.pitch = std::asin(startupForward.y);
	engine.world().get<vve::GuiSystem>().draw([&frame, &activeRenderer, &cameraController, &renderFps,
															 &tonemap, &chromatic, &greyscale, &vignette, &grain, &grade, &dither] {
		ImGui::SetNextWindowSize(ImVec2(320.0F, 320.0F), ImGuiCond_Always);
		ImGui::Begin("Post Processing");
		ImGui::PushItemWidth(160.0F);
		// Adjust the effect settings directly.
		if (tonemap) {
			ImGui::SliderFloat("Exposure", &tonemap->exposure, 0.0F, 3.0F);
		}
		if (chromatic) {
			ImGui::SliderFloat("Chromatic aberration",
							&chromatic->intensity, 0.0F, 0.03F, "%.3f");
		}
		if (greyscale) {
			ImGui::SliderFloat("Greyscale", &greyscale->strength, 0.0F, 1.0F);
		}
		if (grain) {
			ImGui::SliderFloat("Film grain", &grain->intensity, 0.0F, 0.3F);
		}
		if (dither) {
			ImGui::SliderFloat("Dithering", &dither->strength, 0.0F, 8.0F);
		}
		if (vignette && ImGui::CollapsingHeader("Vignette")) {
			ImGui::SliderFloat("Intensity", &vignette->intensity, 0.0F, 1.0F);
			ImGui::SliderFloat("Radius", &vignette->radius, 0.0F, 1.0F);
			ImGui::SliderFloat("Smoothness",
							&vignette->smoothness, 0.01F, 1.0F);
		}
		if (grade && ImGui::CollapsingHeader("Color grading")) {
			ImGui::SliderFloat("Saturation", &grade->saturation, 0.0F, 2.0F);
			ImGui::SliderFloat("Contrast", &grade->contrast, 0.5F, 1.5F);
			ImGui::SliderFloat3("Lift", grade->lift, -0.2F, 0.2F);
			ImGui::SliderFloat3("Gamma", grade->gamma, 0.2F, 3.0F);
			ImGui::SliderFloat3("Gain", grade->gain, 0.0F, 2.0F);
		}

		ImGui::Separator();

		ImGui::Text("Frame: %d", frame);
		ImGui::Text("Render FPS: %.1f", renderFps);
		ImGui::Text("Renderer: %s", activeRenderer.value.c_str());
		ImGui::Text("Camera: %.2f, %.2f, %.2f", cameraController.eye.value.x, cameraController.eye.value.y,
						cameraController.eye.value.z);

		ImGui::PopItemWidth();
		ImGui::End();
	});
	
	const auto startTime = std::chrono::steady_clock::now(); // Clock for Film Grain shader

	while (running && (maxFrames == 0 || frame < maxFrames)) {
		const auto frameInput = engine.world().get<vve::WindowSystem>().input();
		render.setCamera(cameraController.update(frameInput), vve::PixelExtent{.width = 960, .height = 540});
		renderFps = render.renderingFramesPerSecond();

		// New seed with every frame for the shader - time
		if (grain) {
			grain->time = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - startTime).count();
		}

		const auto status = engine.step();
		if (!status) {
			std::cerr << "[postprocessing] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		++frame;
		if (*status == vve::FrameStatus::stopped) { break; }

		auto input = engine.world().get<vve::WindowSystem>().input();
		if (input.wasKeyPressed(vve::Key::escape)) { running = false; }
	}

	std::cout << "[postprocessing] frames=" << frame << '\n';
	return 0;
}
