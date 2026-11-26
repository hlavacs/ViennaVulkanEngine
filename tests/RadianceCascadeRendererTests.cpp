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

	// 0a. Atlas memory and device limits: on a large volume each cascade needs about half the texels of the one before
	{
		RadianceCascadesRenderer layout{};
		const auto texels = [&layout](std::uint32_t cascade, Vec3 volume) {
			const auto atlas = layout.atlasExtent(cascade, volume);
			return std::uint64_t{atlas.width} * atlas.height * layout.probeCount(cascade, volume)[2];
		};
		const Vec3 large{64.0F, 64.0F, 64.0F};
		for (std::uint32_t cascade{}; cascade + 1U < layout.settings().cascade_count; ++cascade) {
			const double ratio = static_cast<double>(texels(cascade + 1U, large)) / static_cast<double>(texels(cascade, large));
			if (ratio < 0.45 || ratio > 0.55) { return 50; }
		}
		// Volume 16 x 8 x 4: the widest atlas is cascade 3 (5 probes x 32 = 160), the deepest cascade 0 (9 layers).
		const Vec3 volume{16.0F, 8.0F, 4.0F};
		if (!layout.atlasesFit(volume, 160U, 9U) || layout.atlasesFit(volume, 159U, 9U) || layout.atlasesFit(volume, 160U, 8U)) { return 51; }
		// The merge's child directions of texel (u, v) at side 4 are texels (2u + k, 2v + l) at side 8; they lie inside the
		// parent's octahedral cell, so each is within 45 degrees of the parent direction.
		for (std::uint32_t u{}; u < 4U; ++u) {
			for (std::uint32_t v{}; v < 4U; ++v) {
				const Vec3 parent = RadianceCascadesRenderer::octahedralDirection(u, v, 4U);
				for (std::uint32_t child{}; child < 4U; ++child) {
					const Vec3 direction = RadianceCascadesRenderer::octahedralDirection(2U * u + child % 2U, 2U * v + child / 2U, 8U);
					if (parent.x * direction.x + parent.y * direction.y + parent.z * direction.z < 0.7071F) { return 59; }
				}
			}
		}
		// A direction side that would wrap 32 bits is refused instead of fitting by accident.
		if (!layout.setSettings({.angular_scale = 1U << 16U}) || layout.atlasesFit(volume, 16384U, 2048U)) { return 52; }
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
	if (renderer.cascadeAtlasSize(0U) != std::array{0U, 0U, 0U}) { return 58; } // No geometry, no atlases.
	if (!render.addPlane(Vec2{5.0F, 5.0F}, LinearColor{.value = {0.0F, 1.0F, 0.0F}}) ||
		!render.addCuboid(Vec3{-1.0F, 0.0F, -1.0F}, Vec3{1.0F, 2.0F, 1.0F}, LinearColor{.value = {1.0F, 0.0F, 0.0F}})) { return 32; }
	renderer.setCamera(Camera::lookAt(Position{{0.0F, 3.0F, 8.0F}}, Position{{0.0F, 1.0F, 0.0F}}));
	if (!renderer.rebuildScene() || renderer.tracedInstanceCount() != 2U) { return 33; }
	// Cascade atlases over the 43 x 11 x 43 voxel volume (10.75 x 2.75 x 10.75 m): cascade 0 has 23 x 7 x 23 probes of 4 x 4 directions.
	const Vec3 probeVolume = renderer.probeVolumeExtent();
	if (probeVolume.x != 10.75F || probeVolume.y != 2.75F || probeVolume.z != 10.75F) { return 53; }
	if (renderer.cascadeAtlasSize(0U) != std::array{92U, 28U, 23U}) { return 54; }
	for (std::uint32_t cascade{}; cascade < renderer.settings().cascade_count; ++cascade) {
		const auto atlas = renderer.atlasExtent(cascade, probeVolume);
		if (renderer.cascadeAtlasSize(cascade) != std::array{atlas.width, atlas.height, renderer.probeCount(cascade, probeVolume)[2]}) { return 55; }
	}
	if (renderer.cascadeAtlasSize(renderer.settings().cascade_count) != std::array{0U, 0U, 0U}) { return 56; }
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
	if (renderer.cascadeAtlasSize(0U) != std::array{0U, 0U, 0U}) { return 57; }
	const auto after = renderer.renderFrame();
	if (after || after.error() != Error::not_initialized) { return 18; }
	std::uint64_t errors = renderer.validationErrorCount();
	if (!renderer.init(window) || renderer.tracedInstanceCount() != 2U || !renderer.renderFrame()) { return 19; }
	renderer.shutdown();
	errors += renderer.validationErrorCount();

	// 9. Interval tracing: a 1.8 m emissive square at y = 0.1 in its own scene. The voxel volume is 10 x 3 x 10 voxels
	// from (-1.25, -0.25, -1.25), so no probe lies in the plane. Every texel of cascades 0 and 1 must hold the plane's
	// emission with transparency 0 when its interval crosses the plane, else no radiance with transparency 1. Rays
	// within a millimetre of an interval end or the square's edge are ambiguous and skipped.
	RenderScene traced{}; // Outlives every use by the renderer, which is shut down below.
	const auto glow = traced.addMaterial(RenderMaterial{.base_color = {.value = {0.9F, 0.1F, 0.1F}}, .emissive = {.value = {0.2F, 0.4F, 0.6F}}});
	(void)traced.addInstance(traced.addPlaneMesh(Vec2{0.9F, 0.9F}), glow, {}, math::translate(identityMat4(), Vec3{0.0F, 0.1F, 0.0F}));
	renderer.bindRenderScene(traced);
	renderer.setMerging(false); // The atlases keep the raw intervals.
	if (!renderer.init(window) || renderer.tracedInstanceCount() != 1U || !renderer.renderFrame()) { return 60; }
	const Vec3 tracedVolume = renderer.probeVolumeExtent();
	if (tracedVolume.x != 2.5F || tracedVolume.y != 0.75F || tracedVolume.z != 2.5F) { return 61; }
	constexpr float planeY{0.1F}, planeHalf{0.9F}, ambiguity{1e-3F};
	const Vec3 probeOrigin{-1.25F, -0.25F, -1.25F};
	std::array<std::size_t, 2> intervalHits{}, intervalMisses{}, intervalMismatches{}, intervalSkipped{};
	for (std::uint32_t cascade{}; cascade < 2U; ++cascade) {
		const auto atlas = renderer.readCascadeAtlas(cascade);
		const auto size = renderer.cascadeAtlasSize(cascade);
		if (!atlas || atlas->size() != std::size_t{size[0]} * size[1] * size[2]) { return 62; }
		const std::uint32_t side = renderer.directionSide(cascade);
		const float spacing = renderer.probeSpacing(cascade), start = renderer.intervalStart(cascade), end = renderer.intervalEnd(cascade);
		for (std::uint32_t layer{}; layer < size[2]; ++layer) {
			for (std::uint32_t y{}; y < size[1]; ++y) {
				for (std::uint32_t x{}; x < size[0]; ++x) {
					const Vec3 probe{probeOrigin.x + static_cast<float>(x / side) * spacing, probeOrigin.y + static_cast<float>(y / side) * spacing,
						probeOrigin.z + static_cast<float>(layer) * spacing};
					const Vec3 direction = RadianceCascadesRenderer::octahedralDirection(x % side, y % side, side);
					bool hit{false};
					if (direction.y != 0.0F) { // A horizontal ray never reaches the plane.
						const float t = (planeY - probe.y) / direction.y;
						const float hx = std::abs(probe.x + t * direction.x), hz = std::abs(probe.z + t * direction.z);
						if (t > 0.0F && (std::abs(t - start) < ambiguity || std::abs(t - end) < ambiguity ||
							std::abs(hx - planeHalf) < ambiguity || std::abs(hz - planeHalf) < ambiguity)) {
							++intervalSkipped[cascade];
							continue;
						}
						hit = t >= start && t <= end && hx < planeHalf && hz < planeHalf;
					}
					const Vec4 expected = hit ? Vec4{0.2F, 0.4F, 0.6F, 0.0F} : Vec4{0.0F, 0.0F, 0.0F, 1.0F};
					const Vec4 actual = (*atlas)[x + std::size_t{size[0]} * (y + std::size_t{size[1]} * layer)];
					bool matches{true};
					for (int channel{}; channel < 4; ++channel) { matches = matches && std::abs(actual[channel] - expected[channel]) < 1e-3F; }
					++(hit ? intervalHits : intervalMisses)[cascade];
					intervalMismatches[cascade] += !matches;
				}
			}
		}
		const std::size_t checked = intervalHits[cascade] + intervalMisses[cascade];
		if (intervalHits[cascade] == 0U || intervalMisses[cascade] == 0U || intervalSkipped[cascade] * 100U > checked) { return 63; }
	}
	const auto badAtlas = renderer.readCascadeAtlas(renderer.settings().cascade_count);
	if (badAtlas || badAtlas.error() != Error::invalid_argument) { return 64; }

	// 9b. Atlas view of cascade 0, probe layer 2 (z = -0.25): every pixel shows either the emission (sRGB 124, 170, 203)
	// or the background (124, 188, 255), and both occur.
	renderer.setView(RadianceCascadesView::cascade_atlas);
	renderer.setDebugAtlas(0U, 2U);
	const auto atlas_capture = directory / "atlas.png";
	if (!renderer.captureFrameToPng(atlas_capture)) { return 65; }
	renderer.setView(RadianceCascadesView::shaded);
	const auto atlas_pixels = test::imagePixels(atlas_capture);
	if (!atlas_pixels || atlas_pixels->size() != std::size_t{extent.width} * extent.height * 4U) { return 66; }
	const std::size_t atlasPixelCount = atlas_pixels->size() / 4U;
	const std::size_t backgroundPixels = atlasPixelCount - wrongPixels(*atlas_pixels, {124, 188, 255, 255});
	const std::size_t emissionPixels = atlasPixelCount - wrongPixels(*atlas_pixels, {124, 170, 203, 255});
	if (backgroundPixels == 0U || emissionPixels == 0U || backgroundPixels + emissionPixels != atlasPixelCount) { return 67; }

	// 10. Merge, step by step: one merged frame must turn each cascade into its raw intervals merged with the merged next
	// cascade, as computed on the CPU from the two read-back atlases; the last cascade, which saw the sky, stays raw.
	const std::uint32_t cascadeCount = renderer.settings().cascade_count;
	const auto readAll = [&renderer, cascadeCount]() -> std::optional<std::vector<std::vector<Vec4>>> {
		auto atlases = std::vector<std::vector<Vec4>>{};
		for (std::uint32_t cascade{}; cascade < cascadeCount; ++cascade) {
			auto atlas = renderer.readCascadeAtlas(cascade);
			if (!atlas) { return std::nullopt; }
			atlases.push_back(std::move(*atlas));
		}
		return atlases;
	};
	// L = L_near + T_near * L_far, T = T_near * T_far; far interpolates the 8 surrounding next-cascade probes
	// trilinearly (clamped to the far grid) and averages the scale x scale child directions of the texel.
	const auto referenceMerge = [&renderer](std::uint32_t cascade, const std::vector<Vec4> &near, const std::vector<Vec4> &far) {
		const auto size = renderer.cascadeAtlasSize(cascade), farSize = renderer.cascadeAtlasSize(cascade + 1U);
		const std::uint32_t side = renderer.directionSide(cascade), farSide = renderer.directionSide(cascade + 1U), scale = farSide / side;
		const float ratio = renderer.probeSpacing(cascade) / renderer.probeSpacing(cascade + 1U);
		const std::array<int, 3> farCount{static_cast<int>(farSize[0] / farSide), static_cast<int>(farSize[1] / farSide), static_cast<int>(farSize[2])};
		auto result = std::vector<Vec4>(near.size());
		for (std::uint32_t layer{}; layer < size[2]; ++layer) {
			for (std::uint32_t y{}; y < size[1]; ++y) {
				for (std::uint32_t x{}; x < size[0]; ++x) {
					const std::array<float, 3> position{static_cast<float>(x / side) * ratio, static_cast<float>(y / side) * ratio, static_cast<float>(layer) * ratio};
					std::array<float, 4> sum{};
					for (int corner{}; corner < 8; ++corner) {
						float weight{1.0F};
						std::array<std::uint32_t, 3> probe{};
						for (int axis{}; axis < 3; ++axis) {
							const int offset{(corner >> axis) & 1};
							const float base = std::floor(position[axis]), fraction = position[axis] - base;
							weight *= offset != 0 ? fraction : 1.0F - fraction;
							probe[axis] = static_cast<std::uint32_t>(std::clamp(static_cast<int>(base) + offset, 0, farCount[axis] - 1));
						}
						if (weight == 0.0F) { continue; }
						for (std::uint32_t k{}; k < scale; ++k) {
							for (std::uint32_t l{}; l < scale; ++l) {
								const std::size_t fx{probe[0] * farSide + (x % side) * scale + k}, fy{probe[1] * farSide + (y % side) * scale + l};
								const Vec4 value = far[fx + std::size_t{farSize[0]} * (fy + std::size_t{farSize[1]} * probe[2])];
								for (int channel{}; channel < 4; ++channel) { sum[static_cast<std::size_t>(channel)] += weight * value[channel]; }
							}
						}
					}
					const std::size_t index{x + std::size_t{size[0]} * (y + std::size_t{size[1]} * layer)};
					const Vec4 n = near[index];
					const float count = static_cast<float>(scale * scale);
					result[index] = Vec4{n.x + n.w * sum[0] / count, n.y + n.w * sum[1] / count, n.z + n.w * sum[2] / count, n.w * sum[3] / count};
				}
			}
		}
		return result;
	};
	const auto raw = readAll();
	renderer.setMerging(true);
	if (!raw || !renderer.renderFrame()) { return 70; }
	const auto merged = readAll();
	const auto sameTexel = [](const Vec4 &a, const Vec4 &b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; };
	if (!merged || !std::ranges::equal(merged->back(), raw->back(), sameTexel)) { return 71; }
	float mergeError{};
	for (std::uint32_t cascade{}; cascade + 1U < cascadeCount; ++cascade) {
		const auto expected = referenceMerge(cascade, (*raw)[cascade], (*merged)[cascade + 1U]);
		for (std::size_t texel{}; texel < expected.size(); ++texel) {
			for (int channel{}; channel < 4; ++channel) {
				const float reference = expected[texel][channel];
				mergeError = std::max(mergeError, std::abs((*merged)[cascade][texel][channel] - reference) / (1.0F + std::abs(reference)));
			}
		}
	}
	if (mergeError > 2e-3F) { return 72; }

	// 10b. Sky only: with a grey plane that emits nothing, the sky is the only light, so every merged texel of every cascade holds the
	// sky colour (the background, linear 0.2, 0.5, 1) times its merged transparency. Cascade 0 must have texels that
	// see all, none and part of the sky.
	RenderScene dark{}; // Outlives every use by the renderer, which is shut down below.
	const auto grey = dark.addMaterial(RenderMaterial{.base_color = {.value = {0.5F, 0.5F, 0.5F}}});
	(void)dark.addInstance(dark.addPlaneMesh(Vec2{0.9F, 0.9F}), grey, {}, math::translate(identityMat4(), Vec3{0.0F, 0.1F, 0.0F}));
	renderer.bindRenderScene(dark);
	if (!renderer.rebuildScene() || !renderer.renderFrame()) { return 73; }
	const auto skyLit = readAll();
	if (!skyLit) { return 74; }
	const std::array sky{0.2F, 0.5F, 1.0F};
	float skyError{};
	std::array<std::size_t, 3> skySeen{}; // Cascade 0 texels that see all, none and part of the sky.
	for (std::uint32_t cascade{}; cascade < cascadeCount; ++cascade) {
		for (const Vec4 &texel : (*skyLit)[cascade]) {
			for (std::size_t channel{}; channel < 3U; ++channel) {
				skyError = std::max(skyError, std::abs(texel[static_cast<int>(channel)] - sky[channel] * texel.w));
			}
			if (cascade == 0U) { ++skySeen[texel.w > 0.999F ? 0U : texel.w < 0.001F ? 1U : 2U]; }
		}
	}
	if (skyError > 2e-3F || skySeen[0] == 0U || skySeen[1] == 0U || skySeen[2] == 0U) { return 75; }
	renderer.shutdown();
	errors += renderer.validationErrorCount();
	std::println("[RadianceCascadeRendererTests] intervals hits={},{} misses={},{} mismatches={},{} skipped={},{} atlas_view emission={} background={}",
		intervalHits[0], intervalHits[1], intervalMisses[0], intervalMisses[1], intervalMismatches[0], intervalMismatches[1],
		intervalSkipped[0], intervalSkipped[1], emissionPixels, backgroundPixels);
	std::println("[RadianceCascadeRendererTests] merge max_error={} sky max_error={} cascade0_sky all={} none={} part={}",
		mergeError, skyError, skySeen[0], skySeen[1], skySeen[2]);

	std::println("[RadianceCascadeRendererTests] wrong_pixels={} resize_requested={} extent={}x{} resized_wrong_pixels={} validation_active={} validation_errors={}",
		wrong, resized, extent.width, extent.height, resized_wrong, validation, errors);
	std::println("[RadianceCascadeRendererTests] scene centre={},{},{} bottom={},{},{} corner={},{},{} scene_ok={}",
		centre[0], centre[1], centre[2], bottom[0], bottom[1], bottom[2], corner[0], corner[1], corner[2], scene_ok);
	std::println("[RadianceCascadeRendererTests] voxel_view magenta_pixels={} red_green_mixtures={} other_differences={}",
		magenta, voxel_differences - unexplained_differences, unexplained_differences);
	const bool intervals_ok = intervalMismatches[0] == 0U && intervalMismatches[1] == 0U;
	return wrong == 0U && resized_wrong == 0U && scene_ok && magenta == 0U && unexplained_differences == 0U && intervals_ok && errors == 0U ? 0 : 30;
}
