/** @file @brief addTerrain contracts, culling bounds and tessellated captures that follow the height grid. */
import std;
import VVEngine;
import VVEngine.Simple;
import VVE.TestSupport;

namespace {

/// @brief A square grid of 0.25-unit cells (16 units wide), flat or with a smooth hill of the given height in its middle.
[[nodiscard]] vve::TerrainDescriptor grid(std::uint32_t samples, float hill) {
	vve::TerrainDescriptor terrain{.columns = samples, .rows = samples, .cell_size = 0.25F};
	for (std::uint32_t z{}; z < samples; ++z) {
		for (std::uint32_t x{}; x < samples; ++x) {
			const float dx = (static_cast<float>(x) / static_cast<float>(samples - 1U) - 0.5F) * 4.0F;
			const float dz = (static_cast<float>(z) / static_cast<float>(samples - 1U) - 0.5F) * 4.0F;
			terrain.heights.push_back(hill * std::exp(-(dx * dx + dz * dz)));
		}
	}
	return terrain;
}

/// @brief Rejects malformed grids before GPU initialization and keeps the full height range in the culling box.
[[nodiscard]] bool cpuContracts() {
	using namespace vve;
	Engine<> engine;
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	const auto valid = grid(65U, 2.0F);
	std::size_t rejected{};
	const auto reject = [&](TerrainDescriptor bad) {
		const auto result = render.addTerrain(std::move(bad));
		if (!result && result.error() == Error::invalid_argument) { ++rejected; }
	};
	auto bad = valid;
	bad.columns = 1U;
	reject(bad);
	bad = valid;
	bad.heights.pop_back();
	reject(bad);
	bad = valid;
	bad.heights[7] = std::numeric_limits<float>::quiet_NaN();
	reject(bad);
	bad = valid;
	bad.cell_size = 0.0F;
	reject(bad);
	bad = valid;
	bad.color.value.x = -1.0F;
	reject(bad);
	if (rejected != 5U || render.sceneInstanceCount() != 0U) { return false; }
	const auto terrain = render.addTerrain(valid);
	const auto box = terrain ? render.objectBounds(*terrain) : std::unexpected(Error::missing_object);
	std::println("[RenderTerrainTests] cpu_rejected={} bounds_x={}..{} bounds_y={}..{}", rejected,
		box ? box->minimum.value.x : -1.0F, box ? box->maximum.value.x : -1.0F, box ? box->minimum.value.y : -1.0F, box ? box->maximum.value.y : -1.0F);
	if (!box || !box->valid || box->minimum.value.x != 0.0F || box->maximum.value.x != 16.0F || box->maximum.value.z != 16.0F ||
		box->maximum.value.y < 1.99F || box->minimum.value.y > 0.01F) { return false; }
	return render.removeObject(*terrain).has_value() && render.sceneInstanceCount() == 0U;
}

/// @brief Captures one frame and returns its RGBA pixels.
[[nodiscard]] auto capture(vve::simple::Engine &engine, const std::filesystem::path &path)
	-> std::optional<std::vector<unsigned char>> {
	if (!engine.renderFrame() || !engine.renderSystem().captureFrameToPng(path)) { return {}; }
	return vve::test::imagePixels(path);
}

} // namespace

/// @brief A lit hill shades unevenly where the flat grid shades evenly; both differ from the empty background.
int main() {
	using namespace vve;
	if (!cpuContracts()) { return 1; }
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	constexpr int extent = 96;
	auto engine = test::hiddenEngine("render-terrain-tests", PixelExtent{extent, extent});
	test::configureRenderer(engine.renderSystem());
	if (!engine.init()) { return 2; }
	auto &render = engine.renderSystem();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{8, 11, 19}}, Position{.value = Vec3{8, 0, 8}}));
	render.setDirectionalLight(Direction{.value = Vec3{0.7F, -0.6F, 0.2F}}, LinearColor{}, LightIntensity{2}, LinearColor{.value = Vec3{0.05F}});
	const auto empty = capture(engine, directory / "empty.png");
	if (!empty || empty->size() != extent * extent * 4U) { return 3; }
	if (!render.forward().terrainSupported()) {
		std::println("[RenderTerrainTests] skipped: no tessellationShader feature");
		render.waitIdle();
		engine.gui().shutdownVulkan();
		render.shutdown();
		return 0;
	}
	// The centre 40 x 40 pixels see only terrain in both scenes.
	auto centre = std::vector<std::size_t>{};
	for (int y = 28; y < 68; ++y) {
		for (int x = 28; x < 68; ++x) { centre.push_back(static_cast<std::size_t>((y * extent + x) * 4)); }
	}
	const auto flat = render.addTerrain(grid(65U, 0.0F));
	if (!flat) { return 4; }
	const auto flatPixels = capture(engine, directory / "flat.png");
	if (!flatPixels || !render.removeObject(*flat)) { return 5; }
	const auto hill = render.addTerrain(grid(65U, 4.0F));
	if (!hill) { return 6; }
	const auto hillPixels = capture(engine, directory / "hill.png");
	if (!hillPixels || render.forward().terrainHeights().size() != 65U * 65U) { return 7; }
	const auto flatStats = test::imageStatistics(*flatPixels, centre), hillStats = test::imageStatistics(*hillPixels, centre);
	const double covered = test::meanAbsoluteDifference(*empty, *flatPixels, centre);
	std::println("[RenderTerrainTests] flat_mean={:.1f} flat_sd={:.2f} hill_mean={:.1f} hill_sd={:.2f} covered_difference={:.1f}",
		flatStats.mean, flatStats.standard_deviation, hillStats.mean, hillStats.standard_deviation, covered);
	if (covered < 10.0 || flatStats.standard_deviation > 6.0 || hillStats.standard_deviation < flatStats.standard_deviation + 8.0) { return 8; }
	if (!render.removeObject(*hill)) { return 9; }
	(void)render.purgeUnusedAssets();
	const auto cleared = capture(engine, directory / "cleared.png");
	if (!cleared || render.sceneInstanceCount() != 0U || !render.forward().terrainHeights().empty() ||
		test::meanAbsoluteDifference(*empty, *cleared, centre) > 1.0) { return 10; }
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderTerrainTests] validation_errors={}", render.forward().validationErrorCount());
	return render.forward().validationErrorCount() == 0U ? 0 : 11;
}
