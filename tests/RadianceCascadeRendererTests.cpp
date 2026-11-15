/** @file @brief Radiance cascades renderer: CPU layout and settings checks, then the standalone Vulkan bring-up on a hidden window. */
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

import std;
import VVE.TestSupport;
import VVEngine;
import VVEngine.Simple;

/// @brief Counts pixels that differ from the expected RGBA value by more than the tolerance.
static std::size_t wrongPixels(const std::vector<unsigned char> &pixels, std::array<int, 4> expected) {
	std::size_t wrong{};
	for (std::size_t offset{}; offset < pixels.size(); offset += 4U) {
		for (const auto channel : std::views::iota(0U, 4U)) {
			if (std::abs(static_cast<int>(pixels[offset + channel]) - expected[channel]) > 3) { ++wrong; break; }
		}
	}
	return wrong;
}

int main() {
	using namespace vve;
	using namespace vve::simple;

	// 0. Default cascade layout (paper's 3D law): spacing, direction side and interval length double per cascade
	{
		RadianceCascadesRenderer layout{};
		const Vec3 volume{16.0F, 8.0F, 4.0F};
		if (layout.probeSpacing(0U) != 0.5F || layout.probeSpacing(2U) != 2.0F) { return 20; }
		if (layout.directionSide(0U) != 4U || layout.directionSide(2U) != 16U) { return 21; }
		if (layout.probeCount(0U, volume) != std::array{33U, 17U, 9U} || layout.probeCount(1U, volume) != std::array{17U, 9U, 5U}) { return 22; }
		const auto atlas = layout.atlasExtent(1U, volume);
		if (atlas.width != 17U * 8U || atlas.height != 9U * 8U) { return 23; }
		if (layout.intervalStart(0U) != 0.0F || layout.intervalEnd(0U) != 0.5F || layout.intervalEnd(1U) != 1.5F ||
			layout.intervalEnd(2U) != 3.5F) { return 24; }
		// Settings: malformed values are rejected, valid ones change the layout
		const auto bad = layout.setSettings({.cascade_count = 0U});
		if (bad || bad.error() != Error::invalid_argument) { return 25; }
		if (!layout.setSettings({.base_probe_spacing = 1.0F, .spatial_scale = 3U}) || layout.probeSpacing(2U) != 9.0F) { return 26; }
	}

	// 1. A window the forward renderer does not use; the engine only provides the SDL window
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	constexpr unsigned side = 64;
	simple::Engine engine{ApplicationName{"radiance-cascade-renderer-tests"}, simple::Windows{.value = {WindowDesc{.id = "main",
		.title = "radiance-cascade-renderer-tests", .extent = PixelExtent{side, side}, .renderer_id = RendererId{.value = "none"}, .visible = false}}}};
	if (!engine.init()) { return 1; }
	SDL_Window *const window = engine.windowSystem().findWindow("main")->native();
	auto &renderer = engine.renderSystem().radianceCascades();

	// 2. State before init
	if (renderer.initialized() || renderer.renderedFrameCount() != 0U) { return 2; }
	const auto early = renderer.renderFrame();
	if (early || early.error() != Error::not_initialized) { return 3; }
	const auto null_window = renderer.init(nullptr);
	if (null_window || null_window.error() != Error::invalid_argument) { return 4; }

	// 3. Init creates the renderer's own ray tracing device; a repeat and late settings are rejected
	if (!renderer.init(window) || !renderer.initialized()) { return 5; }
	const auto again = renderer.init(window);
	if (again || again.error() != Error::already_initialized) { return 6; }
	const auto late = renderer.setSettings({});
	if (late || late.error() != Error::already_initialized) { return 7; }
	if (renderer.extent().width != side || renderer.extent().height != side) { return 8; }

	// 4. Frames are presented and counted; execute() is an alias
	if (!renderer.renderFrame() || renderer.renderedFrameCount() != 1U) { return 9; }
	if (!renderer.execute() || renderer.renderedFrameCount() != 2U) { return 10; }

	// 5. An empty frame is the background colour: linear 0.2, 0.5, 1 encodes to about 124, 188, 255 in sRGB
	renderer.setBackgroundColor(LinearColor{.value = Vec3{0.2F, 0.5F, 1.0F}});
	const auto capture = directory / "background.png";
	if (!renderer.captureFrameToPng(capture) || renderer.renderedFrameCount() != 2U) { return 11; }
	const auto pixels = test::imagePixels(capture);
	if (!pixels || pixels->size() != side * side * 4U) { return 12; }
	const auto wrong = wrongPixels(*pixels, {124, 188, 255, 255});

	// 6. Resize: the swapchain follows the window; hidden windows may ignore the request, the frames must still present
	const bool resized = SDL_SetWindowSize(window, 96, 80) && SDL_SyncWindow(window);
	for (int frame{}; frame < 2; ++frame) { if (!renderer.renderFrame()) { return 13; } }
	int width{}, height{};
	SDL_GetWindowSizeInPixels(window, &width, &height);
	const auto extent = renderer.extent();
	if (extent.width != static_cast<unsigned>(width) || extent.height != static_cast<unsigned>(height)) { return 14; }
	const auto resized_capture = directory / "resized.png";
	if (!renderer.captureFrameToPng(resized_capture)) { return 15; }
	const auto resized_pixels = test::imagePixels(resized_capture);
	if (!resized_pixels || resized_pixels->size() != std::size_t{extent.width} * extent.height * 4U) { return 16; }
	const auto resized_wrong = wrongPixels(*resized_pixels, {124, 188, 255, 255});

	// 7. Shutdown, then a second init on the same window
	const bool validation = renderer.validationActive();
	renderer.shutdown();
	if (renderer.initialized()) { return 17; }
	const auto after = renderer.renderFrame();
	if (after || after.error() != Error::not_initialized) { return 18; }
	std::uint64_t errors = renderer.validationErrorCount();
	if (!renderer.init(window) || !renderer.renderFrame()) { return 19; }
	renderer.shutdown();
	errors += renderer.validationErrorCount();

	std::println("[RadianceCascadeRendererTests] wrong_pixels={} resize_requested={} extent={}x{} resized_wrong_pixels={} validation_active={} validation_errors={}",
		wrong, resized, extent.width, extent.height, resized_wrong, validation, errors);
	return wrong == 0U && resized_wrong == 0U && errors == 0U ? 0 : 30;
}
