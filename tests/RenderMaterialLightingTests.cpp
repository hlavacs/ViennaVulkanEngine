/** @file @brief Public material validation/sharing plus deterministic GGX, roughness and panorama-lighting captures. */
import std;
import VVEngine;
import VVEngine.Simple;
import VVE.TestSupport;

namespace {

/// @brief Exercises facade validation and private-material reclamation without a window or GPU.
bool cpuContracts() {
	using namespace vve;
	Engine<> engine;
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	const auto a = render.addSphere(1, LinearColor{}), b = render.addSphere(1, LinearColor{});
	if (!a || !b || render.materialLighting() != MaterialLighting::classic || render.sceneMaterialCount() != 1U) { return false; }
	const auto invalid = render.setMaterialLighting(static_cast<MaterialLighting>(-1));
	if (invalid || invalid.error() != Error::invalid_argument || render.materialLighting() != MaterialLighting::classic) { return false; }
	if (!render.setMaterialLighting(MaterialLighting::physically_based) || render.materialLighting() != MaterialLighting::physically_based) { return false; }
	// Every change replaces this object's private material; the shared original stays for the other sphere.
	for (const auto value : {0.05F, 0.8F, 0.2F, 0.2F}) {
		if (!render.setObjectSurface(*a, SurfaceMaterial{value, 1}) || render.sceneMaterialCount() != 2U) { return false; }
	}
	for (const auto value : {-1.0F, 1.1F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
		if (render.setObjectSurface(*a, SurfaceMaterial{value, 0}) || render.setObjectSurface(*a, SurfaceMaterial{0.5F, value})) { return false; }
	}
	const auto missing = render.setObjectSurface(RenderObjectHandle{}, SurfaceMaterial{});
	if (missing || missing.error() != Error::missing_object || render.sceneMaterialCount() != 2U ||
		!render.removeObject(*a) || render.sceneMaterialCount() != 1U || !render.removeObject(*b)) { return false; }
	std::println("[RenderMaterialLightingTests] cpu_contracts=ok remaining_materials={}", render.sceneMaterialCount());
	return render.sceneMaterialCount() == 0U;
}

/// @brief Captures one completed frame and keeps its pixels for deterministic comparisons.
auto capture(vve::simple::Engine &engine, const std::filesystem::path &path) -> std::optional<std::vector<unsigned char>> {
	if (!engine.renderFrame() || !engine.renderSystem().captureFrameToPng(path)) { return {}; }
	return vve::test::imagePixels(path);
}

} // namespace

/// @brief A metal sphere reflects the sky, broadens highlights with roughness, and returns to its original state after clearing.
int main() {
	using namespace vve;
	if (!cpuContracts()) { return 1; }
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto panorama = directory / "bands.ppm";
	{
		std::ofstream file{panorama, std::ios::binary};
		file << "P6\n64 32\n255\n";
		// High-contrast angular bands make roughness filtering measurable without external assets.
		for ([[maybe_unused]] const auto y : std::views::iota(0, 32)) {
			for (const auto x : std::views::iota(0, 64)) {
				for (const auto channel : (x / 8) % 2 == 0 ? std::array{255, 30, 0} : std::array{0, 30, 255}) { file.put(static_cast<char>(channel)); }
			}
		}
		if (!file) { return 2; }
	}
	auto engine = test::hiddenEngine("render-material-lighting-tests", PixelExtent{96, 96});
	vve::test::configureRenderer(engine.renderSystem());
	if (!engine.init()) { return 3; }
	auto &render = engine.renderSystem();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	const auto sphere = render.addSphere(0.8F, LinearColor{.value = Vec3{0.8F}});
	if (!sphere || !render.setObjectSurface(*sphere, SurfaceMaterial{0.05F, 1})) { return 4; }
	render.setDirectionalLight(Direction{.value = Vec3{0.4F, -0.2F, -1}}, LinearColor{}, LightIntensity{1}, LinearColor{.value = Vec3{}});
	const auto classic = capture(engine, directory / "classic.png");
	if (!render.setMaterialLighting(MaterialLighting::physically_based)) { return 5; }
	const auto direct = capture(engine, directory / "ggx.png");
	render.clearLights();
	const auto dark = capture(engine, directory / "no-sky.png");
	if (!render.setSkybox(panorama)) { return 6; }
	const auto sharp = capture(engine, directory / "sharp.png");
	if (!render.setObjectSurface(*sphere, SurfaceMaterial{0.9F, 1})) { return 7; }
	const auto rough = capture(engine, directory / "rough.png"), repeated = capture(engine, directory / "rough-repeat.png");
	if (!render.setObjectSurface(*sphere, SurfaceMaterial{0.05F, 1})) { return 8; }
	render.clearSkybox();
	const auto cleared = capture(engine, directory / "sky-cleared.png");
	if (!classic || !direct || !dark || !sharp || !rough || !repeated || !cleared || *dark != *cleared || *rough != *repeated) { return 9; }
	auto mask = std::vector<std::size_t>{};
	// Restrict comparisons to a square wholly inside the sphere; background changes cannot satisfy the test.
	for (const auto y : std::views::iota(36, 60)) {
		for (const auto x : std::views::iota(36, 60)) { mask.push_back(static_cast<std::size_t>((y * 96 + x) * 4)); }
	}
	const auto direct_difference = test::meanAbsoluteDifference(*classic, *direct, mask);
	const auto sky_difference = test::meanAbsoluteDifference(*dark, *sharp, mask);
	const auto roughness_difference = test::meanAbsoluteDifference(*sharp, *rough, mask);
	render.clearScene();
	if (!engine.renderFrame() || render.forward().frameUniforms().environmentLighting.x >= 0) { return 10; }
	std::println("[RenderMaterialLightingTests] validation_active={}", render.forward().validationActive());
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderMaterialLightingTests] direct_difference={} sky_difference={} roughness_difference={} validation_errors={}",
		direct_difference, sky_difference, roughness_difference, render.forward().validationErrorCount());
	return direct_difference > 2 && sky_difference > 5 && roughness_difference > 3 && render.forward().validationErrorCount() == 0 ? 0 : 11;
}
