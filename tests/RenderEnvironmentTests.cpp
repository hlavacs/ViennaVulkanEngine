/** @file @brief Deterministic public atmosphere/water contracts, fog depth and height, and explicit wave animation. */
import std;
import VVEngine;
import VVEngine.Simple;
import VVE.TestSupport;

namespace {

/// @brief Exercises validation before GPU initialization; rejected values preserve state and resource counts.
[[nodiscard]] bool cpuContracts() {
	using namespace vve;
	Engine<> engine;
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	const Scalar nan = std::numeric_limits<Scalar>::quiet_NaN(), inf = std::numeric_limits<Scalar>::infinity();
	const auto haze = AtmosphereSettings{.color = LinearColor{.value = Vec3{0, 0, 1}}, .density = 0.12F, .height_falloff = 0.2F};
	if (render.atmosphere().distance_mist_start != zero() || render.atmosphere().distance_mist_end != zero()) { return false; }
	// Distance mist is independent of weather extinction: it may start at the camera or after a clear foreground.
	std::size_t mist_rejected{};
	for (const auto range : std::array<std::pair<Scalar, Scalar>, 3>{{{0, 1}, {256, 716.8F}, {0, 0}}}) {
		auto mist = haze;
		mist.distance_mist_start = range.first;
		mist.distance_mist_end = range.second;
		if (!render.setAtmosphere(mist) || render.atmosphere().distance_mist_start != range.first ||
			render.atmosphere().distance_mist_end != range.second || render.atmosphere().density != haze.density ||
			render.atmosphere().height_falloff != haze.height_falloff) { return false; }
	}
	auto mist = haze;
	mist.distance_mist_start = 256;
	mist.distance_mist_end = 716.8F;
	if (!render.setAtmosphere(mist)) { return false; }
	// A rejected range must leave every atmosphere field and the empty scene unchanged, not just the offending field.
	const auto reject_mist = [&](AtmosphereSettings bad) {
		const auto result = render.setAtmosphere(bad);
		const auto retained = render.atmosphere();
		++mist_rejected;
		return !result && result.error() == Error::invalid_argument && retained.distance_mist_start == mist.distance_mist_start &&
			retained.distance_mist_end == mist.distance_mist_end && retained.density == mist.density &&
			retained.height_falloff == mist.height_falloff && retained.base_height == mist.base_height &&
			retained.color.value.x == mist.color.value.x && retained.color.value.y == mist.color.value.y &&
			retained.color.value.z == mist.color.value.z && render.sceneMeshCount() == 0U && render.sceneInstanceCount() == 0U;
	};
	for (const auto member : {&AtmosphereSettings::distance_mist_start, &AtmosphereSettings::distance_mist_end}) {
		for (const auto value : std::array<Scalar, 4>{-one(), nan, inf, -inf}) {
			auto bad = mist;
			bad.*member = value;
			if (!reject_mist(bad)) { return false; }
		}
	}
	for (const auto range : std::array<std::pair<Scalar, Scalar>, 3>{{{256, 0}, {256, 255}, {256, 256}}}) {
		auto bad = mist;
		bad.distance_mist_start = range.first;
		bad.distance_mist_end = range.second;
		if (!reject_mist(bad)) { return false; }
	}
	// A zero-density atmosphere still accepts independent distance mist; resetting restores the legacy disabled default.
	if (!render.setAtmosphere(AtmosphereSettings{.distance_mist_start = 256, .distance_mist_end = 716.8F}) ||
		render.atmosphere().density != zero() || !render.setAtmosphere(AtmosphereSettings{})) { return false; }
	std::println("[RenderEnvironmentTests] distance_mist_rejected={} foreground_clear_to={} horizon_complete_at={}",
		mist_rejected, mist.distance_mist_start, mist.distance_mist_end);
	if (render.atmosphere().density != zero() || !render.setAtmosphere(haze) || !render.setAnimationTime(AnimationTime{})) { return false; }
	std::size_t rejected{};
	// Extinction, falloff and colour require finite nonnegative values; base height permits negative values.
	for (const auto value : std::array<Scalar, 3>{-one(), nan, inf}) {
		for (const auto member : {&AtmosphereSettings::density, &AtmosphereSettings::height_falloff}) {
			auto bad = haze;
			bad.*member = value;
			const auto result = render.setAtmosphere(bad);
			if (result || result.error() != Error::invalid_argument) { return false; }
			++rejected;
		}
		auto bad = haze;
		bad.color.value.y = value;
		if (render.setAtmosphere(bad)) { return false; }
	}
	for (const auto value : {nan, inf}) {
		auto bad = haze;
		bad.base_height = value;
		if (render.setAtmosphere(bad)) { return false; }
	}
	for (const double value : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max()}) {
		const auto result = render.setAnimationTime(AnimationTime{value});
		if (result || result.error() != Error::invalid_argument) { return false; }
	}
	if (render.atmosphere().density != haze.density || render.atmosphere().height_falloff != haze.height_falloff ||
		render.atmosphere().color.value.y != zero() || render.atmosphere().base_height != zero()) { return false; }
	for (const auto value : std::array<Scalar, 3>{zero(), -one(), nan}) {
		auto bad = OceanDescriptor{};
		bad.wavelength = value;
		if (render.addOcean(bad)) { return false; }
		bad = OceanDescriptor{};
		bad.half_extent.x = value;
		if (render.addOcean(bad)) { return false; }
	}
	for (const auto value : std::array<Scalar, 3>{-one(), nan, inf}) {
		auto bad = OceanDescriptor{};
		bad.wave_height = value;
		if (render.addOcean(bad)) { return false; }
	}
	auto bad = OceanDescriptor{};
	bad.speed = nan;
	if (render.addOcean(bad)) { return false; }
	bad = OceanDescriptor{};
	bad.position.value.z = inf;
	if (render.addOcean(bad) || render.sceneInstanceCount() != 0U || render.sceneMeshCount() != 0U) { return false; }
	const auto water = render.addOcean(OceanDescriptor{.half_extent = {4, 6}, .wave_height = 0.75F, .speed = -1,
		.position = Position{.value = Vec3{2, 3, -4}}});
	const auto box = water ? render.objectBounds(*water) : std::unexpected(Error::missing_object);
	if (!box || !box->valid || box->minimum.value.x != -2 || box->maximum.value.z != 2 ||
		box->minimum.value.y > 2.25F || box->maximum.value.y < 3.75F) { return false; }
	if (!render.setObjectVisible(*water, false) || render.objectVisible(*water).value_or(true) ||
		!render.setObjectColor(*water, LinearColor{}) || !render.setObjectOpacity(*water, 0.5F) ||
		render.objectOpacity(*water).value_or(0) != 0.5F || !render.removeObject(*water)) { return false; }
	(void)render.purgeUnusedAssets();
	std::println("[RenderEnvironmentTests] cpu_rejected={} water_bounds_y={}..{} remaining_meshes={}",
		rejected, box->minimum.value.y, box->maximum.value.y, render.sceneMeshCount());
	return !render.objectBounds(*water) && render.sceneInstanceCount() == 0U && render.sceneMeshCount() == 0U && render.sceneMaterialCount() == 0U;
}

/// @brief Captures a complete frame, retaining PNGs so a failing pixel comparison can be inspected.
[[nodiscard]] auto capture(vve::simple::Engine &engine, const std::filesystem::path &path)
	-> std::optional<std::vector<unsigned char>> {
	if (!engine.renderFrame() || !engine.renderSystem().captureFrameToPng(path)) { return {}; }
	return vve::test::imagePixels(path);
}

} // namespace

/// @brief Compares fog against known unlit colours and freezes the explicit water clock for repeatable captures.
int main() {
	using namespace vve;
	if (!cpuContracts()) { return 1; }
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	constexpr int extent = 96;
	auto engine = test::hiddenEngine("render-environment-tests", PixelExtent{extent, extent});
	vve::test::configureRenderer(engine.renderSystem());
	if (!engine.init()) { return 2; }
	auto &render = engine.renderSystem();
	const auto sky = test::writeFixture(directory, "green.ppm", std::string{"P6\n1 1\n255\n"} + std::string{"\0\xff\0", 3});
	if (!render.setSkybox(sky)) { return 3; }
	const auto mesh = MeshDescriptor{.positions = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}}, .indices = {0, 1, 2, 0, 2, 3}};
	const auto near = render.addMesh(mesh), far = render.addMesh(mesh);
	if (!near || !far || !render.setObjectUnlit(*near, true) || !render.setObjectUnlit(*far, true)) { return 4; }
	const auto place = [&](Scalar altitude) {
		render.setCamera(Camera::lookAt(Position{.value = Vec3{0, altitude, 0}}, Position{.value = Vec3{0, altitude, -1}}));
		return render.setObjectTransform(*near, Transform{.translation = Position{.value = Vec3{-1.2F, altitude, -4}}, .scale = Scale{.value = Vec3{0.65F}}}) &&
			render.setObjectTransform(*far, Transform{.translation = Position{.value = Vec3{3.6F, altitude, -12}}, .scale = Scale{.value = Vec3{1.95F}}});
	};
	if (!place(0)) { return 5; }
	const auto baseline = capture(engine, directory / "clear.png");
	if (render.forward().frameUniforms().fogHeightTime.z != 0.0F) { return 20; }
	const auto haze = AtmosphereSettings{.color = LinearColor{.value = Vec3{0, 0, 1}}, .density = 0.12F};
	if (!render.setAtmosphere(haze)) { return 6; }
	const auto fog = capture(engine, directory / "fog.png");
	if (!baseline || !fog || baseline->size() != extent * extent * 4U || fog->size() != baseline->size()) { return 7; }
	const auto *near_pixel = test::pixelAt(fog->data(), extent, 24, 48), *far_pixel = test::pixelAt(fog->data(), extent, 72, 48);
	const auto *sky_pixel = test::pixelAt(fog->data(), extent, 4, 4);
	std::println("[RenderEnvironmentTests] near_red={} far_red={} sky={},{},{}", near_pixel[0], far_pixel[0], sky_pixel[0], sky_pixel[1], sky_pixel[2]);
	if (near_pixel[0] < far_pixel[0] + 30 || near_pixel[0] > 245 || far_pixel[2] < 250 || sky_pixel[0] > 3 || sky_pixel[1] < 250 || sky_pixel[2] > 3) { return 8; }
	auto height_haze = haze;
	height_haze.height_falloff = 0.5F;
	if (!place(10) || !render.setAtmosphere(height_haze)) { return 9; }
	const auto elevated = capture(engine, directory / "elevated.png");
	if (!elevated || test::pixelAt(elevated->data(), extent, 72, 48)[0] < far_pixel[0] + 40) { return 10; }
	if (!place(0) || !render.setAtmosphere(AtmosphereSettings{})) { return 11; }
	const auto restored = capture(engine, directory / "restored.png");
	if (!restored || *restored != *baseline) { return 12; }

	// Water motion is driven only by the supplied clock; returning to zero must reproduce every pixel.
	render.clearScene();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 3, 6}}, Position{}));
	render.setDirectionalLight(Direction{.value = Vec3{0.4F, -1, 0.3F}}, LinearColor{}, LightIntensity{2}, LinearColor{.value = Vec3{0.1F}});
	const auto water = render.addOcean(OceanDescriptor{.half_extent = {4, 4}, .wave_height = 0.5F, .wavelength = 3, .speed = 1.7F});
	if (!water || !render.setAnimationTime(AnimationTime{})) { return 13; }
	const auto frozen = capture(engine, directory / "water0.png"), repeated = capture(engine, directory / "water0-repeat.png");
	if (!render.setAnimationTime(AnimationTime{1})) { return 14; }
	if (render.setAnimationTime(AnimationTime{-1})) { return 21; }
	const auto moving = capture(engine, directory / "water1.png");
	if (render.forward().frameUniforms().fogHeightTime.z != 1.0F) { return 22; }
	if (!render.setAnimationTime(AnimationTime{})) { return 15; }
	const auto reset = capture(engine, directory / "water0-reset.png");
	if (!frozen || !repeated || !moving || !reset || *frozen != *repeated || *frozen != *reset || frozen->size() != moving->size()) { return 16; }
	std::size_t changed{};
	for (const auto offset : std::views::iota(std::size_t{}, frozen->size())) { if (std::abs((*frozen)[offset] - (*moving)[offset]) > 2) { ++changed; } }
	if (changed < 30U || !render.removeObject(*water)) { return 17; }
	(void)render.purgeUnusedAssets();
	if (!engine.renderFrame() || render.sceneInstanceCount() != 0U || render.sceneMeshCount() != 0U) { return 18; }
	std::println("[RenderEnvironmentTests] validation_active={}", render.forward().validationActive());
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderEnvironmentTests] changed_wave_channels={} remaining_meshes={} validation_errors={}",
		changed, render.sceneMeshCount(), render.forward().validationErrorCount());
	return render.forward().validationErrorCount() == 0U ? 0 : 19;
}
