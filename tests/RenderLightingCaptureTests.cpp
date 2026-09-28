/**
 * @file
 * @brief Captured-image coverage for imported normal maps and lit/unlit material rendering.
 *
 * Functional objects:
 * - main compares lit, unlit, and flat-normal captures and checks an unlit plane's sRGB colour.
 */

import std;

import VEEngine;
import VVE.TestSupport;
import VEEngine.Simple;

/// @brief Verifies normal-mapped lighting and the sRGB output of an unlit material.
int main(int argc, char **argv) {
	const auto output_directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	auto error = std::error_code{};
	// Clear the previous run first, then retain this run's captures for inspection.
	std::filesystem::remove_all(output_directory, error);
	if (error) { return 20; }
	std::filesystem::create_directories(VVE_TEST_TMP_DIR, error);
	if (error) { return 1; }

	auto engine = vve::test::hiddenEngine("render-lighting-capture-tests", vve::PixelExtent{.width = 128, .height = 128});
	if (!engine.init()) { return 2; }

	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	const bool external_model = argc > 1;
	const auto model_path = external_model ? std::filesystem::path{argv[1]} :
		std::filesystem::path{VVE_TEST_MATERIAL_MODEL};
	const auto model_scene = assets.loadScene(model_path);
	const auto flat_scene = assets.loadScene(std::filesystem::path{VVE_TEST_FLAT_MATERIAL_MODEL});
	if (!model_scene || !flat_scene) { return 3; }

	const auto apply_view = [&render, external_model] {
		const auto eye = external_model ? vve::Position{.value = vve::Vec3{0.0F, 6.0F, 9.0F}} :
			vve::Position{.value = vve::Vec3{0.0F, 0.25F, 2.0F}};
		const auto target = external_model ? vve::Position{.value = vve::Vec3{0.0F, 1.0F, 0.0F}} :
			vve::Position{.value = vve::Vec3{0.0F, 0.25F, 0.0F}};
		render.setCamera(vve::Camera::lookAt(eye, target));
		render.addDirectionalLight(vve::Direction{.value = vve::Vec3{-0.35F, -0.25F, -1.0F}},
			vve::LinearColor{.value = vve::Vec3{1.0F, 0.95F, 0.85F}},
			vve::LightIntensity{.value = 1.5F}, vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}});
	};

	render.clearScene();
	const auto model_instance = render.instantiateScene(*model_scene);
	if (!model_instance) { return 4; }
	const auto model_objects = render.sceneInstanceObjects(*model_instance);
	if (!model_objects || model_objects->empty()) { return 5; }
	apply_view();
	const auto lit_path = output_directory / "lit.png";
	if (render.sceneDirectionalLightCount() != 1U) { return 13; }
	if (!engine.renderFrame() || !render.captureFrameToPng(lit_path)) { return 6; }

	for (const auto object : *model_objects) {
		if (!render.setObjectUnlit(object, true)) { return 7; }
	}
	const auto unlit_path = output_directory / "unlit.png";
	if (render.sceneDirectionalLightCount() != 1U) { return 14; }
	if (!engine.renderFrame() || !render.captureFrameToPng(unlit_path)) { return 8; }

	render.clearScene();
	if (!render.instantiateScene(*flat_scene)) { return 9; }
	apply_view();
	const auto flat_path = output_directory / "flat.png";
	if (render.sceneDirectionalLightCount() != 1U) { return 15; }
	if (!engine.renderFrame() || !render.captureFrameToPng(flat_path)) { return 10; }

	const auto lit_pixels = vve::test::imagePixels(lit_path);
	const auto unlit_pixels = vve::test::imagePixels(unlit_path);
	const auto flat_pixels = vve::test::imagePixels(flat_path);
	if (!lit_pixels || !unlit_pixels || !flat_pixels || lit_pixels->size() != unlit_pixels->size() ||
		lit_pixels->size() != flat_pixels->size()) { return 11; }
	auto mask = std::vector<std::size_t>{};
	// Pixel (0,0) is the clear colour; retain pixels differing by more than eight in any channel.
	for (std::size_t offset{}; offset < unlit_pixels->size(); offset += 4U) {
		if (std::ranges::any_of(std::views::iota(0U, 4U), [&](auto channel) {
			return std::abs(static_cast<int>((*unlit_pixels)[offset + channel]) - (*unlit_pixels)[channel]) > 8;
		})) { mask.push_back(offset); }
	}
	std::cout << "RenderLightingCaptureTests maskedPixelCount=" << mask.size() << " minimumMaskedPixelCount=200\n";
	if (mask.size() < 200U) { return 12; }
	const auto lit = vve::test::imageStatistics(*lit_pixels, mask);
	const auto unlit = vve::test::imageStatistics(*unlit_pixels, mask);
	const auto flat = vve::test::imageStatistics(*flat_pixels, mask);
	const double lit_unlit_difference = vve::test::meanAbsoluteDifference(*lit_pixels, *unlit_pixels, mask);
	const double normal_flat_difference = vve::test::meanAbsoluteDifference(*lit_pixels, *flat_pixels, mask);
	constexpr double minimum_lit_unlit_difference = 22.4555; // Half the measured Linux baseline of 44.911 RGBA byte units.
	constexpr double minimum_normal_flat_difference = 5.6822; // Half the measured Linux baseline of 11.3644 RGBA byte units.
	std::cout << "RenderLightingCaptureTests litMean=" << lit.mean
		<< " litStdDev=" << lit.standard_deviation
		<< " unlitMean=" << unlit.mean
		<< " unlitStdDev=" << unlit.standard_deviation
		<< " flatMean=" << flat.mean
		<< " flatStdDev=" << flat.standard_deviation
		<< " litUnlitMeanAbsDiff=" << lit_unlit_difference
		<< " minimumLitUnlitMeanAbsDiff=" << minimum_lit_unlit_difference
		<< " normalFlatMeanAbsDiff=" << normal_flat_difference
		<< " minimumNormalFlatMeanAbsDiff=" << minimum_normal_flat_difference
		<< " litUnlitMeanRatio=" << lit.mean / unlit.mean << " minimumLitUnlitMeanRatio=1.1\n";
	if (lit.standard_deviation <= 1.0 || lit.mean >= 250.0 ||
		lit_unlit_difference < minimum_lit_unlit_difference || normal_flat_difference < minimum_normal_flat_difference ||
		lit.mean <= unlit.mean * 1.1) {
		return 12;
	}

	// Cover the entire view with a known unlit colour, even while a directional light is active.
	render.clearScene();
	const auto plane = render.addPlane(vve::Vec2{4.0F, 4.0F},
		vve::LinearColor{.value = vve::Vec3{0.5F, 0.25F, 1.0F}});
	if (!plane || !render.setObjectUnlit(*plane, true)) { return 23; }
	apply_view();
	render.setCamera(vve::Camera::lookAt(vve::Position{.value = vve::Vec3{0.0F, 2.0F, 0.0F}}, vve::Position{}));
	const auto unlit_plane_path = output_directory / "unlit_plane.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(unlit_plane_path)) { return 24; }
	const auto unlit_plane_pixels = vve::test::imagePixels(unlit_plane_path);
	if (!unlit_plane_pixels || unlit_plane_pixels->size() != 128U * 128U * 4U) { return 24; }
	constexpr std::size_t centre = (64U * 128U + 64U) * 4U; ///< Centre pixel's RGBA byte offset in the 128x128 capture.
	constexpr std::array<int, 3U> expected_colour{188, 137, 255}; ///< Linear (0.5, 0.25, 1.0) encoded as sRGB bytes.
	std::println("RenderLightingCaptureTests unlitCentre=({},{},{}) expected=(188,137,255) tolerance=2",
		(*unlit_plane_pixels)[centre], (*unlit_plane_pixels)[centre + 1U], (*unlit_plane_pixels)[centre + 2U]);
	// Pin each colour channel independently so lighting or an extra transfer function cannot pass.
	for (const auto channel : std::views::iota(0U, 3U)) {
		if (std::abs(static_cast<int>((*unlit_plane_pixels)[centre + channel]) - expected_colour[channel]) > 2) { return 25; }
	}
#ifndef NDEBUG
	// Validation includes every rendered and captured frame; unavailable layers leave the check inactive.
	const auto &renderer = render.forward();
	std::cout << "RenderLightingCaptureTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 22; }
#endif
	return 0;
}
