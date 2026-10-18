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

import VVEngine;
import VVE.TestSupport;
import VVEngine.Simple;
import VVEngine.Simple.RenderResources;

/// @brief Keeps a pre-acquire source decode failure retryable after clearing its cache, without renderer reinitialization.
bool checkFrameDecodeRecovery() {
	const auto path = std::filesystem::path{VVE_TEST_TMP_DIR} / "decode-retry.ppm";
	std::filesystem::create_directories(path.parent_path());
	const auto writeSource = [&] {
		std::ofstream image{path, std::ios::binary};
		image << "P6\n1 1\n255\n" << "\x80\x40\xff";
		return image.good();
	};
	if (!writeSource()) { return false; }
	vve::simple::RenderScene scene; // Declared first so the renderer's borrowed scene survives engine destruction.
	auto engine = vve::test::hiddenEngine("render-decode-retry", vve::PixelExtent{64, 64});
	vve::test::configureRenderer(engine.renderSystem());
	if (!engine.init() || !engine.renderFrame()) { return false; }
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	const auto slot = scene.acquireTexture(path);
	if (!slot) { return false; }
	const auto generation = scene.findTexture(*slot)->generation;
	// Simulate released upload pixels, then lose their source before the renderer needs to restore them.
	scene.releaseUploadedPixels([&](std::size_t) { return generation; });
	if (!scene.findTexture(*slot)->rgba8.empty() || !std::filesystem::remove(path)) { return false; }
	renderer.bindRenderScene(scene);
	renderer.markSceneResourcesDirty();
	auto &target = renderer.targets.front();
	const auto frame = target.currentFrame;
	const auto serials = target.submitSerials;
	const auto uploads = renderer.textureUploadSubmitCount();
	const auto decodes = scene.textureDecodeCount();
	const auto skipped = renderer.skippedFrameCount();
	std::ostringstream diagnostic;
	auto *previous = std::cerr.rdbuf(diagnostic.rdbuf());
	const auto failed = engine.renderFrame();
	std::cerr.rdbuf(previous);
	if (failed || failed.error() != vve::Error::io_error || scene.textureDecodeCount() != decodes + 1U ||
		diagnostic.str().find("scene sync") == std::string::npos || diagnostic.str().find("VkResult -3") == std::string::npos) { return false; }
	if (!writeSource()) { return false; }
	const auto cached = engine.renderFrame();
	std::println("decode failure={} cached={} decodes={} frame={} uploads={} diagnostic={}",
		failed ? "none" : vve::errorName(failed.error()), cached ? "none" : vve::errorName(cached.error()),
		scene.textureDecodeCount() - decodes, target.currentFrame, renderer.textureUploadSubmitCount() - uploads, diagnostic.str());
	if (cached || cached.error() != vve::Error::io_error || scene.textureDecodeCount() != decodes + 1U ||
		target.currentFrame != frame || target.submitSerials != serials || renderer.textureUploadSubmitCount() != uploads ||
		renderer.skippedFrameCount() != skipped || render.renderedFrameCount() != 1U || render.lastRenderedWindowCount() != 0U ||
		vkGetFenceStatus(renderer.device.device, target.frameSync.inFlightFences[frame]) != VK_SUCCESS) { return false; }
	// Clear the bound test scene's decode cache and the render system's empty scene; neither shuts down the GPU.
	scene.clear();
	render.clearScene();
	if (!scene.acquireTexture(path) || !engine.renderFrame() || render.renderedFrameCount() != 2U ||
		render.lastRenderedWindowCount() != 1U || renderer.gpuTextureCount() != 1U) { return false; }
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("decode recovered=true cleanup_validation_errors={}", renderer.validationErrorCount());
	return renderer.validationErrorCount() == 0U;
}

/// @brief Propagates deterministic rendering setup failures through the public step and run facade.
bool checkFacadeFrameFailure() {
	auto engine = vve::Engine<>{vve::MaxFrames{1}, vve::WindowSetups{vve::WindowSetup{}
		.id("main").extent(vve::PixelExtent{64, 64}).visible(false)}};
	if (!engine.init()) { return false; }
	auto world = engine.world();
	auto &render = world.get<vve::RenderSystem>();
	vve::test::configureRenderer(render);
	std::size_t setup_calls{};
	// Target creation catches this exception and returns platform_error before any frame is acquired or submitted.
	render.setPostProcessSetup([&](vvppl::PostProcessing &) {
		++setup_calls;
		throw std::runtime_error{"injected setup failure"};
	});
	const auto step = engine.step();
	const auto run = engine.run();
	std::println("facade failure step={} run={} setup_calls={} frames={} windows={}",
		step ? "none" : vve::errorName(step.error()), run ? "none" : vve::errorName(run.error()),
		setup_calls, render.renderedFrameCount(), render.lastRenderedWindowCount());
	return !step && step.error() == vve::Error::platform_error && !run && run.error() == vve::Error::platform_error &&
		setup_calls == 2U && render.renderedFrameCount() == 0U && render.lastRenderedWindowCount() == 0U;
}

/// @brief Rejects missing frame resources before acquisition, preserving prior window counts and retaining the fatal error.
bool checkFatalFramePropagation() {
	auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "a", .extent = {64, 64}, .visible = false},
		vve::WindowDesc{.id = "b", .extent = {64, 64}, .visible = false}}}};
	vve::test::configureRenderer(engine.renderSystem());
	if (!engine.init() || !engine.renderFrame()) { return false; }
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	auto &target = renderer.targets.back();
	const auto frame = target.currentFrame;
	const auto serials = target.submitSerials;
	const auto skipped = renderer.skippedFrameCount();
	// Keep every RAII owner alive while triggering the production frame-resource guard; no invalid Vulkan call is made.
	auto commands = std::move(target.commandBuffers.ownedCommandBuffers);
	std::ostringstream diagnostic;
	auto *previous = std::cerr.rdbuf(diagnostic.rdbuf());
	const auto failed = engine.renderFrame();
	std::cerr.rdbuf(previous);
	target.commandBuffers.ownedCommandBuffers = std::move(commands);
	std::println("fatal error={} frames={} windows={} diagnostic={}", failed ? "none" : vve::errorName(failed.error()),
		render.renderedFrameCount(), render.lastRenderedWindowCount(), diagnostic.str());
	if (failed || failed.error() != vve::Error::platform_error || render.renderedFrameCount() != 2U ||
		render.lastRenderedWindowCount() != 1U || target.currentFrame != frame || target.submitSerials != serials ||
		renderer.skippedFrameCount() != skipped || diagnostic.str().find("frame resources") == std::string::npos ||
		diagnostic.str().find("VkResult -3") == std::string::npos) { return false; }
	// Restoring resources cannot turn a fatal error into a successful skip; run must return it too.
	const auto repeated = engine.renderFrame();
	const auto run = engine.run();
	std::println("fatal repeated={} run={} frames={} windows={}", repeated ? "none" : vve::errorName(repeated.error()),
		run ? "none" : vve::errorName(run.error()), render.renderedFrameCount(), render.lastRenderedWindowCount());
	if (repeated || repeated.error() != vve::Error::platform_error || run || run.error() != vve::Error::platform_error ||
		render.renderedFrameCount() != 2U || render.lastRenderedWindowCount() != 0U) { return false; }
	// Cleanup is the explicit recovery boundary; no acquired image or unsignaled fence was left by the injected failure.
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	if (renderer.validationErrorCount() != 0U || !render.initialize(engine.windowSystem()) ||
		!render.renderFrame(engine.windowSystem()) || render.lastRenderedWindowCount() != 2U ||
		render.renderedFrameCount() != 3U) { return false; }
	render.shutdown();
	std::println("fatal recovered=true cleanup_validation_errors={}", renderer.validationErrorCount());
	return renderer.validationErrorCount() == 0U;
}

/// @brief Skips minimized windows without retiring targets, moving the GUI or losing camera and effect state.
bool checkMinimizedWindows() {
	auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "a", .extent = {64, 64}, .visible = false},
		vve::WindowDesc{.id = "b", .extent = {64, 64}, .visible = false}}}};
	vve::test::configureRenderer(engine.renderSystem());
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
	if (!checkFatalFramePropagation()) { return 11; }
	if (!checkFacadeFrameFailure()) { return 12; }
	if (!checkFrameDecodeRecovery()) { return 13; }

	// A capped run must present both frames, including the step that reports stopped.
	{
		auto run_engine = vve::test::hiddenEngine("render-frame-count-run-tests", vve::PixelExtent{.width = 64, .height = 64}, vve::MaxFrames{2});
		vve::test::configureRenderer(run_engine.renderSystem());
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
	vve::test::configureRenderer(engine.renderSystem());
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
