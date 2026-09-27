#include <stb_image.h>

/**
 * @file
 * @brief Captured-image coverage for Sponza base-color texture rendering.
 *
 * Functional objects:
 * - ImageStatistics retains luminance statistics for one captured PNG.
 * - assetRoot finds the repository Sponza asset from the cwd or executable path.
 * - main captures lit and unlit Sponza images and verifies texture-driven detail.
 */

import std;

import VEEngine;
import VEEngine.Simple;

namespace {

constexpr auto sponzaSceneRelativePath = "assets/sea_keep_lonely_watcher/scene.gltf";

/// @brief Mean and population deviation of decoded image luminance.
struct ImageStatistics {
	double mean{};               ///< Mean Rec. 709 luminance in byte units.
	double standard_deviation{}; ///< Population standard deviation of luminance.
};

/// @brief Finds the repository-style asset root from either the cwd or executable location.
[[nodiscard]] std::filesystem::path assetRoot(char *argv0) {
	auto containsSponzaScene = [](const std::filesystem::path &candidate) {
		return std::filesystem::exists(candidate / sponzaSceneRelativePath);
	};
	if (const auto cwd = std::filesystem::current_path(); containsSponzaScene(cwd)) { return cwd; }
	if (argv0 == nullptr) { return {}; }
	auto executable = std::filesystem::absolute(std::filesystem::path{argv0});
	if (std::filesystem::exists(executable)) { executable = std::filesystem::weakly_canonical(executable); }
	for (auto candidate = executable.parent_path(); !candidate.empty(); candidate = candidate.parent_path()) {
		if (containsSponzaScene(candidate)) { return candidate; }
		if (candidate == candidate.root_path()) { break; }
	}
	return {};
}

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
	return ImageStatistics{.mean = mean, .standard_deviation = std::sqrt(variance)};
}

} // namespace

/// @brief Verifies Sponza remains detailed with and without directional lighting.
int main(int argc, char **argv) {
	const auto scene_path = assetRoot(argc > 0 ? argv[0] : nullptr) / sponzaSceneRelativePath;
	if (!std::filesystem::exists(scene_path)) {
		std::cout << "RenderSponzaCaptureTests skipped: asset not found\n";
		return 0;
	}

	const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto output_directory = std::filesystem::temp_directory_path() /
		("vve-render-sponza-capture-" + unique_suffix);
	auto error = std::error_code{};
	if (!std::filesystem::create_directories(output_directory, error) || error) { return 1; }
	const auto finish = [&output_directory](int result) {
		auto cleanup_error = std::error_code{};
		std::filesystem::remove_all(output_directory, cleanup_error);
		return cleanup_error ? 20 : result;
	};

	auto engine = vve::simple::Engine{
		vve::ApplicationName{"render-sponza-capture-tests"},
		vve::WindowSetups{vve::WindowSetup{}
			.id("main")
			.title("render-sponza-capture-tests")
			.extent(vve::PixelExtent{.width = 256, .height = 256})
			.renderer(vve::RendererId{.value = "forward"})
			.visible(false)}};
	if (!engine.init()) { return finish(2); }

	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	const auto scene = assets.loadScene(scene_path);
	if (!scene) { return finish(3); }
	const auto instance = render.instantiateScene(*scene);
	if (!instance) { return finish(4); }
	const auto objects = render.sceneInstanceObjects(*instance);
	if (!objects || objects->empty()) { return finish(5); }

	const auto eye = vve::Position{.value = vve::Vec3{0.0F, 6.0F, 9.0F}};
	const auto target = vve::Position{.value = vve::Vec3{0.0F, 6.0F, 0.0F}};
	render.setCamera(vve::Camera::lookAt(eye, target), vve::PixelExtent{.width = 256, .height = 256});
	render.addDirectionalLight(vve::Direction{.value = vve::Vec3{-0.35F, -0.25F, -1.0F}},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.95F, 0.85F}},
		vve::LightIntensity{.value = 1.5F}, vve::LinearColor{.value = vve::Vec3{0.04F, 0.04F, 0.04F}});
	if (render.sceneDirectionalLightCount() != 1U) { return finish(6); }

	const auto lit_path = output_directory / "lit.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(lit_path)) { return finish(7); }
	for (const auto object : *objects) {
		if (!render.setObjectUnlit(object, true)) { return finish(8); }
	}
	const auto unlit_path = output_directory / "unlit.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(unlit_path)) { return finish(9); }

	const auto lit = imageStatistics(lit_path);
	const auto unlit = imageStatistics(unlit_path);
	if (!lit || !unlit) { return finish(10); }
	std::cout << "RenderSponzaCaptureTests litMean=" << lit->mean
		<< " litStdDev=" << lit->standard_deviation
		<< " unlitMean=" << unlit->mean
		<< " unlitStdDev=" << unlit->standard_deviation
		<< " sceneTextureCount=" << render.sceneTextureCount()
		<< " gpuTextureCount=" << render.gpuTextureCount()
		<< " gpuMaterialCount=" << render.gpuMaterialCount() << '\n';
	if (lit->standard_deviation <= 8.0 || lit->mean <= 10.0 || lit->mean >= 245.0 ||
		unlit->standard_deviation <= 8.0 || unlit->mean <= 10.0 || unlit->mean >= 245.0) {
		return finish(11);
	}
	return finish(0);
}
