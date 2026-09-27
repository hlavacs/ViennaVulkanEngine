#include <stb_image.h>

/**
 * @file
 * @brief Captured-image coverage for imported normal maps and lit/unlit material rendering.
 *
 * Functional objects:
 * - ImageStatistics retains decoded pixels and luminance statistics for one captured PNG.
 * - main renders lit, unlit, and flat-normal variants and compares their deterministic captures.
 */

import std;

import VEEngine;
import VEEngine.Simple;

namespace {

/// @brief Decoded image bytes and luminance distribution used by capture assertions.
struct ImageStatistics {
	std::vector<stbi_uc> rgba{}; ///< Tight RGBA8 pixels used for image comparisons.
	double mean{};               ///< Mean Rec. 709 luminance in byte units.
	double standard_deviation{}; ///< Population standard deviation of luminance.
};

/// @brief Decodes one PNG and computes its luminance mean and population deviation.
[[nodiscard]] auto imageStatistics(const std::filesystem::path &path) -> std::optional<ImageStatistics> {
	int width{};
	int height{};
	int channels{};
	const auto source = path.string();
	auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
		stbi_load(source.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
	if (!pixels || width <= 0 || height <= 0) { return std::nullopt; }

	const auto pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	auto luminance = std::vector<double>{};
	luminance.reserve(pixel_count);
	for (std::size_t index{}; index < pixel_count; ++index) {
		const auto offset = index * 4U;
		luminance.push_back(0.2126 * pixels.get()[offset] + 0.7152 * pixels.get()[offset + 1U] +
			0.0722 * pixels.get()[offset + 2U]);
	}
	const double mean = std::accumulate(luminance.begin(), luminance.end(), 0.0) /
		static_cast<double>(pixel_count);
	const double variance = std::accumulate(luminance.begin(), luminance.end(), 0.0,
		[mean](double total, double value) { const double difference = value - mean; return total + difference * difference; }) /
		static_cast<double>(pixel_count);
	return ImageStatistics{
		.rgba = std::vector<stbi_uc>{pixels.get(), pixels.get() + pixel_count * 4U},
		.mean = mean, .standard_deviation = std::sqrt(variance)};
}

/// @brief Computes mean absolute byte difference for equally sized captures.
[[nodiscard]] double meanAbsoluteDifference(const ImageStatistics &left, const ImageStatistics &right) {
	if (left.rgba.size() != right.rgba.size() || left.rgba.empty()) { return 0.0; }
	const double difference = std::transform_reduce(left.rgba.begin(), left.rgba.end(), right.rgba.begin(), 0.0,
		std::plus<>{}, [](stbi_uc a, stbi_uc b) { return std::abs(static_cast<double>(a) - static_cast<double>(b)); });
	return difference / static_cast<double>(left.rgba.size());
}

} // namespace

/// @brief Verifies normal-mapped lighting differs from unlit and flat-normal rendering.
int main(int argc, char **argv) {
	const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto output_directory = std::filesystem::temp_directory_path() /
		("vve-render-lighting-capture-" + unique_suffix);
	auto error = std::error_code{};
	if (!std::filesystem::create_directories(output_directory, error) || error) { return 1; }
	const auto finish = [&output_directory](int result) {
		auto cleanup_error = std::error_code{};
		std::filesystem::remove_all(output_directory, cleanup_error);
		return cleanup_error ? 20 : result;
	};

	auto engine = vve::simple::Engine{
		vve::ApplicationName{"render-lighting-capture-tests"},
		vve::WindowSetups{vve::WindowSetup{}
			.id("main")
			.title("render-lighting-capture-tests")
			.extent(vve::PixelExtent{.width = 128, .height = 128})
			.renderer(vve::RendererId{.value = "forward"})
			.visible(false)}};
	if (!engine.init()) { return finish(2); }

	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	const bool external_model = argc > 1;
	const auto model_path = external_model ? std::filesystem::path{argv[1]} :
		std::filesystem::path{VVE_TEST_MATERIAL_MODEL};
	const auto model_scene = assets.loadScene(model_path);
	const auto flat_scene = assets.loadScene(std::filesystem::path{VVE_TEST_FLAT_MATERIAL_MODEL});
	if (!model_scene || !flat_scene) { return finish(3); }

	const auto apply_view = [&render, external_model] {
		const auto eye = external_model ? vve::Position{.value = vve::Vec3{0.0F, 6.0F, 9.0F}} :
			vve::Position{.value = vve::Vec3{0.0F, 0.25F, 2.0F}};
		const auto target = external_model ? vve::Position{.value = vve::Vec3{0.0F, 1.0F, 0.0F}} :
			vve::Position{.value = vve::Vec3{0.0F, 0.25F, 0.0F}};
		render.setCamera(vve::Camera::lookAt(eye, target), vve::PixelExtent{.width = 128, .height = 128});
		render.addDirectionalLight(vve::Direction{.value = vve::Vec3{-0.35F, -0.25F, -1.0F}},
			vve::LinearColor{.value = vve::Vec3{1.0F, 0.95F, 0.85F}},
			vve::LightIntensity{.value = 1.5F}, vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}});
	};

	render.clearScene();
	const auto model_instance = render.instantiateScene(*model_scene);
	if (!model_instance) { return finish(4); }
	const auto model_objects = render.sceneInstanceObjects(*model_instance);
	if (!model_objects || model_objects->empty()) { return finish(5); }
	apply_view();
	const auto lit_path = output_directory / "lit.png";
	if (render.sceneDirectionalLightCount() != 1U) { return finish(13); }
	if (!engine.renderFrame() || !render.captureFrameToPng(lit_path)) { return finish(6); }

	for (const auto object : *model_objects) {
		if (!render.setObjectUnlit(object, true)) { return finish(7); }
	}
	const auto unlit_path = output_directory / "unlit.png";
	if (render.sceneDirectionalLightCount() != 1U) { return finish(14); }
	if (!engine.renderFrame() || !render.captureFrameToPng(unlit_path)) { return finish(8); }

	render.clearScene();
	if (!render.instantiateScene(*flat_scene)) { return finish(9); }
	apply_view();
	const auto flat_path = output_directory / "flat.png";
	if (render.sceneDirectionalLightCount() != 1U) { return finish(15); }
	if (!engine.renderFrame() || !render.captureFrameToPng(flat_path)) { return finish(10); }

	const auto lit = imageStatistics(lit_path);
	const auto unlit = imageStatistics(unlit_path);
	const auto flat = imageStatistics(flat_path);
	if (!lit || !unlit || !flat) { return finish(11); }
	const double lit_unlit_difference = meanAbsoluteDifference(*lit, *unlit);
	const double normal_flat_difference = meanAbsoluteDifference(*lit, *flat);
	std::cout << "RenderLightingCaptureTests litMean=" << lit->mean
		<< " litStdDev=" << lit->standard_deviation
		<< " unlitMean=" << unlit->mean
		<< " unlitStdDev=" << unlit->standard_deviation
		<< " flatMean=" << flat->mean
		<< " flatStdDev=" << flat->standard_deviation
		<< " litUnlitMeanAbsDiff=" << lit_unlit_difference
		<< " normalFlatMeanAbsDiff=" << normal_flat_difference << '\n';
	if (lit->standard_deviation <= 1.0 || lit->mean >= 250.0 ||
		lit_unlit_difference <= 0.01 || normal_flat_difference <= 0.01) {
		return finish(12);
	}
	return finish(0);
}
