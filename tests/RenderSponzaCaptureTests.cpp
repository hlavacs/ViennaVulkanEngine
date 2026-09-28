/**
 * @file
 * @brief Captured-image coverage for Sponza base-color texture rendering.
 *
 * Functional objects:
 * - main captures lit and unlit Sponza images and verifies texture-driven detail.
 */

import std;

import VEEngine;
import VVE.TestSupport;
import VEEngine.Simple;

/// @brief Verifies Sponza remains detailed with and without directional lighting.
int main() {
	// A tracked fixture must exist at the configured source path, regardless of the launch directory.
	const auto scene_path = std::filesystem::path{VVE_TEST_SPONZA_SCENE};
	if (!std::filesystem::exists(scene_path)) {
		std::cerr << "RenderSponzaCaptureTests asset not found: " << scene_path << '\n';
		return 21;
	}

	const auto output_directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	auto error = std::error_code{};
	// Clear the previous run first, then retain this run's captures for inspection.
	std::filesystem::remove_all(output_directory, error);
	if (error) { return 20; }
	std::filesystem::create_directories(VVE_TEST_TMP_DIR, error);
	if (error) { return 1; }

	auto engine = vve::test::hiddenEngine("render-sponza-capture-tests", vve::PixelExtent{.width = 256, .height = 256});
	if (!engine.init()) { return 2; }

	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	const auto scene = assets.loadScene(scene_path);
	if (!scene) { return 3; }
	const auto instance = render.instantiateScene(*scene);
	if (!instance) { return 4; }
	const auto objects = render.sceneInstanceObjects(*instance);
	if (!objects || objects->empty()) { return 5; }

	const auto eye = vve::Position{.value = vve::Vec3{0.0F, 6.0F, 9.0F}};
	const auto target = vve::Position{.value = vve::Vec3{0.0F, 6.0F, 0.0F}};
	render.setCamera(vve::Camera::lookAt(eye, target));
	render.addDirectionalLight(vve::Direction{.value = vve::Vec3{-0.35F, -0.25F, -1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.95F, 0.85F}},
		vve::LightIntensity{.value = 1.5F}, vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}});
	if (render.sceneDirectionalLightCount() != 1U) { return 6; }

	const auto lit_path = output_directory / "lit.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(lit_path)) { return 7; }
	for (const auto object : *objects) {
		if (!render.setObjectUnlit(object, true)) { return 8; }
	}
	const auto unlit_path = output_directory / "unlit.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(unlit_path)) { return 9; }

	const auto lit_pixels = vve::test::imagePixels(lit_path);
	const auto unlit_pixels = vve::test::imagePixels(unlit_path);
	// Measure every pixel of each image, independently of its dimensions.
	const auto offsets = std::views::transform([](std::size_t index) { return index * 4U; });
	const auto lit = lit_pixels ? std::optional{vve::test::imageStatistics(*lit_pixels,
		std::views::iota(std::size_t{}, lit_pixels->size() / 4U) | offsets)} : std::nullopt;
	const auto unlit = unlit_pixels ? std::optional{vve::test::imageStatistics(*unlit_pixels,
		std::views::iota(std::size_t{}, unlit_pixels->size() / 4U) | offsets)} : std::nullopt;
	if (!lit || !unlit) { return 10; }
	std::cout << "RenderSponzaCaptureTests litMean=" << lit->mean
		<< " litStdDev=" << lit->standard_deviation
		<< " unlitMean=" << unlit->mean
		<< " unlitStdDev=" << unlit->standard_deviation
		<< " sceneTextureCount=" << render.sceneTextureCount()
		<< " gpuTextureCount=" << render.gpuTextureCount()
		<< " gpuMaterialCount=" << render.gpuMaterialCount() << '\n';
	if (lit->standard_deviation <= 8.0 || lit->mean <= 10.0 || lit->mean >= 245.0 ||
		unlit->standard_deviation <= 8.0 || unlit->mean <= 10.0 || unlit->mean >= 245.0) {
		return 11;
	}
#ifndef NDEBUG
	// Validation includes every rendered and captured frame; unavailable layers leave the check inactive.
	const auto &renderer = render.forward();
	std::cout << "RenderSponzaCaptureTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 22; }
#endif
	return 0;
}
