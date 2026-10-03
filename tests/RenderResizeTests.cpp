/**
 * @file
 * @brief Checks swapchain resizing and PNG extents without rebuilding the forward pipeline.
 */
#include <SDL3/SDL_video.h>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>

import std;
import VVE.TestSupport;
import VVEngine.Simple;

/// @brief Exercises a hidden-window resize, or explicit recreation when the driver ignores it.
int main() {
	auto engine = vve::test::hiddenEngine("render-resize-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 1; }
	auto &render = engine.renderSystem();
	// Draw geometry in both pipelines so validation also checks their dynamic state.
	const auto object = render.addCuboid(vve::Vec3{-1.0F, -1.0F, -1.0F}, vve::Vec3{1.0F, 1.0F, 1.0F},
		vve::LinearColor{.value = {1.0F, 0.0F, 0.0F}});
	if (!object || !render.setObjectUnlit(*object, true)) { return 2; }
	render.setCamera(vve::Camera::lookAt(vve::Position{{0.0F, 0.0F, 4.0F}}, vve::Position{}));
	// A red-channel gain makes the retained post chain observable in pixels after either resize path.
	render.setPostProcessSetup([](vvppl::PostProcessing &chain) { chain.addColorGrade().gain[0] = 0.25F; });
	render.setDirectionalLight(vve::Direction{.value = vve::Vec3{0.25F, -1.0F, 0.35F}},
		vve::LinearColor{}, vve::LightIntensity{}, vve::LinearColor{});
	if (!engine.renderFrame() || render.renderedFrameCount() != 1U) { return 3; }
	auto &renderer = render.forward();
	const auto initial_extent = renderer.currentWindowPixelExtent(renderer.targets.front());
	if (renderer.forwardPipelineCreateCount() != 1U) { return 4; }

	// SDL_SyncWindow waits for the platform's resize request before the next two frames.
	if (!SDL_SetWindowSize(renderer.targets.front().window, 96, 80) || !SDL_SyncWindow(renderer.targets.front().window)) { return 5; }
	for (const auto frame : {1U, 2U}) {
		if (!engine.renderFrame()) { return 6; }
		std::println("[RenderResizeTests] resize_frame={} presented_windows={}", frame, render.lastRenderedWindowCount());
	}
	const auto extent = renderer.currentWindowPixelExtent(renderer.targets.front());
	const bool ignored_resize = extent.width == initial_extent.width && extent.height == initial_extent.height;
	std::println("[RenderResizeTests] path={} initial={}x{} requested=96x80 drawable={}x{} swapchain={}x{}",
		ignored_resize ? "explicit-recreate-hidden-resize-ignored" : "window-resize",
		initial_extent.width, initial_extent.height, extent.width, extent.height,
		renderer.targets.front().swapchain.extent.width, renderer.targets.front().swapchain.extent.height);
	if (ignored_resize) {
		// Drivers may keep hidden surfaces at their original extent; still exercise replacement.
		if (renderer.recreateSwapchain(renderer.targets.front(), extent) != VK_SUCCESS) { return 7; }
		// Recreation clears the last presented image; establish a frame before requesting a capture.
		const auto rendered = engine.renderFrame();
		std::println("[RenderResizeTests] recreated_presented={}", render.lastRenderedWindowCount());
		if (!rendered || render.lastRenderedWindowCount() != 1U) { return 14; }
	} else {
		if (renderer.targets.front().swapchain.extent.width != extent.width || renderer.targets.front().swapchain.extent.height != extent.height) { return 8; }
	}
	// Both paths must retain the effect and capture the current drawable size.
	{
		const auto path = std::filesystem::path{VVE_TEST_TMP_DIR} / "resized.png";
		std::filesystem::create_directories(path.parent_path());
		std::filesystem::remove(path);
		if (!render.captureFrameToPng(path)) { return 9; }
		int width{}, height{}, channels{};
		auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
			stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
		std::println("[RenderResizeTests] png={}x{} expected={}x{}", width, height, extent.width, extent.height);
		if (!pixels || width != static_cast<int>(extent.width) || height != static_cast<int>(extent.height)) { return 10; }
		const auto *centre = vve::test::pixelAt(pixels.get(), width, width / 2, height / 2);
		std::println("[RenderResizeTests] post_colour={},{},{} expected=137,0,0", centre[0], centre[1], centre[2]);
		if (std::abs(static_cast<int>(centre[0]) - 137) > 2 || centre[1] > 2 || centre[2] > 2) { return 13; }
	}

	// Wait for validation callbacks before checking the counter on either resize path.
	render.waitIdle();
	std::println("[RenderResizeTests] forwardPipelineCreateCount={} validationActive={} validationErrorCount={}",
		renderer.forwardPipelineCreateCount(), renderer.validationActive(), renderer.validationErrorCount());
	if (renderer.forwardPipelineCreateCount() != 1U) { return 11; }
	if (renderer.validationErrorCount() != 0U) { return 12; }
	// Keep validation observable through final image-view and swapchain destruction too.
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderResizeTests] cleanup_validation_errors={}", renderer.validationErrorCount());
	return renderer.validationErrorCount() == 0U ? 0 : 12;
}
