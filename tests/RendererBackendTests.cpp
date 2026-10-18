/** @file @brief Public runtime renderer selection, instance independence and startup lifetime contracts. */
import std;
import VVEngine;

namespace {

/// @brief Rejects malformed selections and keeps startup choices independent without opening a window.
[[nodiscard]] bool startupContracts() {
	using namespace vve;
	Engine<> first, second;
	auto first_world = first.world(), second_world = second.world();
	auto &a = first_world.get<RenderSystem>();
	auto &b = second_world.get<RenderSystem>();
	if (a.rendererBackend() != RendererBackend::forward || b.rendererBackend() != RendererBackend::forward) { return false; }
	const auto invalid = a.setRendererBackend(static_cast<RendererBackend>(99));
	if (invalid || invalid.error() != Error::invalid_argument || a.rendererBackend() != RendererBackend::forward) { return false; }
	if (!a.setRendererBackend(RendererBackend::deferred) || a.rendererBackend() != RendererBackend::deferred ||
		b.rendererBackend() != RendererBackend::forward) { return false; }
	if (!b.setRendererBackend(RendererBackend::deferred) || !a.setRendererBackend(RendererBackend::forward) ||
		a.rendererBackend() != RendererBackend::forward || b.rendererBackend() != RendererBackend::deferred) { return false; }
	std::println("[RendererBackendTests] default=forward invalid=invalid_argument independent=true startup_switching=true");
	return true;
}

/// @brief Uses only facade calls to select either compiled backend and lock it after its first GPU frame.
[[nodiscard]] bool renderedContracts(vve::RendererBackend backend) {
	using namespace vve;
	Engine<> engine{WindowSetups{WindowSetup{}.id("main").extent({64, 64}).visible(false)}};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	if (!engine.init() || !render.setRendererBackend(backend) || render.rendererBackend() != backend) { return false; }
	const auto object = render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{});
	if (!object || !render.setObjectUnlit(*object, true) || !engine.step() || render.renderedFrameCount() != 1U) { return false; }
	const auto other = backend == RendererBackend::forward ? RendererBackend::deferred : RendererBackend::forward;
	const auto changed = render.setRendererBackend(other);
	const auto invalid = render.setRendererBackend(static_cast<RendererBackend>(99));
	if (changed || changed.error() != Error::already_initialized || !render.setRendererBackend(backend) ||
		invalid || invalid.error() != Error::invalid_argument || render.rendererBackend() != backend || !engine.step()) { return false; }
	std::println("[RendererBackendTests] renderer={} presented={} late_change={} same_selection=ok invalid_preserves=true",
		backend == RendererBackend::deferred ? "deferred" : "forward", render.renderedFrameCount(), errorName(changed.error()));
	return render.renderedFrameCount() == 2U;
}

/// @brief A real Vulkan Profiles layer limits colour attachments; rejected deferred selection must preserve working forward rendering.
[[nodiscard]] bool unsupportedContract() {
	using namespace vve;
	Engine<> engine{WindowSetups{WindowSetup{}.id("main").extent({64, 64}).visible(false)}};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	if (!engine.init()) { return false; }
	const auto rejected = render.setRendererBackend(RendererBackend::deferred);
	if (rejected || rejected.error() != Error::platform_error || render.rendererBackend() != RendererBackend::forward) { return false; }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 3}}, Position{}));
	const auto object = render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{.value = Vec3{1, 0, 0}});
	if (!object || !render.setObjectUnlit(*object, true) || !engine.step() || !engine.step() || render.renderedFrameCount() != 2U) { return false; }
	const auto capture = std::filesystem::path{VVE_TEST_TMP_DIR} / "unsupported-forward.png";
	std::filesystem::create_directories(capture.parent_path());
	if (!render.captureFrameToPng(capture)) { return false; }
	std::println("[RendererBackendTests] profile=VP_VVE_forward_only unavailable={} active=forward presented={} capture={}",
		errorName(rejected.error()), render.renderedFrameCount(), capture.string());
	return render.rendererBackend() == RendererBackend::forward && std::filesystem::file_size(capture) > 0U;
}

} // namespace

/// @brief Builds once and exercises both renderer selections through the official engine facade.
int main(int argc, char **argv) {
	if (argc == 2 && std::string_view{argv[1]} == "--expect-unsupported") { return unsupportedContract() ? 0 : 2; }
	if (argc != 1) { return 3; }
	return startupContracts() && renderedContracts(vve::RendererBackend::forward) &&
		renderedContracts(vve::RendererBackend::deferred) ? 0 : 1;
}
