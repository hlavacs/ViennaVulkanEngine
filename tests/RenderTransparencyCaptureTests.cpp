/** @file @brief Captures a half-transparent blue plane in front of a red plane and checks the blended colour. */
import std;
import VVEngine.Simple;
import VVE.TestSupport;

/// @brief Renders both planes unlit from above; the centre must mix red and blue, the edge must stay red.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	constexpr int extent = 128;
	auto engine = test::hiddenEngine("render-transparency-capture-tests", PixelExtent{extent, extent});
	if (!engine.init()) { return 1; }
	auto &render = engine.renderSystem();
	// The transparent plane is added first, so the renderer must sort it behind the solid one.
	const auto glass = render.addPlane(Vec2{0.5F, 0.5F}, LinearColor{.value = Vec3{0, 0, 1}},
		Transform{.translation = Position{.value = Vec3{0, 1, 0}}});
	const auto floor = render.addPlane(Vec2{2, 2}, LinearColor{.value = Vec3{1, 0, 0}});
	if (!glass || !floor || !render.setObjectUnlit(*glass, true) || !render.setObjectUnlit(*floor, true) ||
		!render.setObjectOpacity(*glass, 0.5F)) { return 2; }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 4, 0}}, Position{}));
	const auto capture = directory / "glass-over-red.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(capture)) { return 3; }
	const auto pixels = test::imagePixels(capture);
	if (!pixels || pixels->size() != extent * extent * 4U) { return 4; }
	const auto *centre = test::pixelAt(pixels->data(), extent, extent / 2, extent / 2);
	const auto *edge = test::pixelAt(pixels->data(), extent, extent / 2, extent / 2 + 30);
	// Linear 0.5 encodes to about 188 in the sRGB swapchain; alpha stays opaque.
	const bool mixed = centre[0] > 150 && centre[0] < 225 && centre[1] < 30 && centre[2] > 150 && centre[2] < 225 && centre[3] == 255;
	const bool red = edge[0] > 240 && edge[1] < 30 && edge[2] < 30;
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderTransparencyCaptureTests] centre={},{},{},{} edge={},{},{} validation_errors={} capture={}",
		centre[0], centre[1], centre[2], centre[3], edge[0], edge[1], edge[2], render.forward().validationErrorCount(), capture.string());
	return mixed && red && render.forward().validationErrorCount() == 0U ? 0 : 5;
}
