/** @file @brief Disabled-GI capture equivalence with opaque, emissive, sky and transparent content at both coverage counts. */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {

/// @brief Renders a fixed scene using existing lighting only; retains pass and validation evidence alongside the PNG.
[[nodiscard]] auto capture(vve::RendererBackend backend, vve::Antialiasing antialiasing,
	const std::filesystem::path &directory, bool zeroStrength = false) -> std::optional<std::vector<unsigned char>> {
	using namespace vve;
	auto engine = test::hiddenEngine("hybrid-gi-equivalence", PixelExtent{128, 128});
	auto &render = engine.renderSystem();
	if (!engine.init() || !render.setRendererBackend(backend) || !render.setAntialiasing(antialiasing)) { return {}; }
	if (!render.setGiSettings(GiSettings{.enabled = zeroStrength,.indirect_strength = zeroStrength ? 0.0F : 1.0F})) { return {}; }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 2, 6}}, Position{.value = Vec3{0, 0.4F, 0}}));
	render.setDirectionalLight(Direction{.value = Vec3{-0.4F, -1, -0.6F}}, LinearColor{}, LightIntensity{1.5F}, LinearColor{.value = Vec3{0.04F}});
	if (!render.setAnimationTime(AnimationTime{}) || !render.setMaterialLighting(MaterialLighting::physically_based)) { return {}; }
	const auto opaque = render.addCuboid(Vec3{-0.65F}, Vec3{0.65F}, LinearColor{.value = Vec3{0.7F, 0.25F, 0.08F}},
		Transform{.translation = Position{.value = Vec3{-0.8F, 0, 0}}});
	const auto transparent = render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{.value = Vec3{0.1F, 0.3F, 0.9F}},
		Transform{.translation = Position{.value = Vec3{-0.5F, 0, 1.1F}}});
	if (!opaque || !transparent || !render.addPlane(Vec2{2, 2}, LinearColor{.value = Vec3{0.45F}},
		Transform{.translation = Position{.value = Vec3{0, -0.66F, 0}}}) || !render.setObjectOpacity(*transparent, 0.45F)) { return {}; }
	const auto scene = engine.assets().loadScene(directory / "emissive.obj");
	if (!scene || !render.instantiateScene(*scene) || !render.setSkybox(directory / "sky.ppm")) { return {}; }
	// Verify the fixture actually imports emission rather than merely drawing a bright base colour.
	if (!std::ranges::any_of(render.renderMaterials(), [](const auto &material) { return material.emissive.value.y > 1.0F; })) { return {}; }
	const auto name = backend == RendererBackend::hybrid_gi ? (zeroStrength ? "hybrid-gi-zero" : "hybrid-gi-disabled") : "deferred";
	const auto path = directory / (std::string{name} + ".png");
	// Exercise both fenced slots and capture a completed steady frame, with an explicit paused animation clock.
	if (!engine.renderFrame() || !engine.renderFrame() || !render.captureFrameToPng(path)) { return {}; }
	using Pass = simple::ForwardRenderer::RecordedPass;
	const auto &renderer = render.forward();
	const auto passes = renderer.lastRecordedPassOrder();
	const auto geometry = std::ranges::find(passes, Pass::gbuffer), lighting = std::ranges::find(passes, Pass::deferred_lighting);
	const auto compose = std::ranges::find(passes, Pass::forward_transparency);
	if (std::ranges::count(passes, Pass::gbuffer) != 1 || std::ranges::count(passes, Pass::deferred_lighting) != 1 ||
		std::ranges::count(passes, Pass::forward_color) != 0 || compose == passes.end() || geometry >= lighting || lighting >= compose ||
		render.rendererBackend() != backend || render.renderedFrameCount() != 2U) { return {}; }
	// Attribute and depth attachments must keep the requested coverage samples, including at silhouettes.
	for (const auto &target : renderer.targets) {
		for (const auto &image : target.gbufferImages) {
			if (image.image == VK_NULL_HANDLE || image.format != VK_FORMAT_R16G16B16A16_SFLOAT ||
				image.extent.width != 128U || image.extent.height != 128U || image.samples != renderer.sampleCount()) { return {}; }
		}
		if (target.depthImage.format != VK_FORMAT_D32_SFLOAT || target.depthImage.samples != renderer.sampleCount()) { return {}; }
	}
	VkPhysicalDeviceProperties device{};
	vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice, &device);
	auto pixels = test::imagePixels(path);
	if (!pixels || pixels->size() != 128U * 128U * 4U) { return {}; }
	const bool validation = renderer.validationActive();
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[HybridGiEquivalenceTests] renderer={} device={} samples={} gbuffer=1 deferred_lighting=1 gi_active=0 ao_active=0 validation_active={} validation_errors={} capture={}",
		name, std::string_view{device.deviceName}, antialiasing == Antialiasing::msaa4 ? 4 : 1, validation, renderer.validationErrorCount(), path.string());
	return renderer.validationErrorCount() == 0U ? std::move(pixels) : std::nullopt;
}

} // namespace

/// @brief Compares all pixels and an object mask with the existing deferred mean-error tolerance of three byte units.
int main(int argc, char **argv) {
	using namespace vve;
	const bool multisample = argc == 2 && std::string_view{argv[1]} == "--msaa4";
	if (argc > 1 && !multisample) { return 3; }
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR} / (multisample ? "msaa4" : "none");
	// Self-contained OBJ/MTL and panorama fixtures keep exposure, materials and animation identical in both runs.
	(void)test::writeFixture(directory, "emissive.mtl", "newmtl glow\nKd 0.02 0.04 0.02\nKe 0.1 1.2 0.2\n");
	(void)test::writeFixture(directory, "emissive.obj", "mtllib emissive.mtl\no emitter\nusemtl glow\n"
		"v 0.3 -0.5 0\nv 1.5 -0.5 0\nv 0.9 1.4 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
	(void)test::writeFixture(directory, "sky.ppm", std::string{"P6\n2 1\n255\n"} + std::string{"\x20\x50\xa0\x20\x50\xa0", 6});
	const auto aa = multisample ? Antialiasing::msaa4 : Antialiasing::none;
	const auto deferred = capture(RendererBackend::deferred, aa, directory);
	const auto hybrid = capture(RendererBackend::hybrid_gi, aa, directory);
	const auto zero = capture(RendererBackend::hybrid_gi, aa, directory, true);
	if (!deferred || !hybrid || !zero) { return 1; }
	auto mask = std::vector<std::size_t>{}, surface = std::vector<std::size_t>{};
	unsigned maximum{};
	// A sky-relative object mask prevents the background from diluting errors on actual rendered surfaces.
	for (const auto pixel : std::views::iota(std::size_t{}, std::size_t{128U * 128U})) {
		const auto offset = pixel * 4U;
		mask.push_back(offset);
		bool object{};
		for (const auto channel : std::views::iota(0U, 3U)) {
			maximum = std::max(maximum, static_cast<unsigned>(std::abs(static_cast<int>((*deferred)[offset + channel]) - (*hybrid)[offset + channel])));
			object |= std::abs(static_cast<int>((*deferred)[offset + channel]) - (*deferred)[channel]) > 8;
		}
		if (object) { surface.push_back(offset); }
	}
	if (surface.size() < 100U) { return 2; }
	const auto mean = test::meanAbsoluteDifference(*deferred, *hybrid, mask);
	const auto object_mean = test::meanAbsoluteDifference(*deferred, *hybrid, surface);
	const auto receipt = std::format("mean_absolute_rgba_difference={:.5f} surface_difference={:.5f} max_rgb_error={} surface_pixels={} mean_limit=3 surface_limit=3\n",
		mean, object_mean, maximum, surface.size());
	std::print("[HybridGiEquivalenceTests] {}", receipt);
	(void)test::writeFixture(directory, "comparison.txt", receipt);
	const auto zero_mean = test::meanAbsoluteDifference(*deferred, *zero, mask);
	std::println("[HybridGiEquivalenceTests] zero_strength_difference={} limit=3", zero_mean);
	return mean <= 3.0 && object_mean <= 3.0 && zero_mean <= 3.0 ? 0 : 2;
}
