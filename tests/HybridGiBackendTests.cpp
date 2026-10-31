/** @file @brief Facade selection, startup lifetime and real-device capability rejection for the reserved GI backend. */
import std;
import VVEngine;
import VVE.TestSupport;

namespace {

constexpr std::array backends{vve::RendererBackend::forward, vve::RendererBackend::deferred,
	vve::RendererBackend::hybrid_gi}; ///< Every independently selectable public backend.
static_assert(backends[0] != backends[1] && backends[0] != backends[2] && backends[1] != backends[2]);

/// @brief Checks the unchanged default, all startup transitions and isolation between two facade engines.
[[nodiscard]] bool startupContracts() {
	using namespace vve;
	Engine<> first, second;
	auto first_world = first.world(), second_world = second.world();
	auto &a = first_world.get<RenderSystem>(), &b = second_world.get<RenderSystem>();
	if (a.rendererBackend() != RendererBackend::forward || b.rendererBackend() != RendererBackend::forward ||
		!b.setRendererBackend(RendererBackend::hybrid_gi)) { return false; }
	// Exercise malformed values with each valid selection retained, including hybrid_gi.
	for (const auto backend : backends) {
		if (!a.setRendererBackend(backend) || a.rendererBackend() != backend || b.rendererBackend() != RendererBackend::hybrid_gi) { return false; }
		for (const auto value : {-1, 99}) {
			const auto invalid = a.setRendererBackend(static_cast<RendererBackend>(value));
			if (invalid || invalid.error() != Error::invalid_argument || a.rendererBackend() != backend) { return false; }
		}
	}
	if (!b.setRendererBackend(RendererBackend::deferred) || a.rendererBackend() != RendererBackend::hybrid_gi) { return false; }
	std::println("[HybridGiBackendTests] default=forward distinct=3 selectable=3 invalid=invalid_argument independent=true");
	return true;
}

/// @brief Renders through the facade, then rejects every different backend while allowing idempotent selection.
[[nodiscard]] bool renderedContracts(vve::RendererBackend backend) {
	using namespace vve;
	Engine<> engine{WindowSetups{WindowSetup{}.id("main").extent({64, 64}).visible(false)}};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	if (!engine.init() || !render.setRendererBackend(backend)) { return false; }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	if (!render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{}) || !engine.step() || render.renderedFrameCount() != 1U) { return false; }
	// A rendered GI frame locks the distinct choice even though its passes currently match deferred.
	for (const auto requested : backends) {
		const auto result = render.setRendererBackend(requested);
		if ((requested == backend ? !result : (result || result.error() != Error::already_initialized)) ||
			render.rendererBackend() != backend) { return false; }
	}
	const auto invalid = render.setRendererBackend(static_cast<RendererBackend>(99));
	if (invalid || invalid.error() != Error::invalid_argument || render.rendererBackend() != backend || !engine.step()) { return false; }
	std::println("[HybridGiBackendTests] backend={} late_change=already_initialized same_selection=ok invalid_preserves=true presented={}",
		std::to_underlying(backend), render.renderedFrameCount());
	return render.renderedFrameCount() == 2U;
}

/// @brief The Khronos Profiles layer removes deferred capability; both deferred choices must fail transactionally.
[[nodiscard]] bool unsupportedContracts() {
	using namespace vve;
	Engine<> engine{WindowSetups{WindowSetup{}.id("main").extent({64, 64}).visible(false)}};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	if (!engine.init()) { return false; }
	for (const auto backend : {RendererBackend::deferred, RendererBackend::hybrid_gi}) {
		const auto result = render.setRendererBackend(backend);
		if (result || result.error() != Error::platform_error || render.rendererBackend() != RendererBackend::forward) { return false; }
		std::println("[HybridGiBackendTests] profile=VP_VVE_forward_only rejected={} error={} preserved=forward",
			std::to_underlying(backend), errorName(result.error()));
	}
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	const auto object = render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{.value = Vec3{1, 0, 0}});
	if (!object || !render.setObjectUnlit(*object, true) || !engine.step() || !engine.step()) { return false; }
	const auto path = std::filesystem::path{VVE_TEST_TMP_DIR} / "unsupported-forward.png";
	std::filesystem::create_directories(path.parent_path());
	if (!render.captureFrameToPng(path)) { return false; }
	const auto pixels = test::imagePixels(path);
	if (!pixels || pixels->size() != 64U * 64U * 4U) { return false; }
	const auto *centre = test::pixelAt(pixels->data(), 64, 32, 32);
	std::println("[HybridGiBackendTests] preserved=forward presented={} capture={} centre={},{},{}",
		render.renderedFrameCount(), path.string(), centre[0], centre[1], centre[2]);
	return render.rendererBackend() == RendererBackend::forward && render.renderedFrameCount() == 2U &&
		centre[0] > 200 && centre[1] < 5 && centre[2] < 5;
}

} // namespace

/// @brief Runs real graphics contracts, or the separately registered Vulkan Profiles restriction scenario.
int main(int argc, char **argv) {
	if (argc == 2 && std::string_view{argv[1]} == "--expect-unsupported") { return unsupportedContracts() ? 0 : 2; }
	if (argc != 1) { return 3; }
	if (!startupContracts()) { return 1; }
	for (const auto backend : backends) { if (!renderedContracts(backend)) { return 1; } }
	return 0;
}
