#include <vulkan/vulkan_core.h>
#include <VVPPL.h>

/**
 * @file
 * @brief Frame accounting for uninitialized, presented and skipped rendering calls.
 *
 * Functional objects:
 * - main checks CPU-only rejection, presentation and minimized-window resource retention.
 */

import std;

import VEEngine;
import VVE.TestSupport;
import VEEngine.Simple;

/// @brief Skips minimized windows without retiring targets, moving the GUI or losing camera and effect state.
bool checkMinimizedWindows() {
	auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "a", .extent = {64, 64}, .visible = false},
		vve::WindowDesc{.id = "b", .extent = {64, 64}, .visible = false}}}};
	if (!engine.init()) { return false; }
	auto *a = engine.windowSystem().findWindow("a");
	auto *b = engine.windowSystem().findWindow("b");
	if (!a || !b) { return false; }
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	const auto camera = vve::Camera::lookAt(vve::Position{{4.0F, 6.0F, 9.0F}}, vve::Position{});
	if (!render.setCamera(b->info().handle, camera)) { return false; }
	std::vector<vvppl::PostProcessing *> chains{};
	render.setPostProcessSetup([&](vvppl::PostProcessing &chain) {
		chains.push_back(&chain);
		(void)chain.addColorGrade();
	});
	std::size_t gui_calls{};
	engine.gui().draw([&] { ++gui_calls; });
	// Nothing drawable on the first call must leave lazy GPU initialization and pending cameras intact.
	a->info().minimized = b->info().minimized = true;
	if (!engine.renderFrame() || render.renderedFrameCount() != 0U || render.lastRenderedWindowCount() != 0U ||
		render.initialized() || !renderer.targets.empty() || !chains.empty() || gui_calls != 0U) { return false; }
	a->info().minimized = b->info().minimized = false;
	if (!engine.renderFrame() || render.lastRenderedWindowCount() != 2U || render.renderedFrameCount() != 1U ||
		renderer.targets.size() != 2U || chains.size() != 2U || gui_calls != 1U) { return false; }
	auto *target_a = &renderer.targets.front();
	auto *target_b = &renderer.targets.back();
	const auto swapchains = std::array<VkSwapchainKHR, 2>{target_a->swapchain.swapchain, target_b->swapchain.swapchain};

	// One minimized window leaves the other presenting and counts the logical frame once.
	b->info().minimized = true;
	if (!engine.renderFrame()) { return false; }
	std::println("minimized=b frames={} windows={}", render.renderedFrameCount(), render.lastRenderedWindowCount());
	if (render.lastRenderedWindowCount() != 1U || render.renderedFrameCount() != 2U || gui_calls != 2U) { return false; }
	const auto fps = render.renderingFramesPerSecond();
	a->info().minimized = true;
	if (!engine.renderFrame()) { return false; }
	std::println("minimized=both frames={} windows={}", render.renderedFrameCount(), render.lastRenderedWindowCount());
	if (render.lastRenderedWindowCount() != 0U || render.renderedFrameCount() != 2U ||
		render.renderingFramesPerSecond() != fps || gui_calls != 2U) { return false; }
	// Restoring only b must not transfer the GUI from its still-minimized owner a.
	b->info().minimized = false;
	if (!engine.renderFrame() || render.lastRenderedWindowCount() != 1U || render.renderedFrameCount() != 3U ||
		gui_calls != 2U) { return false; }
	a->info().minimized = false;
	if (!engine.renderFrame()) { return false; }
	std::println("minimized=none frames={} windows={} gui_calls={}", render.renderedFrameCount(),
		render.lastRenderedWindowCount(), gui_calls);
	if (render.lastRenderedWindowCount() != 2U || render.renderedFrameCount() != 4U || gui_calls != 3U ||
		renderer.targets.size() != 2U || &renderer.targets.front() != target_a || &renderer.targets.back() != target_b ||
		target_a->swapchain.swapchain != swapchains[0] || target_b->swapchain.swapchain != swapchains[1] ||
		chains.size() != 2U || target_a->postProcess.get() != chains[0] || target_b->postProcess.get() != chains[1] ||
		!target_a->guiWindow || target_b->guiWindow || target_a->camera || !target_b->camera ||
		target_b->camera->position.value.x != camera.position.value.x || renderer.forwardPipelineCreateCount() != 1U) { return false; }
	// Include pending GPU work and target destruction in the validation check.
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("minimized cleanup_validation_errors={}", renderer.validationErrorCount());
	return renderer.validationErrorCount() == 0U;
}

/// @brief Counts only presented frames and leaves skipped calls successful without advancing statistics.
int main() {
	// An uninitialized render system must reject a frame without creating a window or Vulkan device.
	auto render = vve::simple::RenderSystem{};
	auto windows = vve::simple::WindowSystem{};
	const auto uninitialized = render.renderFrame(windows);
	std::cout << "uninitialized error=" << (uninitialized ? "none" : vve::errorName(uninitialized.error()))
		<< " frames=" << render.renderedFrameCount() << " windows=" << render.lastRenderedWindowCount() << '\n';
	if (uninitialized || uninitialized.error() != vve::Error::not_initialized ||
		render.renderedFrameCount() != 0U || render.lastRenderedWindowCount() != 0U ||
		render.renderingFramesPerSecond() != 0.0) { return 1; }
	if (!checkMinimizedWindows()) { return 10; }

	// A capped run must present both frames, including the step that reports stopped.
	{
		auto run_engine = vve::test::hiddenEngine("render-frame-count-run-tests", vve::PixelExtent{.width = 64, .height = 64}, vve::MaxFrames{2});
		const auto result = run_engine.run();
		const auto &run_render = run_engine.renderSystem();
		std::cout << "run error=" << (result ? "none" : vve::errorName(result.error()))
			<< " frames=" << run_render.renderedFrameCount() << " expected=2\n";
		if (!result || run_render.renderedFrameCount() != 2U) { return 8; }
		const auto &run_renderer = run_render.forward();
		std::cout << "run validationActive=" << run_renderer.validationActive()
			<< " validationErrorCount=" << run_renderer.validationErrorCount() << '\n';
		if (run_renderer.validationActive() && run_renderer.validationErrorCount() != 0U) { return 9; }
	}

	// Engine::renderFrame initializes the renderer lazily on the hidden SDL window.
	auto engine = vve::test::hiddenEngine("render-frame-count-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 2; }
	auto &active_render = engine.renderSystem();
	// Every successful presentation contributes exactly one frame and one rendered window.
	for (const auto expected : {1U, 2U, 3U}) {
		if (!engine.renderFrame()) { return 3; }
		std::cout << "presented frames=" << active_render.renderedFrameCount()
			<< " expected=" << expected << " windows=" << active_render.lastRenderedWindowCount() << '\n';
		if (active_render.renderedFrameCount() != expected || active_render.lastRenderedWindowCount() != 1U) { return 4; }
	}

	// A missing borrowed window deterministically yields zero drawable extent, without changing the live SDL window.
	auto &renderer = active_render.forward();
	const auto fps = active_render.renderingFramesPerSecond();
	auto *native = std::exchange(renderer.targets.front().window, nullptr);
	const auto skipped = engine.renderFrame();
	renderer.targets.front().window = native;
	std::cout << "skipped success=" << skipped.has_value() << " frames=" << active_render.renderedFrameCount()
		<< " windows=" << active_render.lastRenderedWindowCount() << '\n';
	if (!skipped || active_render.renderedFrameCount() != 3U || active_render.lastRenderedWindowCount() != 0U ||
		active_render.renderingFramesPerSecond() != fps) { return 5; }
	if (!engine.renderFrame() || active_render.renderedFrameCount() != 4U ||
		active_render.lastRenderedWindowCount() != 1U) { return 6; }

	// Include the skipped frame and resumed presentation in the validation check.
	std::cout << "RenderFrameCountTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 7; }
	return 0;
}
