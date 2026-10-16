/** @file @brief Checks the default background colour and captures an empty scene cleared to a set colour. */
import std;
import VVEngine.Simple;
import VVE.TestSupport;

/// @brief The default stays light blue; a set colour reaches every pixel of an empty frame.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	constexpr int extent = 64;
	auto engine = test::hiddenEngine("render-background-color-tests", PixelExtent{extent, extent});
	if (!engine.init()) { return 1; }
	auto &render = engine.renderSystem();
	const auto initial = render.backgroundColor().value;
	if (initial.x != 0.45F || initial.y != 0.70F || initial.z != 1.0F) { return 2; }
	render.setBackgroundColor(LinearColor{.value = Vec3{0.2F, 0.5F, 1.0F}});
	const auto stored = render.backgroundColor().value;
	const auto capture = directory / "background.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(capture)) { return 3; }
	const auto pixels = test::imagePixels(capture);
	if (!pixels || pixels->size() != extent * extent * 4U) { return 4; }
	// Linear 0.2 and 0.5 encode to about 124 and 188 in the sRGB swapchain.
	constexpr std::array expected{124, 188, 255, 255};
	std::size_t wrong{};
	for (std::size_t offset{}; offset < pixels->size(); offset += 4U) {
		for (const auto channel : std::views::iota(0U, 4U)) {
			if (std::abs(static_cast<int>((*pixels)[offset + channel]) - expected[channel]) > 3) { ++wrong; break; }
		}
	}
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderBackgroundColorTests] stored={},{},{} first_pixel={},{},{},{} wrong_pixels={} validation_errors={}",
		stored.x, stored.y, stored.z, (*pixels)[0], (*pixels)[1], (*pixels)[2], (*pixels)[3], wrong, render.forward().validationErrorCount());
	return stored.x == 0.2F && wrong == 0U && render.forward().validationErrorCount() == 0U ? 0 : 5;
}
