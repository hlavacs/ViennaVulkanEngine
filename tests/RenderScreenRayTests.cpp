/**
 * @file
 * @brief screenRay checks: the window centre follows the camera, and the ray through a rendered cube's pixels hits it.
 *
 * The window is twice as wide as high, so a wrong aspect ratio or a flipped y axis makes the ray miss the cube.
 */
import std;
import VVEngine.Simple;
import VVE.TestSupport;

/// @brief Renders one off-centre cube, finds its pixel centroid and casts the screen ray through it.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	constexpr int width = 128;
	constexpr int height = 64;
	auto engine = test::hiddenEngine("render-screen-ray-tests", PixelExtent{width, height});
	if (!engine.init()) { return 1; }
	auto &render = engine.renderSystem();
	const auto *window = engine.windowSystem().findWindow("main");
	if (window == nullptr) { return 2; }
	const auto handle = window->info().handle;
	render.setBackgroundColor(LinearColor{.value = zeroVec3()});
	render.setCamera(handle, Camera::lookAt(Position{.value = Vec3{0, 0, 5}}, Position{})).value();
	const auto cube = render.addCuboid(Vec3{-0.2F, -0.2F, -0.2F}, Vec3{0.2F, 0.2F, 0.2F}, LinearColor{.value = oneVec3()},
		Transform{.translation = Position{.value = Vec3{2.5F, 0.8F, 0}}});
	if (!cube || !render.setObjectUnlit(*cube, true)) { return 3; }
	// The centre ray starts at the camera and looks along its forward direction.
	const auto centre = render.screenRay(handle, Vec2{0.5F, 0.5F});
	const auto unknown = render.screenRay(WindowHandle{}, Vec2{0.5F, 0.5F});
	if (!centre || std::abs(centre->second.value.z + 1) > 1.0e-5F || centre->first.value.z != 5.0F ||
		unknown || unknown.error() != Error::invalid_handle) { return 4; }
	const auto capture = directory / "cube.png";
	if (!engine.renderFrame() || !render.captureFrameToPng(capture)) { return 5; }
	const auto pixels = test::imagePixels(capture);
	if (!pixels || pixels->size() != width * height * 4U) { return 6; }
	double sum_x{}, sum_y{};
	std::size_t count{};
	for (const auto y : std::views::iota(0, height)) {
		for (const auto x : std::views::iota(0, width)) {
			if (test::pixelAt(pixels->data(), width, x, y)[0] > 128) { sum_x += x + 0.5; sum_y += y + 0.5; ++count; }
		}
	}
	if (count == 0U) { return 7; }
	const auto point = Vec2{static_cast<Scalar>(sum_x / count / width), static_cast<Scalar>(sum_y / count / height)};
	const auto ray = render.screenRay(handle, point);
	const auto hit = ray ? render.raycast(ray->first, ray->second, 100.0F) : std::nullopt;
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderScreenRayTests] cube_pixels={} centroid={:.3f},{:.3f} hit={} distance={} validation_errors={}", count, point.x, point.y,
		hit.has_value(), hit ? hit->distance : -1.0F, render.forward().validationErrorCount());
	return hit && hit->object == *cube && render.forward().validationErrorCount() == 0U ? 0 : 8;
}
