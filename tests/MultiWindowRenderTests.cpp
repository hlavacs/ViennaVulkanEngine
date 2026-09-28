/**
 * @file
 * @brief Multi-window presentation, capture, independent resize, GUI ownership and target teardown.
 * Helpers inspect deterministic PNG pixels; main also checks renderer selection and chain lifetimes.
 */
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>

import std;
import VVE.TestSupport;
import VEEngine.Simple;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

namespace {

/// @brief Captures one target and checks its extent, central cuboid and GUI-only corner marker.
[[nodiscard]] bool checkCapture(vve::simple::RenderSystem &render, vve::WindowHandle window,
	const std::filesystem::path &path, vve::PixelExtent extent, bool gui, bool red = true, bool background = false) {
	std::filesystem::remove(path);
	if (!render.captureFrameToPng(window, path) || !std::filesystem::exists(path)) { return false; }
	int width{}, height{}, channels{};
	auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
		stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
	if (!pixels || width != static_cast<int>(extent.width) || height != static_cast<int>(extent.height)) { return false; }
	const auto *centre = vve::test::pixelAt(pixels.get(), width, width / 2, height / 2);
	const auto *corner = vve::test::pixelAt(pixels.get(), width, 4, 4);
	std::println("[MultiWindowRenderTests] file={} extent={}x{} centre={},{},{} corner={},{},{}",
		path.filename().string(), width, height, centre[0], centre[1], centre[2], corner[0], corner[1], corner[2]);
	// The unlit red cuboid covers the centre; a retained per-window effect can suppress only its red channel.
	if (background) {
		const auto *sky = vve::test::pixelAt(pixels.get(), width, width - 4, 4);
		for (const auto channel : {0, 1, 2}) {
			if (std::abs(static_cast<int>(centre[channel]) - sky[channel]) > 2) { return false; }
		}
		if (centre[2] < centre[0] + 30) { return false; }
	} else if (red ? (centre[0] < centre[1] + 80 || centre[0] < centre[2] + 80) : centre[0] > 2) { return false; }
	if (gui) {
		const int grey = render.forward().device.swapchainMutableFormat ? 128 : 188;
		for (const auto channel : {0, 1, 2}) {
			if (std::abs(static_cast<int>(corner[channel]) - grey) > 2) { return false; }
		}
	} else if (corner[2] < corner[0] + 30) { return false; } // Sky blue, with no GUI marker.
	return true;
}

/// @brief Reads the last uploaded camera and cascade matrices, after the GPU has completed the frame.
[[nodiscard]] vve::simple::FrameUniforms uniforms(const vve::simple::WindowTarget &target) {
	vve::simple::FrameUniforms result{};
	const auto count = target.uniformBuffers.buffers.size();
	std::memcpy(&result, target.uniformBuffers.buffers[(target.currentFrame + count - 1U) % count].mapped, sizeof(result));
	return result;
}

/// @brief Reports the largest element difference so view, projection and cascade failures are inspectable.
[[nodiscard]] float matrixDifference(const vve::Mat4 &a, const vve::Mat4 &b) {
	float result{};
	for (const auto column : {0, 1, 2, 3}) {
		for (const auto row : {0, 1, 2, 3}) { result = std::max(result, std::abs(a[column][row] - b[column][row])); }
	}
	return result;
}

/// @brief Proves independent cameras, live default fallback, and per-window projection and directional cascades.
[[nodiscard]] bool checkWindowCameras(vve::simple::Engine &engine, const std::filesystem::path &path) {
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	const auto &a = renderer.targets.front();
	const auto &b = renderer.targets.back();
	const vve::PixelExtent extent{b.swapchain.extent.width, b.swapchain.extent.height};
	const auto toward = vve::Camera::lookAt(vve::Position{{0.0F, 6.0F, 9.0F}}, vve::Position{{0.0F, 1.0F, 0.0F}});
	auto away = toward;
	away.forward.value = {0.0F, 0.0F, 1.0F};
	away.fov_y.radians = 0.8F;
	render.setCamera(toward);
	if (!render.setCamera(b.handle, away) || !engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return false; }
	render.waitIdle();
	const auto ua = uniforms(a);
	const auto ub = uniforms(b);
	const auto expected_view = vve::math::lookAt(away.position.value,
		vve::math::add(away.position.value, away.forward.value), vve::Vec3{0.0F, 1.0F, 0.0F});
	const auto expected_projection = vve::math::perspectiveVulkan(away.fov_y.radians,
		static_cast<float>(extent.width) / extent.height, 0.1F, 100.0F);
	renderer.prepareShadowFrame(expected_view, away.fov_y.radians,
		static_cast<float>(extent.width) / extent.height, 0.1F, 100.0F);
	const auto &expected_shadows = renderer.frameUniforms(); ///< Prepared matrices share the uniform upload source.
	const auto cascade = vve::simple::kShadowMatrixDirBase;
	std::println("[MultiWindowRenderTests] camera_view_error={} projection_error={} cascade_error={} distinct_cascades={}",
		matrixDifference(ub.view, expected_view), matrixDifference(ub.projection, expected_projection),
		matrixDifference(ub.shadowViewProjs[cascade], expected_shadows.shadowViewProjs[cascade]),
		matrixDifference(ua.shadowViewProjs[cascade], ub.shadowViewProjs[cascade]));
	if (matrixDifference(ub.view, expected_view) > 1e-5F || matrixDifference(ub.projection, expected_projection) > 1e-5F ||
		matrixDifference(ub.shadowViewProjs[cascade], expected_shadows.shadowViewProjs[cascade]) > 1e-5F ||
		matrixDifference(ua.shadowViewProjs[cascade], ub.shadowViewProjs[cascade]) < 1e-3F) { return false; }
	if (!checkCapture(render, a.handle, path / "a-default.png", {64, 64}, true) ||
		!checkCapture(render, b.handle, path / "b-away.png", extent, false, true, true)) { return false; }
	// Changing the default changes only the unassigned window; clearing b then adopts that new default.
	if (!render.setCamera(b.handle, toward)) { return false; }
	render.setCamera(away);
	if (!checkCapture(render, a.handle, path / "a-new-default.png", {64, 64}, true, true, true) ||
		!checkCapture(render, b.handle, path / "b-explicit.png", extent, false) || !render.clearCamera(b.handle) ||
		!checkCapture(render, b.handle, path / "b-cleared.png", extent, false, true, true)) { return false; }
	render.setCamera(toward);
	return checkCapture(render, a.handle, path / "a-restored.png", {64, 64}, true) &&
		checkCapture(render, b.handle, path / "b-restored.png", extent, false) && !a.camera && !b.camera;
}

/// @brief Rejects unknown renderer ids at init and lets an entirely opted-out engine step without a GPU.
[[nodiscard]] bool checkRendererSelection() {
	auto invalid = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "invalid", .extent = {64, 64}, .renderer_id = {"foo"}, .visible = false}}}};
	const auto result = invalid.init();
	if (result || result.error() != vve::Error::invalid_argument || invalid.windowSystem().windowCount() != 0U) { return false; }
	auto disabled = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "disabled", .extent = {64, 64}, .renderer_id = {"none"}, .visible = false}}}};
	if (!disabled.init() || !disabled.renderFrame()) { return false; }
	const auto &render = disabled.renderSystem();
	std::println("[MultiWindowRenderTests] invalid=invalid_argument all_none_presented={}", render.lastRenderedWindowCount());
	return render.lastRenderedWindowCount() == 0U && render.renderedFrameCount() == 0U &&
		render.forward().targets.empty() && !render.forward().initialized();
}

} // namespace

/// @brief Renders two hidden windows with one shared device, while a third window opts out.
int main() {
	if (!checkRendererSelection()) { return 1; }
	auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "a", .extent = {64, 64}, .visible = false},
		vve::WindowDesc{.id = "b", .extent = {96, 64}, .renderer_id = {"forward"}, .visible = false},
		vve::WindowDesc{.id = "unused", .extent = {64, 64}, .renderer_id = {"none"}, .visible = false}}}};
	if (!engine.init()) { return 2; }
	auto &windows = engine.windowSystem();
	auto *a = windows.findWindow("a");
	auto *b = windows.findWindow("b");
	auto *unused = windows.findWindow("unused");
	if (!a || !b || !unused) { return 3; }
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	const auto initial_camera = vve::Camera::lookAt(vve::Position{{0.0F, 6.0F, 9.0F}}, vve::Position{{0.0F, 1.0F, 0.0F}});
	// Exercise pending overrides and clearing before the first frame creates GPU targets.
	render.setCamera(initial_camera);
	if (!render.setCamera(a->info().handle, vve::Camera{}) || !render.clearCamera(a->info().handle) ||
		!render.setCamera(b->info().handle, initial_camera)) { return 26; }
	for (const auto handle : {vve::WindowHandle{}, unused->info().handle}) {
		const auto set = render.setCamera(handle, initial_camera);
		const auto clear = render.clearCamera(handle);
		if (set || set.error() != vve::Error::invalid_handle || clear || clear.error() != vve::Error::invalid_handle) { return 27; }
	}
	const auto cuboid = render.addCuboid(vve::Vec3{-1.0F, -1.0F, -1.0F}, vve::Vec3{1.0F, 2.0F, 1.0F},
		vve::LinearColor{.value = {0.8F, 0.08F, 0.02F}});
	if (!cuboid || !render.setObjectUnlit(*cuboid, true)) { return 4; }
	// A directional light exercises the shared shadow arrays with a different aspect ratio per window.
	render.setDirectionalLight(vve::Direction{.value = {0.25F, -1.0F, 0.35F}},
		vve::LinearColor{}, vve::LightIntensity{}, vve::LinearColor{});
	std::vector<vvppl::PostProcessing *> chains{};
	std::vector<vvppl::ColorGradeSettings *> settings{};
	render.setPostProcessSetup([&](vvppl::PostProcessing &chain) {
		chains.push_back(&chain);
		settings.push_back(&chain.addColorGrade());
	});
	std::size_t gui_calls{};
	engine.gui().draw([&] {
		++gui_calls;
		ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2{0, 0}, ImVec2{12, 12}, IM_COL32(128, 128, 128, 255));
	});
	// A logical frame counts once even though two independent swapchains present.
	for (const auto frame : {1U, 2U}) {
		if (!engine.renderFrame()) { return 5; }
		std::println("[MultiWindowRenderTests] frame={} presented={} expected=2", frame, render.lastRenderedWindowCount());
		if (render.lastRenderedWindowCount() != 2U || render.renderedFrameCount() != frame) { return 6; }
		// The second target records a fresh command buffer and must bind its own shadow pipeline/set too.
		const auto stats = renderer.lastFrameDrawStats();
		std::println("[MultiWindowRenderTests] lastFrameDrawStats={},{},{},{} shadow_layers={}", stats.forwardDraws,
			stats.shadowDraws, stats.vertexBufferBinds, stats.pipelineBinds, renderer.lastShadowLayerPassCount());
		if (stats.pipelineBinds != 2U || stats.shadowDraws == 0U ||
			renderer.lastShadowLayerPassCount() != vve::simple::kNumShadowCascades) { return 32; }
		// Complete both submissions before checking validation of each window's shadow bindings.
		render.waitIdle();
		std::println("[MultiWindowRenderTests] frame={} validation_active={} validation_errors={}",
			frame, renderer.validationActive(), renderer.validationErrorCount());
		if (renderer.validationErrorCount() != 0U) { return 33; }
	}
	if (renderer.targets.size() != 2U || chains.size() != 2U || chains[0] == chains[1] ||
		settings[0] == settings[1] || gui_calls != 2U || !engine.gui().ready()) { return 7; }
	auto &target_a = renderer.targets.front();
	auto &target_b = renderer.targets.back();
	if (target_a.handle != a->info().handle || target_b.handle != b->info().handle ||
		!target_a.guiWindow || target_b.guiWindow || !target_b.imageViews.ownedGuiViews.empty()) { return 8; }
	if (target_a.camera || !target_b.camera) { return 28; }
	render.waitIdle();
	const auto initial_a = uniforms(target_a);
	const auto initial_b = uniforms(target_b);
	if (matrixDifference(initial_a.view, initial_b.view) > 1e-5F ||
		std::abs(initial_a.projection[0][0] - initial_b.projection[0][0] * 1.5F) > 1e-5F ||
		matrixDifference(initial_a.shadowViewProjs[vve::simple::kShadowMatrixDirBase],
			initial_b.shadowViewProjs[vve::simple::kShadowMatrixDirBase]) < 1e-3F) { return 29; }
	// A non-GUI window still delivers normalized motion to the shared input state.
	SDL_Event motion{};
	motion.type = SDL_EVENT_MOUSE_MOTION;
	motion.motion.windowID = SDL_GetWindowID(b->native());
	motion.motion.x = 48.0F;
	motion.motion.y = 16.0F;
	ImGui::GetIO().WantCaptureMouse = false;
	if (!SDL_PushEvent(&motion) || !windows.poll()) { return 24; }
	const auto mouse = windows.input().mousePosition(b->info().handle);
	if (!mouse || std::abs(mouse->x - 0.5F) > 1e-5F || std::abs(mouse->y - 0.25F) > 1e-5F) { return 25; }
	const auto path = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(path);
	if (!checkCapture(render, a->info().handle, path / "a.png", {64, 64}, true) ||
		!checkCapture(render, b->info().handle, path / "b.png", {96, 64}, false)) { return 9; }
	const auto no_target = render.captureFrameToPng(unused->info().handle, path / "unused.png");
	const auto unknown = render.captureFrameToPng(vve::WindowHandle{}, path / "unknown.png");
	if (no_target || no_target.error() != vve::Error::invalid_handle || unknown || unknown.error() != vve::Error::invalid_handle) { return 10; }
	// Retained settings affect only their own window; GUI recording happens only for a's captures.
	settings[1]->gain[0] = 0.0F;
	if (!checkCapture(render, b->info().handle, path / "b-effect.png", {96, 64}, false, false) ||
		!checkCapture(render, a->info().handle, path / "a-effect.png", {64, 64}, true)) { return 11; }
	settings[1]->gain[0] = 1.0F;
	if (gui_calls != 4U) { return 12; }

	// Only b changes size; a retains its attachments and the renderer retains its shared pipeline.
	const VkSwapchainKHR a_swapchain = target_a.swapchain.swapchain;
	if (!SDL_SetWindowSize(b->native(), 112, 80) || !SDL_SyncWindow(b->native())) { return 13; }
	for (const auto frame : {1U, 2U}) {
		if (!engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return 14; }
		std::println("[MultiWindowRenderTests] resize_frame={} presented=2", frame);
	}
	const auto extent = renderer.currentWindowPixelExtent(target_b);
	const bool ignored = extent.width == 96U && extent.height == 64U;
	if (ignored && renderer.recreateSwapchain(target_b, extent) != VK_SUCCESS) { return 15; }
	std::println("[MultiWindowRenderTests] resize={} b={}x{}", ignored ? "explicit-recreate" : "window", extent.width, extent.height);
	if (target_b.swapchain.extent.width != extent.width || target_b.swapchain.extent.height != extent.height ||
		target_a.swapchain.swapchain != a_swapchain || target_a.swapchain.extent.width != 64U || target_a.swapchain.extent.height != 64U ||
		chains.size() != 2U || target_a.postProcess.get() != chains[0] || target_b.postProcess.get() != chains[1]) { return 16; }
	if (!checkCapture(render, a->info().handle, path / "a-resized.png", {64, 64}, true) ||
		!checkCapture(render, b->info().handle, path / "b-resized.png", {extent.width, extent.height}, false)) { return 17; }
	if (!checkWindowCameras(engine, path)) { return 30; }
	if (renderer.forwardPipelineCreateCount() != 1U) { return 18; }

	// Closing the GUI window retires its target; the surviving target never inherits the GUI pass.
	a->info().should_close = true;
	const auto closed_camera = render.setCamera(a->info().handle, initial_camera);
	const auto closed_clear = render.clearCamera(a->info().handle);
	if (closed_camera || closed_camera.error() != vve::Error::invalid_handle ||
		closed_clear || closed_clear.error() != vve::Error::invalid_handle) { return 31; }
	const auto status = engine.step();
	if (!status || *status != vve::FrameStatus::stopped) { return 19; }
	const auto calls_before_close = gui_calls;
	const auto frames_before_close = render.renderedFrameCount();
	if (!engine.renderFrame() || render.lastRenderedWindowCount() != 1U || renderer.targets.size() != 1U ||
		render.renderedFrameCount() != frames_before_close + 1U || gui_calls != calls_before_close ||
		!render.captureFrameToPng(path / "first-survivor.png")) { return 20; }
	const auto closed = render.captureFrameToPng(a->info().handle, path / "closed.png");
	if (closed || closed.error() != vve::Error::invalid_handle) { return 21; }
	b->info().should_close = true;
	if (!engine.renderFrame() || render.lastRenderedWindowCount() != 0U || !renderer.targets.empty() ||
		render.renderedFrameCount() != frames_before_close + 1U) { return 22; }
	// Keep the validation counter observable through GUI and renderer destruction.
	render.waitIdle();
	const bool validation_active = renderer.validationActive();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[MultiWindowRenderTests] setup_calls={} gui_calls={} pipelines={} validation_active={} cleanup_validation_errors={}",
		chains.size(), gui_calls, renderer.forwardPipelineCreateCount(), validation_active, renderer.validationErrorCount());
	return renderer.validationErrorCount() == 0U ? 0 : 23;
}
