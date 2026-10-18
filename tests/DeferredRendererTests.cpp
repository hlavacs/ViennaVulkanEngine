/** @file @brief Paired deterministic captures exercise real deferred passes, depth, normals, lights and material composition. */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {

using Captures = std::map<std::string, std::vector<unsigned char>>; ///< RGBA captures indexed by deterministic scene state.

/// @brief Checks actual recorded passes and window attachments rather than trusting the retained backend name.
[[nodiscard]] bool checkPasses(const vve::simple::ForwardRenderer &renderer, vve::RendererBackend backend) {
	using Pass = vve::simple::ForwardRenderer::RecordedPass;
	const auto passes = renderer.lastRecordedPassOrder();
	const bool deferred = backend == vve::RendererBackend::deferred;
	if (std::ranges::count(passes, Pass::gbuffer) != (deferred ? 1 : 0) ||
		std::ranges::count(passes, Pass::deferred_lighting) != (deferred ? 1 : 0) ||
		std::ranges::count(passes, Pass::forward_color) != (deferred ? 0 : 1)) { return false; }
	if (!deferred) { return true; }
	const auto geometry = std::ranges::find(passes, Pass::gbuffer), lighting = std::ranges::find(passes, Pass::deferred_lighting);
	if (geometry >= lighting) { return false; }
	// Every light finishes its depth work before geometry; colour composition and display effects follow lighting.
	for (auto pass = passes.begin(); pass != passes.end(); ++pass) {
		const bool shadow = *pass == Pass::directional_shadow || *pass == Pass::spot_shadow || *pass == Pass::point_shadow;
		const bool composition = *pass == Pass::forward_transparency || *pass == Pass::post_process || *pass == Pass::gui;
		if ((shadow && pass >= geometry) || (composition && pass <= lighting)) { return false; }
	}
	// The final colour may resolve; surface attributes always keep their original samples and window size.
	for (const auto &target : renderer.targets) {
		for (const auto index : std::views::iota(std::size_t{}, target.gbufferImages.size())) {
			const auto &image = target.gbufferImages[index];
			if (image.image == VK_NULL_HANDLE || image.format != renderer.gbufferFormats[index] ||
				image.extent.width != target.swapchain.extent.width || image.extent.height != target.swapchain.extent.height ||
				image.samples != target.depthImage.samples) { return false; }
		}
		if (target.depthImage.format != VK_FORMAT_D32_SFLOAT) { return false; }
	}
	return true;
}

/// @brief Captures a bounded frame and records the pass diagnostics alongside its retained PNG.
[[nodiscard]] bool capture(vve::simple::Engine &engine, const std::filesystem::path &directory,
	std::string name, Captures &captures, vve::RendererBackend backend) {
	auto &render = engine.renderSystem();
	const auto path = directory / (name + ".png");
	if (!engine.renderFrame() || !render.captureFrameToPng(path) || !checkPasses(render.forward(), backend)) {
		std::println("[DeferredRendererTests] capture_failed={} renderer={}", name, static_cast<int>(backend));
		return false;
	}
	auto pixels = vve::test::imagePixels(path);
	if (!pixels || pixels->size() != 128U * 128U * 4U) { return false; }
	captures.emplace(std::move(name), std::move(*pixels));
	using Pass = vve::simple::ForwardRenderer::RecordedPass;
	const auto passes = render.forward().lastRecordedPassOrder();
	std::println("[DeferredRendererTests] renderer={} gbuffer={} deferred_lighting={} validation_errors={} capture={}",
		static_cast<int>(backend), std::ranges::count(passes, Pass::gbuffer), std::ranges::count(passes, Pass::deferred_lighting),
		render.forward().validationErrorCount(), path.string());
	return render.forward().validationErrorCount() == 0U;
}

/// @brief Builds identical scenes for either backend and retains captures of each material and scene transition.
[[nodiscard]] auto renderScenes(vve::RendererBackend backend, vve::Antialiasing antialiasing,
	const std::filesystem::path &directory) -> std::optional<Captures> {
	using namespace vve;
	std::filesystem::create_directories(directory);
	auto engine = test::hiddenEngine("deferred-comparison-tests", PixelExtent{128, 128});
	auto &render = engine.renderSystem();
	if (!render.setRendererBackend(backend) || !render.setAntialiasing(antialiasing) || !engine.init()) { return {}; }
	auto captures = Captures{};
	const auto save = [&](std::string name) { return capture(engine, directory, std::move(name), captures, backend); };
	const auto dark = LinearColor{.value = Vec3{0}};
	render.setBackgroundColor(LinearColor{.value = Vec3{0.03F, 0.04F, 0.06F}});
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 6, 6}}, Position{}));
	const auto floor = render.addPlane(Vec2{4, 4}, LinearColor{.value = Vec3{0.5F}});
	const auto cube = render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{.value = Vec3{0.7F, 0.3F, 0.1F}},
		Transform{.translation = Position{.value = Vec3{0, 1.5F, 0}}});
	if (!floor || !cube) { return {}; }
	// Moving one caster isolates both shadow sampling and position reconstruction for all three light types.
	for (const auto type : {0, 1, 2}) {
		render.clearLights();
		if (type == 0) { render.setDirectionalLight(Direction{.value = Vec3{0, -1, 0}}, LinearColor{}, LightIntensity{1}, dark); }
		if (type == 1) { render.setPointLight(Position{.value = Vec3{0, 5, 0}}, LinearColor{}, LightIntensity{4}, LightRange{10}, dark); }
		if (type == 2) { render.setSpotLight(Position{.value = Vec3{0, 5, 0}}, Direction{.value = Vec3{0, -1, 0}},
			LinearColor{}, LightIntensity{4}, LightRange{10}, SpotConeAngle{0.5F}, dark); }
		if (!render.setObjectTransform(*cube, Transform{.translation = Position{.value = Vec3{0, 1.5F, 0}}}) ||
			!save(std::format("light-{}-shadow", type)) ||
			!render.setObjectTransform(*cube, Transform{.translation = Position{.value = Vec3{3, 1.5F, 0}}}) ||
			!save(std::format("light-{}-clear", type))) { return {}; }
	}
	// An imported tangent-space normal map and a nonuniform world transform expose coordinate-space mistakes.
	render.clearScene();
	const auto scene = engine.assets().loadScene(std::filesystem::path{VVE_TEST_MATERIAL_MODEL});
	if (!scene || !render.instantiateScene(*scene)) { return {}; }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0.25F, 2}}, Position{.value = Vec3{0, 0.25F, 0}}));
	render.setDirectionalLight(Direction{.value = Vec3{-0.35F, -0.25F, -1}}, LinearColor{}, LightIntensity{1.5F}, LinearColor{.value = Vec3{0.04F}});
	if (!save("normal-map")) { return {}; }
	render.clearScene();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	const auto sphere = render.addSphere(0.8F, LinearColor{.value = Vec3{0.7F, 0.4F, 0.15F}},
		Transform{.scale = Scale{.value = Vec3{1, 0.85F, 0.6F}}});
	if (!sphere || !render.setObjectSurface(*sphere, SurfaceMaterial{0.15F, 0.7F})) { return {}; }
	render.setDirectionalLight(Direction{.value = Vec3{0.4F, -0.2F, -1}}, LinearColor{}, LightIntensity{2}, dark);
	if (!save("material-classic") || !render.setMaterialLighting(MaterialLighting::physically_based) || !save("material-pbr")) { return {}; }
	const auto sky = test::writeFixture(directory, "sky.ppm", std::string{"P6\n2 1\n255\n"} + std::string{"\xff\x20\0\0\x20\xff", 6});
	if (!render.setSkybox(sky) || !save("material-sky") || !render.setObjectSurface(*sphere, SurfaceMaterial{0.85F, 1}) ||
		!save("material-rough") || !render.setObjectUnlit(*sphere, true) || !save("material-unlit")) { return {}; }

	// Transparent quads cover opaque red; a green quad behind it must fail the retained geometry depth test.
	render.clearScene();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 4, 0}}, Position{}));
	const auto red = render.addPlane(Vec2{2, 2}, LinearColor{.value = Vec3{1, 0, 0}});
	const auto blue = render.addPlane(Vec2{0.7F, 0.7F}, LinearColor{.value = Vec3{0, 0, 1}}, Transform{.translation = Position{.value = Vec3{0, 1, 0}}});
	const auto green = render.addPlane(Vec2{1, 1}, LinearColor{.value = Vec3{0, 1, 0}}, Transform{.translation = Position{.value = Vec3{0, -1, 0}}});
	if (!red || !blue || !green) { return {}; }
	for (const auto handle : {*red, *blue, *green}) { if (!render.setObjectUnlit(handle, true)) { return {}; } }
	if (!render.setObjectOpacity(*blue, 0.5F) || !render.setObjectOpacity(*green, 0.5F) || !save("transparent-depth")) { return {}; }
	const auto *centre = test::pixelAt(captures.at("transparent-depth").data(), 128, 64, 64);
	if (centre[0] < 150 || centre[1] > 3 || centre[2] < 150) { return {}; }
	// Opaque water must carry its own BRDF flags through the G-buffer, even when an unlit object draws last.
	render.clearScene();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 2.5F, 5}}, Position{}));
	if (!render.setMaterialLighting(MaterialLighting::physically_based) || !render.setSkybox(sky)) { return {}; }
	render.setDirectionalLight(Direction{.value = Vec3{0.4F, -1, -0.3F}}, LinearColor{}, LightIntensity{2}, LinearColor{.value = Vec3{0.1F}});
	const auto water = render.addOcean(OceanDescriptor{.half_extent = {4, 4}, .wave_height = 0.35F, .wavelength = 3.2F, .speed = 1.3F});
	const auto marker = render.addCuboid(Vec3{-0.3F}, Vec3{0.3F}, LinearColor{.value = Vec3{1, 0.5F, 0.1F}},
		Transform{.translation = Position{.value = Vec3{1.5F, 0.5F, 0}}});
	if (!water || !marker || !render.setObjectOpacity(*water, 1) || !render.setObjectUnlit(*marker, true) ||
		!render.setAnimationTime(AnimationTime{}) || !save("opaque-ocean0") ||
		!render.setAnimationTime(AnimationTime{1}) || !save("opaque-ocean1")) { return {}; }
	// Dynamic vertex replacement runs through both fenced upload slots before resource removal.
	render.clearScene();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	const auto triangle = render.addMesh(MeshDescriptor{.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}, .indices = {0, 1, 2}});
	if (!triangle || !render.setObjectUnlit(*triangle, true) || !save("dynamic-before") ||
		!render.setObjectMeshPositions(*triangle, {{-1, -1, 0}, {1, -1, 0}, {0.7F, 1, 0}}) || !save("dynamic-after") ||
		!render.removeObject(*triangle) || !save("removed")) { return {}; }
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[DeferredRendererTests] renderer={} cleanup_validation_errors={} frames={}", static_cast<int>(backend),
		render.forward().validationErrorCount(), render.renderedFrameCount());
	return render.forward().validationErrorCount() == 0U ? std::optional{std::move(captures)} : std::nullopt;
}

} // namespace

/// @brief Requires both compiled backends to reproduce each scene within a documented HDR-quantization tolerance.
int main(int argc, char **argv) {
	const bool multisample = argc == 2 && std::string_view{argv[1]} == "--msaa4";
	if (argc > 1 && !multisample) { return 3; }
	const auto antialiasing = multisample ? vve::Antialiasing::msaa4 : vve::Antialiasing::none;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR} / (multisample ? "msaa4" : "none");
	const auto forward = renderScenes(vve::RendererBackend::forward, antialiasing, directory / "forward");
	const auto deferred = renderScenes(vve::RendererBackend::deferred, antialiasing, directory / "deferred");
	if (!forward || !deferred || forward->size() != deferred->size()) { return 1; }
	auto report = std::ofstream{directory / "comparison.txt"};
	auto mask = std::vector<std::size_t>{};
	// Every RGB pixel participates, including clear background, silhouettes and transparent edges.
	for (const auto pixel : std::views::iota(std::size_t{}, std::size_t{128U * 128U})) { mask.push_back(pixel * 4U); }
	bool success{true};
	for (const auto &[name, pixels] : *forward) {
		const double difference = vve::test::meanAbsoluteDifference(pixels, deferred->at(name), mask);
		auto surface = std::vector<std::size_t>{};
		// An object-only metric prevents clear background from hiding a material or normal-map regression.
		for (const auto offset : mask) {
			if (std::ranges::any_of(std::views::iota(0U, 3U), [&](auto channel) {
				return std::abs(static_cast<int>(pixels[offset + channel]) - pixels[channel]) > 8;
			})) { surface.push_back(offset); }
		}
		const auto surface_difference = surface.empty() ? difference : vve::test::meanAbsoluteDifference(pixels, deferred->at(name), surface);
		// True per-sample UVs filter the high-frequency normal map differently from forward pixel-frequency shading.
		const double surface_tolerance = multisample && name == "normal-map" ? 5.0 : 3.0;
		const auto line = std::format("scene={} mean_absolute_rgba_difference={:.5f} surface_difference={:.5f} surface_pixels={} maximum=3 surface_maximum={} capture_forward=forward/{}.png capture_deferred=deferred/{}.png",
			name, difference, surface_difference, surface.size(), surface_tolerance, name, name);
		std::println("[DeferredRendererTests] {}", line);
		report << line << '\n';
		success &= difference <= 3.0 && surface_difference <= surface_tolerance; // Half-float factors and reconstructed depth can round differently.
	}
	return success && report.good() ? 0 : 2;
}
