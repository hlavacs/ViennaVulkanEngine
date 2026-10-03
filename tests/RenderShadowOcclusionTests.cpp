#include <stb_image.h>

/**
 * @file
 * @brief Verifies real directional, spot and point shadows through depth readback and captured pixels.
 *
 * Functional objects:
 * - centreLuminance decodes the centre receiver patch in linear light.
 * - hasOriginSample checks the occluded or clear receiver against the rendered shadow map.
 * - main moves a cube away from the receiver for each light type and compares both frames.
 */

import std;

import VVEngine;
import VVE.TestSupport;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;

namespace {

/// @brief Returns mean linear Rec. 709 luminance of the centre 5x5 patch in a 128x128 PNG.
[[nodiscard]] auto centreLuminance(const std::filesystem::path &path) -> std::optional<double> {
	int width{};
	int height{};
	int channels{};
	const auto source = path.string();
	auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
		stbi_load(source.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
	if (!pixels || width != 128 || height != 128) { return std::nullopt; }
	const auto linear = [](stbi_uc byte) {
		const double srgb = static_cast<double>(byte) / 255.0;
		return srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
	};
	double total{};
	// PNG bytes are sRGB; decode them before comparing the shader's linear-light attenuation.
	for (const int y : std::views::iota(height / 2 - 2, height / 2 + 3)) {
		for (const int x : std::views::iota(width / 2 - 2, width / 2 + 3)) {
			const auto offset = static_cast<std::size_t>((y * width + x) * 4);
			total += 0.2126 * linear(pixels.get()[offset]) + 0.7152 * linear(pixels.get()[offset + 1U]) +
				0.0722 * linear(pixels.get()[offset + 2U]);
		}
	}
	return total / 25.0;
}

/// @brief Checks one origin sample against the known cube/plane geometry, without reproducing the CPU compare.
[[nodiscard]] bool hasOriginSample(const vve::simple::ForwardRenderer &renderer, std::uint32_t type, bool occluded) {
	const auto &samples = renderer.shadowDepthSamples;
	if (samples.size() != 1U) {
		std::cout << "RenderShadowOcclusionTests type=" << type << " sampleCount=" << samples.size() << '\n';
		return false;
	}
	const auto &sample = samples.front();
	std::cout << "RenderShadowOcclusionTests type=" << type << " occluded=" << occluded
		<< " hasGpu=" << sample.has_gpu << " expectedDepth=" << sample.expected_depth
		<< " gpuDepth=" << sample.gpu_depth << " bias=" << sample.bias << " shadowFactor=" << sample.shadow_factor << '\n';
	if (sample.light_type != type || sample.light_index != 0U || !sample.has_gpu ||
		sample.world.x != 0.0F || sample.world.y != 0.0F || sample.world.z != 0.0F ||
		!std::isfinite(sample.expected_depth) || !std::isfinite(sample.gpu_depth) ||
		!std::isfinite(sample.bias) || !std::isfinite(sample.shadow_factor)) { return false; }
	// With near=0.1, far=10 and axial depth=5, the shader's 2cm offset is 0.00008113261 in depth units.
	if (type != 3U && std::abs(sample.bias - 0.00008113261F) > 1.0e-7F) { return false; }
	return occluded ? sample.gpu_depth < sample.expected_depth - 0.001F && std::abs(sample.shadow_factor - 0.35F) < 1.0e-6F :
		std::abs(sample.gpu_depth - sample.expected_depth) < 0.002F && std::abs(sample.shadow_factor - 1.0F) < 1.0e-6F;
}

} // namespace

/// @brief Requires depth occlusion and visible shadow darkening for every supported light type.
int main() {
	const auto output_directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	auto error = std::error_code{};
	// Retain only this run's captures, in the build tree's dedicated test directory.
	std::filesystem::remove_all(output_directory, error);
	if (error) { return 1; }
	std::filesystem::create_directories(VVE_TEST_TMP_DIR, error);
	if (error) { return 2; }
	constexpr auto extent = vve::PixelExtent{.width = 128, .height = 128}; ///< Fixed capture size and camera aspect.
	auto engine = vve::test::hiddenEngine("render-shadow-occlusion-tests", extent);
	if (!engine.init()) { return 3; }
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	renderer.setGpuDebugReadback(true);
	constexpr auto grey = vve::LinearColor{.value = vve::Vec3{0.5F, 0.5F, 0.5F}};
	constexpr auto no_ambient = vve::LinearColor{.value = vve::Vec3{0.0F, 0.0F, 0.0F}};
	constexpr auto down = vve::Direction{.value = vve::Vec3{0.0F, -1.0F, 0.0F}};
	constexpr auto light_position = vve::Position{.value = vve::Vec3{0.0F, 5.0F, 0.0F}};
	constexpr std::array light_types{3U, 1U, 2U}; ///< Diagnostic tags in directional, spot, point order.
	constexpr std::array names{"directional", "spot", "point"}; ///< Stable capture names and failure context.
	// Each isolated scene has only one light; moving the cube leaves camera, materials and receiver unchanged.
	for (const auto index : std::views::iota(0U, 3U)) {
		const int failure_base = 10 + static_cast<int>(index) * 10; ///< Unique exit codes identify both case and stage.
		render.clearScene();
		renderer.scene.ambient = 0.0F; // Isolate direct light so ambient cannot mask a broken shadow compare.
		if (!render.addPlane(vve::Vec2{4.0F, 4.0F}, grey)) { return failure_base; }
		auto cube_transform = vve::Transform{.translation = vve::Position{.value = vve::Vec3{0.0F, 1.5F, 0.0F}}};
		const auto cube = render.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F}, vve::Vec3{0.5F, 0.5F, 0.5F}, grey, cube_transform);
		if (!cube) { return failure_base + 1; }
		render.setCamera(vve::Camera::lookAt(vve::Position{.value = vve::Vec3{0.0F, 6.0F, 6.0F}}, vve::Position{}));
		// Range falloff at the origin is 1/4, so perspective lights use intensity 4 to match the directional light.
		switch (light_types[index]) {
		case 3U: render.setDirectionalLight(down, vve::LinearColor{}, vve::LightIntensity{.value = 1.0F}, no_ambient); break;
		case 1U: render.setSpotLight(light_position, down, vve::LinearColor{}, vve::LightIntensity{.value = 4.0F},
			vve::LightRange{.value = 10.0F}, vve::SpotConeAngle{.radians = 0.5F}, no_ambient); break;
		case 2U: render.setPointLight(light_position, vve::LinearColor{}, vve::LightIntensity{.value = 4.0F},
			vve::LightRange{.value = 10.0F}, no_ambient); break;
		}
		const auto occluded_path = output_directory / (std::string{names[index]} + "_occluded.png");
		if (!engine.renderFrame() || !render.captureFrameToPng(occluded_path)) { return failure_base + 2; }
		if (!hasOriginSample(renderer, light_types[index], true)) { return failure_base + 3; }

		// Translate only the caster; the origin now sees the receiver plane in the shadow map.
		cube_transform.translation.value.x = 3.0F;
		if (!render.setObjectTransform(*cube, cube_transform)) { return failure_base + 4; }
		const auto clear_path = output_directory / (std::string{names[index]} + "_clear.png");
		if (!engine.renderFrame() || !render.captureFrameToPng(clear_path)) { return failure_base + 5; }
		if (!hasOriginSample(renderer, light_types[index], false)) { return failure_base + 6; }
		const auto occluded = centreLuminance(occluded_path);
		const auto clear = centreLuminance(clear_path);
		if (!occluded || !clear || *clear <= 0.0) { return failure_base + 7; }
		std::cout << "RenderShadowOcclusionTests light=" << names[index] << " occludedLuminance=" << *occluded
			<< " clearLuminance=" << *clear << " ratio=" << *occluded / *clear << " maximumRatio=0.6\n";
		if (*occluded > 0.6 * *clear) { return failure_base + 8; }
	}
	// Include all shadow passes, captures and scene resets in the validation check, also in Release.
	std::cout << "RenderShadowOcclusionTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 4; }
	return 0;
}
