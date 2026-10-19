/**
 * @file
 * @brief Physical sky: invalid settings are rejected, then forward and deferred captures of a sea scene are checked.
 *
 * From 120 m above the sea, a clear sky is bluer overhead than at the horizon and shows a white sun; overcast clouds grey
 * the sky and the sea and hide the sun; inside the cloud layer the view is bright grey fog. Both backends must agree.
 */
#include <VVPPL.h>

import std;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {

/// @brief Mean RGB in 0..1 of a square around a pixel.
[[nodiscard]] std::array<double, 3> mean(const std::vector<unsigned char> &pixels, int width, int x, int y, int radius = 4) {
	std::array<double, 3> sum{};
	for (int dy{-radius}; dy <= radius; ++dy) {
		for (int dx{-radius}; dx <= radius; ++dx) {
			for (int c{}; c < 3; ++c) { sum[c] += vve::test::pixelAt(pixels.data(), width, x + dx, y + dy)[c] / 255.0; }
		}
	}
	for (auto &value : sum) { value /= (2 * radius + 1) * (2 * radius + 1); }
	return sum;
}

/// @brief Renders one view per setting and returns the centre colour of each, or nothing on failure.
[[nodiscard]] std::optional<std::vector<std::array<double, 3>>> captures(vve::RendererBackend backend) {
	using namespace vve;
	constexpr int width = 160, height = 90;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR} / (backend == RendererBackend::deferred ? "deferred" : "forward");
	std::filesystem::create_directories(directory);
	auto engine = test::hiddenEngine("render-sky", PixelExtent{width, height});
	auto &render = engine.renderSystem();
	if (!render.setRendererBackend(backend) || !engine.init()) { return std::nullopt; }
	render.setPostProcessSetup([](vvppl::PostProcessing &post) { post.addTonemap().exposure = 0.85F; });
	const Vec3 light{-0.45F, -0.74F, 0.28F};
	render.setDirectionalLight(Direction{.value = light}, LinearColor{.value = Vec3{1.0F}}, LightIntensity{1.5F}, LinearColor{.value = Vec3{0.02F}});
	if (!render.setMaterialLighting(MaterialLighting::physically_based) || !render.addOcean({.half_extent = {20000, 20000}, .wave_height = 0}) ||
		!render.setAtmosphere({.density = 0.00004F, .height_falloff = 0.002F})) { return std::nullopt; }
	const Vec3 sun = math::normalize(Vec3{-light.x, -light.y, -light.z});
	const Vec3 eye{0, 120, 0};
	struct View { std::string name; SkySettings sky; Vec3 eye, direction; int x, y; };
	const SkySettings overcast{.enabled = true, .cloud_coverage = 1, .cloud_base = 500, .cloud_thickness = 900};
	const std::array views{
		View{"clear_up", {.enabled = true}, eye, Vec3{0, 1, 0.3F}, width / 2, height / 2},
		View{"clear_horizon", {.enabled = true}, eye, Vec3{0, 0.1F, -1}, width / 2, height / 4},
		View{"clear_sun", {.enabled = true}, eye, sun, width / 2, height / 2},
		View{"clear_sea", {.enabled = true}, eye, Vec3{0, 0.1F, -1}, width / 2, height - 6},
		View{"cumulus", {.enabled = true, .cloud_coverage = 0.45F, .cloud_base = 900}, eye, Vec3{0, 0.25F, -1}, width / 2, height / 2},
		View{"overcast_up", overcast, eye, Vec3{0, 1, 0.3F}, width / 2, height / 2},
		View{"overcast_sun", overcast, eye, sun, width / 2, height / 2},
		View{"overcast_sea", overcast, eye, Vec3{0, 0.1F, -1}, width / 2, height - 6},
		View{"inside_cloud", overcast, Vec3{0, 900, 0}, Vec3{0, 0, -1}, width / 2, height / 2}};
	std::vector<std::array<double, 3>> colours{};
	for (const auto &view : views) {
		render.setCamera(Camera::lookAt(Position{.value = view.eye}, Position{.value = math::add(view.eye, view.direction)}, {}, FovY{1.0F},
			ClipPlanes{.near_plane = 0.5F, .far_plane = 12000.0F}));
		const auto path = directory / (view.name + ".png");
		if (!render.setSky(view.sky) || !engine.renderFrame() || !render.captureFrameToPng(path)) { return std::nullopt; }
		const auto pixels = test::imagePixels(path);
		if (!pixels) { return std::nullopt; }
		colours.push_back(mean(*pixels, width, view.x, view.y, view.name.ends_with("sun") ? 0 : 4));
		std::println("[RenderSkyTests] {} {}={:.2f},{:.2f},{:.2f}", directory.filename().string(), view.name, colours.back()[0], colours.back()[1], colours.back()[2]);
	}
	const auto errors = render.forward().validationErrorCount();
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	if (errors != 0U) { std::println("[RenderSkyTests] validation_errors={}", errors); return std::nullopt; }
	return colours;
}

} // namespace

/// @brief Exit code 0 when validation and both backends pass.
int main() {
	using namespace vve;
	auto engine = test::hiddenEngine("render-sky-validation", PixelExtent{32, 32});
	auto &render = engine.renderSystem();
	const std::array invalid{SkySettings{.intensity = -1}, SkySettings{.haze = std::numeric_limits<float>::quiet_NaN()},
		SkySettings{.cloud_coverage = 1.5F}, SkySettings{.cloud_thickness = 0}, SkySettings{.wind = Vec2{std::numeric_limits<float>::infinity(), 0}}};
	const bool rejected = std::ranges::none_of(invalid, [&](const SkySettings &sky) { return render.setSky(sky).has_value(); });
	render.clearScene(); // The sky is renderer state, not a scene object.
	if (!rejected || render.sky().enabled || !render.setSky({.enabled = true}) || (render.clearScene(), !render.sky().enabled)) {
		std::println("[RenderSkyTests] result=FAIL validation");
		return 1;
	}
	const auto forward = captures(RendererBackend::forward), deferred = captures(RendererBackend::deferred);
	if (!forward || !deferred) { std::println("[RenderSkyTests] result=FAIL capture"); return 1; }
	const auto &c = *forward;
	const auto luminance = [](const std::array<double, 3> &rgb) { return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]; };
	const auto grey = [](const std::array<double, 3> &rgb) { return std::max({rgb[0], rgb[1], rgb[2]}) - std::min({rgb[0], rgb[1], rgb[2]}) < 0.08; };
	const bool blue = c[0][2] > c[0][1] && c[0][1] > c[0][0] && c[0][2] - c[0][0] > c[1][2] - c[1][0]; // Deeper blue overhead.
	const bool sunDisk = std::ranges::all_of(c[2], [](double v) { return v > 0.95; }) && luminance(c[6]) < 0.9;
	const bool overcastGrey = grey(c[5]) && grey(c[8]) && luminance(c[8]) > 0.4; // Grey sky; bright fog inside the clouds.
	const bool shaded = luminance(c[7]) < 0.9 * luminance(c[3]); // Clouds shade the sun and grey the reflected sky.
	double difference{};
	for (std::size_t i{}; i < c.size(); ++i) { for (int k{}; k < 3; ++k) { difference = std::max(difference, std::abs(c[i][k] - (*deferred)[i][k])); } }
	std::println("[RenderSkyTests] blue={} sun={} overcast_grey={} shaded_sea={} backend_difference={:.3f}", blue, sunDisk, overcastGrey, shaded, difference);
	const bool ok = blue && sunDisk && overcastGrey && shaded && difference < 0.03;
	std::println("[RenderSkyTests] result={}", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
