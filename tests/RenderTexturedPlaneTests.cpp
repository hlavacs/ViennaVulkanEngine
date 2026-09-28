/** @file @brief Captures one textured plane and verifies all ten texture repeats on both UV axes. */
import std;
import VEEngine.Simple;
import VVE.TestSupport;

/// @brief Samples every quadrant of every tile, distinguishing repeat addressing from edge clamping.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto texture = directory / "quadrants.ppm";
	constexpr std::array colors{std::array{255, 0, 0}, std::array{0, 255, 0},
		std::array{0, 0, 255}, std::array{255, 255, 255}};
	// Broad quadrants leave their centres unaffected by the shared linear mip sampler.
	{
		std::ofstream file{texture, std::ios::binary};
		file << "P6\n16 16\n255\n";
		for (const auto y : std::views::iota(0, 16)) {
			for (const auto x : std::views::iota(0, 16)) {
				for (const auto channel : colors[(y / 8) * 2 + x / 8]) { file.put(static_cast<char>(channel)); }
			}
		}
		if (!file) { return 1; }
	}
	constexpr int extent = 256;
	auto engine = test::hiddenEngine("textured-plane-tests", PixelExtent{extent, extent});
	if (!engine.init()) { return 2; }
	auto &render = engine.renderSystem();
	// Rotate the XZ plane toward the camera so every tile has the same projected size.
	const auto rotation = std::sqrt(0.5F);
	const auto object = render.addTexturedPlane(Vec2{1, 1}, texture, Vec2{10, 10},
		Transform{.rotation = Rotation{.value = Quat{rotation, rotation, 0, 0}}});
	if (!object || !render.setObjectUnlit(*object, true)) { return 3; }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	const auto capture = directory / "plane-10x10.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(capture)) { return 4; }
	const auto pixels = test::imagePixels(capture);
	if (!pixels || pixels->size() != extent * extent * 4U) { return 5; }
	// The 60-degree camera projects the plane linearly; account for pixel centres when rounding.
	const double pixels_per_unit = extent / (6.0 * std::tan(std::numbers::pi / 6.0));
	std::size_t checked{};
	for (const auto tile_v : std::views::iota(0, 10)) {
		for (const auto tile_u : std::views::iota(0, 10)) {
			for (const auto quadrant : std::views::iota(0, 4)) {
				const double u = tile_u + (quadrant % 2 == 0 ? 0.25 : 0.75);
				const double v = tile_v + (quadrant / 2 == 0 ? 0.25 : 0.75);
				const int x = static_cast<int>(std::lround(extent / 2.0 + pixels_per_unit * (v / 5.0 - 1.0) - 0.5));
				const int y = static_cast<int>(std::lround(extent / 2.0 + pixels_per_unit * (u / 5.0 - 1.0) - 0.5));
				const auto *pixel = test::pixelAt(pixels->data(), extent, x, y);
				// Engine textures use bottom-up rows, so fractional v=0 samples the fixture's bottom half.
				const auto &expected = colors[(1 - quadrant / 2) * 2 + quadrant % 2];
				for (const auto channel : std::views::iota(0, 3)) {
					if (std::abs(static_cast<int>(pixel[channel]) - expected[channel]) > 48) {
						std::println("[RenderTexturedPlaneTests] tile={},{} quadrant={} pixel={},{} rgb={},{},{} expected={},{},{}",
							tile_u, tile_v, quadrant, x, y, pixel[0], pixel[1], pixel[2],
							expected[0], expected[1], expected[2]);
						return 6;
					}
				}
				++checked;
			}
		}
	}
	const bool compact = render.sceneMeshCount() == 1U && render.sceneInstanceCount() == 1U && render.gpuTextureCount() == 1U;
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderTexturedPlaneTests] repeats=10x10 samples={} one_plane={} validation_errors={} capture={}",
		checked, compact, render.forward().validationErrorCount(), capture.string());
	return compact && checked == 400U && render.forward().validationErrorCount() == 0U ? 0 : 7;
}
