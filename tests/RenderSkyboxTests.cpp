/**
 * @file
 * @brief Skybox checks: CPU errors and resource counts, then captures looking up, down and ahead at a cube.
 *
 * The panorama fixture is red in its top half and green in its bottom half, so up must be red and down green.
 */
import std;
import VVEngine.Simple;
import VVE.TestSupport;

namespace {

/// @brief Renders one frame from the origin towards a target and returns the captured pixels.
[[nodiscard]] auto captureTowards(vve::simple::Engine &engine, vve::Vec3 target, const std::filesystem::path &path)
	-> std::optional<std::vector<unsigned char>> {
	using namespace vve;
	auto &render = engine.renderSystem();
	render.setCamera(Camera::lookAt(Position{}, Position{.value = target}));
	if (!engine.renderFrame() || !render.captureFrameToPng(path)) { return std::nullopt; }
	return test::imagePixels(path);
}

/// @brief Reports whether one pixel matches an RGB colour within a tolerance.
[[nodiscard]] bool near(const unsigned char *pixel, std::array<int, 3> rgb) {
	return std::ranges::all_of(std::views::iota(0, 3), [&](int channel) { return std::abs(pixel[channel] - rgb[channel]) <= 40; });
}

} // namespace

/// @brief Runs the CPU checks before GPU initialization, then the three captures.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto panorama = directory / "red_over_green.ppm";
	{
		std::ofstream file{panorama, std::ios::binary};
		file << "P6\n16 8\n255\n";
		for (const auto y : std::views::iota(0, 8)) {
			for ([[maybe_unused]] const auto x : std::views::iota(0, 16)) {
				for (const auto channel : y < 4 ? std::array{255, 0, 0} : std::array{0, 255, 0}) { file.put(static_cast<char>(channel)); }
			}
		}
		if (!file) { return 1; }
	}
	constexpr int extent = 64;
	auto engine = test::hiddenEngine("render-skybox-tests", PixelExtent{extent, extent});
	auto &render = engine.renderSystem();
	// CPU: a missing file fails; set, clear and clearScene change the texture count.
	const auto missing = render.setSkybox(directory / "missing.png");
	if (missing || missing.error() != Error::io_error || render.sceneTextureCount() != 0U) { return 2; }
	if (!render.setSkybox(panorama) || render.sceneTextureCount() != 1U || !render.setSkybox(panorama) ||
		render.sceneTextureCount() != 1U || render.sceneInstanceCount() != 1U) { return 3; }
	render.clearSkybox();
	const bool cleared = render.sceneTextureCount() == 0U && render.sceneMeshCount() == 0U && render.sceneMaterialCount() == 0U;
	if (!render.setSkybox(panorama)) { return 4; }
	render.clearScene();
	if (!cleared || render.sceneTextureCount() != 0U || render.sceneInstanceCount() != 0U) { return 5; }
	std::println("[RenderSkyboxTests] cpu missing=io_error set_clear=ok");

	// GPU: the sky surrounds the camera and lies behind every object.
	if (!engine.init() || !render.setSkybox(panorama)) { return 6; }
	const auto cube = render.addCuboid(Vec3{-0.5F, -0.5F, -0.5F}, Vec3{0.5F, 0.5F, 0.5F}, LinearColor{.value = oneVec3()},
		Transform{.translation = Position{.value = Vec3{0, 0, -3}}});
	if (!cube || !render.setObjectUnlit(*cube, true)) { return 7; }
	const auto up = captureTowards(engine, Vec3{0, 1, 0}, directory / "up.png");
	const auto down = captureTowards(engine, Vec3{0, -1, 0}, directory / "down.png");
	const auto ahead = captureTowards(engine, Vec3{0, 0, -1}, directory / "ahead.png");
	if (!up || !down || !ahead) { return 8; }
	const auto *up_centre = test::pixelAt(up->data(), extent, extent / 2, extent / 2);
	const auto *down_centre = test::pixelAt(down->data(), extent, extent / 2, extent / 2);
	const auto *ahead_centre = test::pixelAt(ahead->data(), extent, extent / 2, extent / 2);
	const auto *ahead_top = test::pixelAt(ahead->data(), extent, 2, 2);
	const auto *ahead_bottom = test::pixelAt(ahead->data(), extent, 2, extent - 3);
	const bool ok = near(up_centre, {255, 0, 0}) && near(down_centre, {0, 255, 0}) && near(ahead_centre, {255, 255, 255}) &&
		near(ahead_top, {255, 0, 0}) && near(ahead_bottom, {0, 255, 0});
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderSkyboxTests] up={},{},{} down={},{},{} ahead_centre={},{},{} ahead_top={},{},{} ahead_bottom={},{},{} validation_errors={}",
		up_centre[0], up_centre[1], up_centre[2], down_centre[0], down_centre[1], down_centre[2],
		ahead_centre[0], ahead_centre[1], ahead_centre[2], ahead_top[0], ahead_top[1], ahead_top[2],
		ahead_bottom[0], ahead_bottom[1], ahead_bottom[2], render.forward().validationErrorCount());
	return ok && render.forward().validationErrorCount() == 0U ? 0 : 9;
}
