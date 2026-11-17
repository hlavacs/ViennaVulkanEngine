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

	// 0b. CPU voxelization of a 1.8 m square plane with 0.5 m voxels: once between voxel boundaries, once on one
	{
		RenderScene empty{};
		const auto none = RadianceCascadesRenderer::voxelize(empty, 0.5F);
		if (!none || none->size != std::array{0U, 0U, 0U} || !none->albedo.empty()) { return 40; }

		const auto planeVoxels = [](Scalar height, Scalar voxelSize) {
			RenderScene scene{};
			const auto material = scene.addMaterial(RenderMaterial{.base_color = {.value = {0.2F, 0.4F, 0.6F}}, .emissive = {.value = {0.0F, 0.0F, 3.0F}}});
			const auto mesh = scene.addPlaneMesh(Vec2{0.9F, 0.9F});
			(void)scene.addInstance(mesh, material, {}, math::translate(identityMat4(), Vec3{0.0F, height, 0.0F}));
			return RadianceCascadesRenderer::voxelize(scene, voxelSize);
		};
		// Occupied voxels: count, the y layers they lie in, and whether every one carries the plane's averages.
		const auto occupancy = [](const RadianceCascadesVoxels &voxels) {
			std::size_t count{};
			std::set<std::uint32_t> layers{};
			bool averages{true};
			for (std::size_t index{}; index < voxels.albedo.size(); ++index) {
				if (voxels.albedo[index].w == 0.0F) { continue; }
				++count;
				layers.insert(static_cast<std::uint32_t>(index / voxels.size[0] % voxels.size[1]));
				const auto albedo = voxels.albedo[index], emissive = voxels.emissive[index], normal = voxels.normal[index];
				averages = averages && std::abs(albedo.x - 0.2F) < 1e-5F && std::abs(albedo.y - 0.4F) < 1e-5F && std::abs(albedo.z - 0.6F) < 1e-5F &&
					emissive.z == 3.0F && normal.y > 0.999F;
			}
			return std::tuple{count, layers, averages};
		};

		const auto between = planeVoxels(0.1F, 0.5F); // Plane inside voxel layer y in [0, 0.5).
		if (!between || between->size != std::array{6U, 3U, 6U} || between->origin.x != -1.5F || between->origin.y != -0.5F || between->origin.z != -1.5F) { return 41; }
		const auto [betweenCount, betweenLayers, betweenAverages] = occupancy(*between);
		if (betweenCount != 16U || betweenLayers != std::set{1U} || !betweenAverages) { return 42; }

		const auto onBoundary = planeVoxels(0.0F, 0.5F); // Plane exactly on the boundary between layers 0 and 1.
		if (!onBoundary || onBoundary->size != std::array{6U, 3U, 6U}) { return 43; }
		const auto [boundaryCount, boundaryLayers, boundaryAverages] = occupancy(*onBoundary);
		if (boundaryCount != 32U || boundaryLayers != std::set{0U, 1U} || !boundaryAverages) { return 44; }

		const auto zeroSize = planeVoxels(0.0F, 0.0F);
		if (zeroSize || zeroSize.error() != Error::invalid_argument) { return 45; }
		const auto tooFine = planeVoxels(0.0F, 1e-4F); // 18000 voxels along x.
		if (tooFine || tooFine.error() != Error::capacity_exceeded) { return 46; }
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

	// 7. Primary rays: a red cube on a green ground; the camera looks at the cube's centre from above the ground
	auto &render = engine.renderSystem();
	if (renderer.tracedInstanceCount() != 0U) { return 31; }
	if (!render.addPlane(Vec2{5.0F, 5.0F}, LinearColor{.value = {0.0F, 1.0F, 0.0F}}) ||
		!render.addCuboid(Vec3{-1.0F, 0.0F, -1.0F}, Vec3{1.0F, 2.0F, 1.0F}, LinearColor{.value = {1.0F, 0.0F, 0.0F}})) { return 32; }
	renderer.setCamera(Camera::lookAt(Position{{0.0F, 3.0F, 8.0F}}, Position{{0.0F, 1.0F, 0.0F}}));
	if (!renderer.rebuildScene() || renderer.tracedInstanceCount() != 2U) { return 33; }
	const auto scene_capture = directory / "scene.png";
	if (!renderer.renderFrame() || !renderer.captureFrameToPng(scene_capture)) { return 34; }
	const auto scene_pixels = test::imagePixels(scene_capture);
	if (!scene_pixels || scene_pixels->size() != std::size_t{extent.width} * extent.height * 4U) { return 35; }
	const auto pixel = [&](unsigned x, unsigned y) {
		const auto offset = (std::size_t{y} * extent.width + x) * 4U;
		return std::array<int, 3>{(*scene_pixels)[offset], (*scene_pixels)[offset + 1U], (*scene_pixels)[offset + 2U]};
	};
	const auto centre = pixel(extent.width / 2U, extent.height / 2U);   // Cube.
	const auto bottom = pixel(extent.width / 2U, extent.height - 2U);   // Ground in front of the cube.
	const auto corner = pixel(1U, 1U);                                  // Above the horizon.
	const bool scene_ok = centre == std::array{255, 0, 0} && bottom == std::array{0, 255, 0} &&
		std::abs(corner[0] - 124) <= 3 && std::abs(corner[1] - 188) <= 3 && corner[2] == 255;

	// 7b. Voxel view: plane [-5, 5] and cube [0, 2] in y lie on 0.25 m voxel boundaries, the hardest case for lookups.
	// Every hit must land in an occupied voxel (no magenta). Uniform materials give the same colours as above, except
	// at the cube's base, where voxels touched by both the cube and the ground hold a red-green average.
	if (renderer.voxelGrid() != std::array{43U, 11U, 43U}) { return 36; }
	renderer.setView(RadianceCascadesView::voxel_albedo);
	const auto voxel_capture = directory / "voxels.png";
	if (!renderer.captureFrameToPng(voxel_capture)) { return 37; }
	renderer.setView(RadianceCascadesView::shaded);
	const auto voxel_pixels = test::imagePixels(voxel_capture);
	if (!voxel_pixels || voxel_pixels->size() != scene_pixels->size()) { return 38; }
	std::size_t magenta{}, voxel_differences{}, unexplained_differences{};
	for (std::size_t offset{}; offset < voxel_pixels->size(); offset += 4U) {
		const auto r = (*voxel_pixels)[offset], g = (*voxel_pixels)[offset + 1U], b = (*voxel_pixels)[offset + 2U];
		magenta += r == 255 && g == 0 && b == 255;
		const bool differs = r != (*scene_pixels)[offset] || g != (*scene_pixels)[offset + 1U] || b != (*scene_pixels)[offset + 2U];
		voxel_differences += differs;
		unexplained_differences += differs && !(b == 0 && r > 0 && g > 0);
	}

	// 8. Shutdown, then a second init on the same window, which uploads the bound scene again
	const bool validation = renderer.validationActive();
	renderer.shutdown();
	if (renderer.initialized() || renderer.tracedInstanceCount() != 0U) { return 17; }
	const auto after = renderer.renderFrame();
	if (after || after.error() != Error::not_initialized) { return 18; }
	std::uint64_t errors = renderer.validationErrorCount();
	if (!renderer.init(window) || renderer.tracedInstanceCount() != 2U || !renderer.renderFrame()) { return 19; }
	renderer.shutdown();
	errors += renderer.validationErrorCount();

	std::println("[RadianceCascadeRendererTests] wrong_pixels={} resize_requested={} extent={}x{} resized_wrong_pixels={} validation_active={} validation_errors={}",
		wrong, resized, extent.width, extent.height, resized_wrong, validation, errors);
	std::println("[RadianceCascadeRendererTests] scene centre={},{},{} bottom={},{},{} corner={},{},{} scene_ok={}",
		centre[0], centre[1], centre[2], bottom[0], bottom[1], bottom[2], corner[0], corner[1], corner[2], scene_ok);
	std::println("[RadianceCascadeRendererTests] voxel_view magenta_pixels={} red_green_mixtures={} other_differences={}",
		magenta, voxel_differences - unexplained_differences, unexplained_differences);
	return wrong == 0U && resized_wrong == 0U && scene_ok && magenta == 0U && unexplained_differences == 0U && errors == 0U ? 0 : 30;
}
