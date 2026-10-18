/**
 * @file
 * @brief Sprite checks: CPU creation, errors and sharing, then captures from three directions with equal coverage.
 *
 * The texture is opaque white on its left half and fully transparent on its right half, so a camera-facing sprite
 * covers half of its quad from every direction.
 */
import std;
import VVEngine.Simple;
import VVE.TestSupport;

namespace {

/// @brief Writes an uncompressed 32-bit TGA, top row first; the left half is opaque white, the right half transparent.
[[nodiscard]] bool writeHalfTransparentTga(const std::filesystem::path &path) {
	std::ofstream file{path, std::ios::binary};
	const std::array<unsigned char, 18> header{0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 8, 0, 32, 0x28};
	file.write(reinterpret_cast<const char *>(header.data()), header.size());
	for (const auto y : std::views::iota(0, 8)) {
		for (const auto x : std::views::iota(0, 8)) {
			const unsigned char alpha = x < 4 ? 255 : 0;
			for (const unsigned char channel : {static_cast<unsigned char>(255), static_cast<unsigned char>(255),
				static_cast<unsigned char>(255), alpha}) { file.put(static_cast<char>(channel)); }
			(void)y;
		}
	}
	return static_cast<bool>(file);
}

/// @brief Renders one frame from a camera position towards the origin and counts bright pixels.
[[nodiscard]] auto coverageFrom(vve::simple::Engine &engine, vve::Vec3 eye, const std::filesystem::path &path) -> std::optional<std::size_t> {
	using namespace vve;
	auto &render = engine.renderSystem();
	render.setCamera(Camera::lookAt(Position{.value = eye}, Position{}));
	if (!engine.renderFrame() || !render.captureFrameToPng(path)) { return std::nullopt; }
	const auto pixels = test::imagePixels(path);
	if (!pixels) { return std::nullopt; }
	std::size_t covered{};
	for (std::size_t offset{}; offset < pixels->size(); offset += 4U) { covered += (*pixels)[offset] > 128 ? 1U : 0U; }
	std::println("[RenderSpriteTests] eye={},{},{} covered={} forward_draws={}", eye.x, eye.y, eye.z, covered,
		render.forward().lastFrameDrawStats().forwardDraws);
	return render.forward().lastFrameDrawStats().forwardDraws == 1U ? std::optional{covered} : std::nullopt;
}

} // namespace

/// @brief Runs the CPU checks before GPU initialization, then compares coverage from three directions.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto texture = directory / "half_transparent.tga";
	if (!writeHalfTransparentTga(texture)) { return 1; }
	constexpr int extent = 128;
	auto engine = test::hiddenEngine("render-sprite-tests", PixelExtent{extent, extent});
	vve::test::configureRenderer(engine.renderSystem());
	auto &render = engine.renderSystem();
	// CPU: invalid sizes and missing textures fail; equal sizes share one quad, a new colour adds a material.
	const auto flat = render.addSprite(Vec2{0, 1}, texture);
	const auto missing = render.addSprite(Vec2{1, 1}, directory / "missing.png");
	const auto first = render.addSprite(Vec2{1, 1}, texture);
	const auto second = render.addSprite(Vec2{1, 1}, texture, LinearColor{.value = Vec3{1, 0, 0}},
		Transform{.translation = Position{.value = Vec3{2, 0, 0}}});
	if (flat || flat.error() != Error::invalid_argument || missing || missing.error() != Error::io_error || !first || !second) { return 2; }
	const bool shared = render.sceneMeshCount() == 1U && render.sceneVertexCount() == 4U && render.sceneIndexCount() == 6U &&
		render.sceneMaterialCount() == 2U && render.sceneTextureCount() == 1U;
	const auto larger = render.addSprite(Vec2{2, 1}, texture);
	if (!shared || !larger || render.sceneMeshCount() != 2U || !render.removeObject(*second) || !render.removeObject(*larger)) { return 3; }
	std::println("[RenderSpriteTests] cpu errors=ok shared_quad={} meshes_after_removal={}", shared, render.sceneMeshCount());

	// GPU: the sprite at the origin faces each camera, so its coverage does not depend on the direction.
	render.setBackgroundColor(LinearColor{.value = zeroVec3()});
	if (!engine.init()) { return 4; }
	const auto front = coverageFrom(engine, Vec3{0, 0, 3}, directory / "front.png");
	const auto side = coverageFrom(engine, Vec3{3, 0, 0}, directory / "side.png");
	const auto above = coverageFrom(engine, Vec3{0, 3, 0}, directory / "above.png");
	if (!front || !side || !above) { return 5; }
	// A 1x1 quad 3 units away under a 60 degree field of view spans about 37 pixels; half of it is opaque.
	const double expected = std::pow(extent / (6.0 * std::tan(std::numbers::pi / 6.0)), 2.0) / 2.0;
	const auto close = [](double left, double right, double tolerance) { return std::abs(left - right) <= tolerance * right; };
	const bool ok = close(static_cast<double>(*front), expected, 0.04) && close(static_cast<double>(*side), static_cast<double>(*front), 0.03) &&
		close(static_cast<double>(*above), static_cast<double>(*front), 0.03);
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderSpriteTests] expected_half_coverage={:.0f} validation_errors={}", expected, render.forward().validationErrorCount());
	return ok && render.forward().validationErrorCount() == 0U ? 0 : 6;
}
