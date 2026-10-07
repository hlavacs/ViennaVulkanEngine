/// @file
/// @brief Checks facade window ownership, frame snapshots and per-window camera assignments.
import std;
import VVEngine;

namespace {

/// @brief Records the window snapshot received by a user system.
struct WindowProbe {
	int *updates{}; ///< Number of delivered update hooks.
	vve::WindowFrameData *frame{}; ///< Last delivered window states.

	/// @brief Copies the window states so the caller can check identity and drawable size.
	template <typename TWorld>
	std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &, const vve::WindowFrameData &windows) {
		++*updates;
		*frame = windows;
		return {};
	}
};

} // namespace

/// @brief Assigns cameras through RenderSystem while WindowSystem and frame snapshots retain window identity.
int main() {
	int updates{};
	vve::WindowFrameData frame{};
	auto engine = vve::EngineBuilder<WindowProbe>{}
		.applicationName("window-ownership-tests")
		.maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
		.windows(vve::WindowSetups{
			vve::WindowSetup{}.id("main").title("window-ownership-main").extent({64, 64}).visible(false),
			vve::WindowSetup{}.id("tools").title("window-ownership-tools").extent({64, 64}).visible(false)})
		.userSystems(vve::makeUserSystems(WindowProbe{.updates = &updates, .frame = &frame})).build();
	if (!engine.init()) { return 1; }
	auto world = engine.world();
	auto &windows = world.get<vve::WindowSystem>();
	auto &render = world.get<vve::RenderSystem>();
	const auto main = windows.findWindow("main");
	const auto tools = windows.findWindow("tools");
	if (!main || !tools || main->handle() == tools->handle()) { return 2; }
	// Cameras may be assigned and cleared before lazy Vulkan initialization.
	render.setCamera(vve::Camera{});
	if (!render.setCamera(main->handle(), vve::Camera{}) || !render.clearCamera(main->handle()) ||
		!render.setCamera(tools->handle(), vve::Camera{})) { return 3; }
	const auto invalid = render.setCamera(vve::WindowHandle{}, vve::Camera{});
	if (invalid || invalid.error() != vve::Error::invalid_handle) { return 4; }
	const auto status = engine.step();
	if (!status || *status != vve::FrameStatus::stopped || updates != 1 || frame.windows.size() != 2U) { return 5; }
	// The snapshots still describe both owned windows without ECS camera bindings.
	for (const auto &window : frame.windows) {
		const auto original = windows.findWindow(window.handle);
		if (!original || window.id != original->id() || window.title != original->title() ||
			window.extent.width != 64U || window.extent.height != 64U) { return 6; }
	}
	std::println("[WindowOwnershipTests] presented_windows={} expected=2 snapshot_windows={}",
		render.lastRenderedWindowCount(), frame.windows.size());
	if (render.lastRenderedWindowCount() != 2U || render.renderedFrameCount() != 1U) { return 7; }
	if (!render.clearCamera(tools->handle()) || !render.clearCamera(main->handle())) { return 8; }
	const auto invalid_clear = render.clearCamera(vve::WindowHandle{});
	return !invalid_clear && invalid_clear.error() == vve::Error::invalid_handle ? 0 : 9;
}
