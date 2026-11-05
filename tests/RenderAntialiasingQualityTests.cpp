/** @file @brief Independent polygon-area MSAA oracle for engine and native geometry on all three backends. */
#include <SDL3/SDL_video.h>
#include <stb_image_write.h>
#include <vulkan/vulkan.h>
#include "vertexMain.h"
#include "plainMain.h"
#include "unlitMain.h"
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {
using namespace vve;
using Point = std::array<double, 2>; ///< Pixel-space coordinates; the oracle uses double precision independently of shaders.
constexpr unsigned side = 128; ///< Fixed square viewport, without GUI, sky, fog, animation or post-processing.
constexpr Vec3 colour{.2F, .55F, .9F}; ///< Non-binary linear RGB exercises HDR storage as well as coverage.
constexpr std::array<Point, 4> corners{{{30, 25}, {101, 44}, {84, 102}, {13, 83}}}; ///< Tilted parallelogram in drawable pixels.
constexpr std::array<Point, 4> offsets{{{0, 0}, {.25, .375}, {.5, .75}, {.875, .125}}}; ///< Several subpixel phases, fixed before measurement.
// Frozen IQ-01 limits: four coverage samples must remove at least 35% of point-sampling error.
constexpr double maxEdgeRatio = .65, maxEdgeError = .14; ///< Aggregate reduction and absolute mean normalized coverage error.
constexpr double minAliasedEdgeError = .1; ///< The unfiltered fixture must contain substantial error, beyond storage jitter.
constexpr double maxInteriorError = .001; ///< Two FP16 stores below one cost less than 0.001 linear RGB.
constexpr double maxCoverageError = .005; ///< 0.5% area allowance for sample quadrature and FP16, not missing geometry.
constexpr unsigned minEdges = 150, minInterior = 2000; ///< Independently sized masks cannot pass empty or with a lost feature.
const std::filesystem::path output{VVE_AA_QUALITY_DIR}; ///< Transient evidence under the build tree, never baseline replacements.

/// @brief Fail with a searchable receipt; never interpret missing graphics or readback as a skip.
void require(bool success, std::string_view message) {
	if (!success) { throw std::runtime_error{std::string{message}}; }
}
/// @brief Convert Vulkan failure into the native callback's existing platform_error contract.
void requireVk(VkResult result) { if (result != VK_SUCCESS) { throw Error::platform_error; } }

/// @brief Shift the explicit fixture geometry without consulting any rendered pixels.
auto polygon(Point offset) {
	auto points = corners;
	for (auto &p : points) { p[0] += offset[0]; p[1] += offset[1]; }
	return points;
}
/// @brief Exact area of a convex polygon clipped against this pixel's four half-planes (Sutherland-Hodgman).
double coverage(unsigned x, unsigned y, Point offset) {
	const auto points = polygon(offset);
	std::vector<Point> clipped(points.begin(), points.end());
	// Intersect each boundary analytically; no hardware sample positions or GPU image enter the reference.
	for (const auto boundary : std::views::iota(0U, 4U)) {
		const auto axis = boundary / 2;
		const double bound = (axis == 0 ? x : y) + (boundary % 2);
		const auto inside = [&](Point p) { return boundary % 2 ? p[axis] <= bound : p[axis] >= bound; };
		std::vector<Point> next;
		if (clipped.empty()) { return 0; }
		auto a = clipped.back();
		for (const auto b : clipped) {
			if (inside(a) != inside(b)) {
				const double t = (bound - a[axis]) / (b[axis] - a[axis]);
				next.push_back({a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])});
			}
			if (inside(b)) { next.push_back(b); }
			a = b;
		}
		clipped = std::move(next);
	}
	double twiceArea{};
	if (!clipped.empty()) {
		auto a = clipped.back();
		for (const auto b : clipped) { twiceArea += a[0] * b[1] - a[1] * b[0]; a = b; }
	}
	return std::clamp(std::abs(twiceArea) * .5, 0.0, 1.0);
}
/// @brief Interior mask stays 1.5 pixels from each infinite fixture edge, independent of coverage/output differences.
bool interior(unsigned x, unsigned y, Point offset) {
	const auto p = polygon(offset);
	for (const auto i : std::views::iota(0U, 4U)) {
		const auto a = p[i], b = p[(i + 1) % 4];
		const double dx = b[0] - a[0], dy = b[1] - a[1];
		if ((dx * (y + .5 - a[1]) - dy * (x + .5 - a[0])) / std::hypot(dx, dy) <= 1.5) { return false; }
	}
	return true;
}

/// @brief Shader constants share the existing native_procedural.slang layout, without an engine material/uniform ABI.
struct Push {
	Mat4 clip{}; ///< Pixel-space quad mapped directly into Vulkan clip coordinates.
	Vec4 low{0, 0, 0, 0}, high{1, 1, 0, 0}; ///< Unit square in the existing shader's front box face.
	Vec4 color{colour, 1}, eye{}; ///< Unlit linear RGB; unused capture constants remain zero.
};
static_assert(sizeof(Push) == 128);
/// @brief Callback-owned pipeline; VVE retires the owner after its last submission fence, before device destruction.
struct NativeQuad {
	VkDevice device{}; ///< Borrowed device, valid throughout callback ownership.
	VkPipelineLayout layout{}; VkPipeline pipeline{}; ///< One pipeline for this engine's startup pass/coverage selection.
	Point offset{}; ///< Updated only between ordinary frame calls; draws copy it into push constants.
	unsigned draws{}, samples{}, attachments{}; ///< Actual callback receipts, checked against the selected backend.
	~NativeQuad();
	void create(const NativeDrawContext &);
	auto record(const NativeDrawContext &) -> std::expected<void, Error>;
};
/// @brief Release native owners while the fenced engine device remains valid.
NativeQuad::~NativeQuad() {
	if (device) { vkDestroyPipeline(device, pipeline, nullptr); vkDestroyPipelineLayout(device, layout, nullptr); }
}
/// @brief Build from the public samples, attachment count and formats, reusing the native test shader build rules.
void NativeQuad::create(const NativeDrawContext &c) {
	device = reinterpret_cast<VkDevice>(c.device);
	samples = c.samples; attachments = c.colorAttachmentCount;
	const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push)};
	const VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1, .pPushConstantRanges = &push};
	requireVk(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout));
	std::array<VkShaderModule, 2> modules{};
	const auto shader = [&](std::span<const std::uint32_t> code, VkShaderModule &module) {
		const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = code.size_bytes(), .pCode = code.data()};
		requireVk(vkCreateShaderModule(device, &info, nullptr, &module));
	};
	try {
		shader(native_vertexMain, modules[0]);
		shader(c.pass == NativeDrawPass::gbuffer ? std::span<const std::uint32_t>{native_unlitMain} : std::span<const std::uint32_t>{native_plainMain}, modules[1]);
		const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = modules[0], .pName = "vertexMain"},
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = modules[1], .pName = c.pass == NativeDrawPass::gbuffer ? "unlitMain" : "plainMain"}}};
		const VkPipelineVertexInputStateCreateInfo vertices{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		const VkPipelineInputAssemblyStateCreateInfo assembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
		const VkPipelineViewportStateCreateInfo viewport{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
		const VkPipelineRasterizationStateCreateInfo raster{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1};
		const VkPipelineMultisampleStateCreateInfo multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VkSampleCountFlagBits(c.samples)};
		const VkPipelineDepthStencilStateCreateInfo depth{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS};
		std::array<VkPipelineColorBlendAttachmentState, 3> colors{};
		for (auto &a : colors) { a.colorWriteMask = 15; }
		const VkPipelineColorBlendStateCreateInfo blend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = c.colorAttachmentCount, .pAttachments = colors.data()};
		const std::array states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		const VkPipelineDynamicStateCreateInfo dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = states.data()};
		std::array<VkFormat, 3> formats{};
		std::ranges::transform(c.colorFormats, formats.begin(), [](auto f) { return VkFormat(f); });
		const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = c.colorAttachmentCount, .pColorAttachmentFormats = formats.data(), .depthAttachmentFormat = VkFormat(c.depthFormat)};
		const VkGraphicsPipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &rendering,
			.stageCount = 2, .pStages = stages.data(), .pVertexInputState = &vertices, .pInputAssemblyState = &assembly, .pViewportState = &viewport,
			.pRasterizationState = &raster, .pMultisampleState = &multisample, .pDepthStencilState = &depth, .pColorBlendState = &blend,
			.pDynamicState = &dynamic, .layout = layout};
		requireVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
	} catch (...) { for (auto m : modules) { vkDestroyShaderModule(device, m, nullptr); } throw; }
	for (auto m : modules) { vkDestroyShaderModule(device, m, nullptr); }
}
/// @brief Draw only the main opaque pass; G-buffer and forward use identical coverage and linear unlit RGB.
auto NativeQuad::record(const NativeDrawContext &c) -> std::expected<void, Error> try {
	if (c.prepare || (c.pass != NativeDrawPass::forward_opaque && c.pass != NativeDrawPass::gbuffer)) { return {}; }
	if (!device) { create(c); }
	const auto p = polygon(offset);
	Push push;
	// Unit-square axes reproduce the explicit pixel-space parallelogram without using engine camera matrices.
	push.clip[0] = Vec4{float(2 * (p[1][0] - p[0][0]) / side), float(2 * (p[1][1] - p[0][1]) / side), 0, 0};
	push.clip[1] = Vec4{float(2 * (p[3][0] - p[0][0]) / side), float(2 * (p[3][1] - p[0][1]) / side), 0, 0};
	push.clip[3] = Vec4{float(2 * p[0][0] / side - 1), float(2 * p[0][1] / side - 1), .5F, 1};
	const auto cmd = reinterpret_cast<VkCommandBuffer>(c.commandBuffer);
	const VkViewport viewport{float(c.viewportX), float(c.viewportY), float(c.width), float(c.height), 0, 1};
	const VkRect2D scissor{{int(c.viewportX), int(c.viewportY)}, {c.width, c.height}};
	vkCmdSetViewport(cmd, 0, 1, &viewport); vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
	vkCmdDraw(cmd, 6, 1, 24, 0); ++draws; // Existing native shader's +Z face, with zero thickness.
	return {};
} catch (...) { return std::unexpected(Error::platform_error); }

/// @brief Require a live validation messenger in Debug and explicit synchronization validation in CTest's environment.
bool validation(const simple::ForwardRenderer &r) {
	const auto *sync = std::getenv("VK_LAYER_VALIDATE_SYNC"), *layers = std::getenv("VK_INSTANCE_LAYERS");
	const bool requested = sync && std::string_view{sync} == "1" && layers && std::string_view{layers}.contains("VK_LAYER_KHRONOS_validation");
#ifndef NDEBUG
	const bool active = requested && r.validationActive(); // Device.ixx enables validate_sync using layer_settings/validation_features.
#else
	const bool active = requested; // Release requires the external layer and CTest's validation-error expression.
#endif
	std::println("validation.active={} sync.requested={} sync.configuration=Device.ixx+VK_LAYER_VALIDATE_SYNC", active, requested);
	return active;
}
/// @brief Verify presentation and record environment/device in the same process that creates and runs each fixture.
void environment(simple::ForwardRenderer &r) {
	for (const auto *name : {"DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE", "SDL_VIDEODRIVER", "SDL_VIDEO_DRIVER"}) {
		std::println("{}={}", name, std::getenv(name) ? std::getenv(name) : "<unset>");
	}
	auto &target = r.targets.front();
	VkBool32 present{};
	requireVk(vkGetPhysicalDeviceSurfaceSupportKHR(r.physicalDevice.physicalDevice, target.presentQueueFamily, target.surface.surface, &present));
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(r.physicalDevice.physicalDevice, &properties);
	std::println("SDL.backend={} presentation={} device={} driver={} software_renderer={} framebuffer.colour_samples={} depth_samples={}",
		SDL_GetCurrentVideoDriver(), present, std::string_view{properties.deviceName}, properties.driverVersion,
		properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU, properties.limits.framebufferColorSampleCounts, properties.limits.framebufferDepthSampleCounts);
	require(present && target.swapchain.extent.width == side && target.swapchain.extent.height == side && validation(r), "presentation/extent/validation unavailable");
}

/// @brief Measure fixed geometry masks; write a single contact sheet containing every candidate, reference, difference and mask.
auto measure(std::span<const Vec4> pixels, Point offset, unsigned row, std::vector<unsigned char> &sheet) {
	require(pixels.size() == side * side, "missing HDR pixels");
	double edgeError{}, area{}, actualArea{}, interiorError{};
	unsigned edges{}, interiors{};
	// The reference is integrated in linear light; display encoding is used only to visualize the evidence PNG.
	for (auto y : std::views::iota(0U, side)) for (auto x : std::views::iota(0U, side)) {
		const double expected = coverage(x, y, offset);
		const auto value = pixels[y * side + x];
		for (auto c : std::views::iota(0U, 4U)) { require(std::isfinite(value[c]), "non-finite HDR component"); }
		require(std::abs(value.w - 1) <= maxInteriorError, "HDR alpha changed");
		const double actual = value.x / colour.x;
		const bool edge = expected > 1e-9 && expected < 1 - 1e-9, inside = interior(x, y, offset);
		if (edge) { edgeError += std::abs(actual - expected); ++edges; }
		if (inside) {
			++interiors;
			for (auto c : std::views::iota(0U, 3U)) { interiorError = std::max(interiorError, std::abs(double(value[c] - colour[c]))); }
		}
		area += expected; actualArea += actual;
		// Four columns: rendered linear HDR encoded for display, analytic reference, 8x coverage error, geometry masks.
		for (auto column : std::views::iota(0U, 4U)) for (auto c : std::views::iota(0U, 3U)) {
			double v = column == 0 ? value[c] : column == 1 ? expected * colour[c] : column == 2 ? 8 * std::abs(actual - expected) : (edge ? (c == 0 ? 1 : 0) : inside ? (c == 1 ? 1 : 0) : 0);
			if (column < 2) { v = v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - .055; }
			sheet[((row * side + y) * side * 4 + column * side + x) * 3 + c] = static_cast<unsigned char>(std::lround(255 * std::clamp(v, 0.0, 1.0)));
		}
	}
	const double massError = std::abs(actualArea - area) / area;
	std::println("offset={},{} edges={} interior={} reference.area={} measured.area={} coverage.relative_error={} interior.max_rgb={} edge.mae={}",
		offset[0], offset[1], edges, interiors, area, actualArea, massError, interiorError, edgeError / edges);
	require(edges >= minEdges && interiors >= minInterior, "geometry masks too small");
	require(std::abs(area - 4441) < 1e-7, "analytic reference area differs from determinant 71*58+19*17");
	require(interiorError <= maxInteriorError && massError <= maxCoverageError, "interior colour/feature coverage changed");
	return std::pair{edgeError, edges};
}

/// @brief Exercise all offsets in one startup selection; unsupported tests run a real Profiles-layer rejection/recovery.
double run(RendererBackend backend, Antialiasing aa, bool native, bool unsupported, std::vector<unsigned char> &sheet) {
	const bool four = aa == Antialiasing::msaa4 || aa == Antialiasing::msaa4_smaa;
	auto engine = test::hiddenEngine("msaa-linear-quality", PixelExtent{side, side});
	auto &r = engine.renderSystem(); auto &renderer = r.forward();
	require(bool(engine.init()) && bool(r.setRendererBackend(backend)) && bool(r.setAntialiasing(aa)), "startup selection failed");
	r.setBackgroundColor(LinearColor{Vec3{0}});
	r.setCamera(Camera::lookAt(Position{Vec3{0, 0, 4}}, Position{}, {}, FovY{std::numbers::pi_v<float> / 2}));
	r.clearLights(); require(bool(r.setAnimationTime(AnimationTime{})), "cannot freeze animation");
	std::shared_ptr<NativeQuad> quad;
	if (native) {
		quad = std::make_shared<NativeQuad>();
		require(bool(r.setNativeDraw([quad](const NativeDrawContext &c) { return quad->record(c); })), "native registration failed");
	}
	const auto first = engine.renderFrame();
	if (unsupported) {
		require(!first && first.error() == Error::platform_error && r.antialiasing() == aa && r.rendererBackend() == backend && r.renderedFrameCount() == 0, "unsupported 4x must preserve startup state");
		require(renderer.validationErrorCount() == 0 && !renderer.initialized(), "unsupported init leaked resources/validation errors");
		require(bool(r.setAntialiasing(Antialiasing::none)) && bool(engine.renderFrame()), "unsupported recovery with none failed");
		require(r.rendererBackend() == backend && r.antialiasing() == Antialiasing::none, "recovery changed backend/state");
		std::println("capability.backend={} rejected=platform_error state_preserved=true recovery=none", unsigned(backend));
	}
	else { require(bool(first), "render initialization failed (graphics tests do not skip)"); }
	environment(renderer);
	if (unsupported) {
		VkPhysicalDeviceProperties p{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice, &p);
		require(p.limits.framebufferColorSampleCounts == VK_SAMPLE_COUNT_1_BIT && p.limits.framebufferDepthSampleCounts == VK_SAMPLE_COUNT_1_BIT, "Profiles override not active");
	}
	double errors{}; unsigned edges{};
	RenderObjectHandle mesh{};
	for (const auto index : std::views::iota(0U, unsigned(offsets.size()))) {
		if (quad) { quad->offset = offsets[index]; r.notifyNativeDrawChanged(); }
		else {
			if (mesh.value != 0) { require(bool(r.removeObject(mesh)), "remove fixture failed"); }
			MeshDescriptor geometry{.indices = {0, 2, 1, 0, 3, 2}, .color = LinearColor{colour}};
			// A 90-degree pinhole at z=4 spans eight world units; this mapping is independent of engine matrices.
			for (const auto p : polygon(offsets[index])) { geometry.positions.push_back(Vec3{float((p[0] - side / 2) * 8 / side), float((side / 2 - p[1]) * 8 / side), 0}); }
			const auto created = r.addMesh(std::move(geometry)); require(bool(created), "mesh creation failed"); mesh = *created;
			require(bool(r.setObjectUnlit(mesh, true)), "unlit fixture failed");
		}
		// Queue ordinary frames across both fenced slots before the explicit diagnostic readback.
		for (auto frame : std::views::iota(0, 3)) { (void)frame; require(bool(engine.renderFrame()), "ordinary frame failed"); }
		const auto pixels = r.captureFrameLinear(renderer.targets.front().handle);
		require(bool(pixels), "captureFrameLinear failed");
		const auto [error, count] = measure(*pixels, offsets[index], (four ? 4 : 0) + index, sheet);
		errors += error; edges += count;
	}
	if (quad) {
		require(quad->draws >= 12 && quad->samples == (four ? 4U : 1U) && quad->attachments == (backend == RendererBackend::forward ? 1U : 3U), "native pass/sample contract failed");
		quad.reset(); // Only the fenced callback retains GPU ownership through shutdown.
	}
	const auto late = r.setAntialiasing(Antialiasing::none), invalid = r.setAntialiasing(static_cast<Antialiasing>(99));
	require(!late && late.error() == Error::already_initialized && !invalid && invalid.error() == Error::invalid_argument, "late/invalid AA error precedence changed");
	r.waitIdle(); engine.gui().shutdownVulkan(); r.shutdown();
	std::println("backend={} requested.samples={} effective.samples={} native={} validation.errors={}", unsigned(backend), four ? 4 : 1, unsupported ? 1 : four ? 4 : 1, native, renderer.validationErrorCount());
	require(renderer.validationErrorCount() == 0, "validation errors including teardown");
	return errors / edges;
}
/// @brief Quality gate shared with bypassed-AA negative control; edge counts alone cannot pass it.
bool improved(double single, double multi) { return single > minAliasedEdgeError && multi <= maxEdgeError && multi <= single * maxEdgeRatio; }
}

/// @brief All backends, both modes and both geometry owners; failures retain deterministic metrics/captures.
int main(int argc, char **argv) try {
	const bool unsupported = argc == 2 && std::string_view{argv[1]} == "--expect-unsupported";
	require(argc == 1 || unsupported, "unknown arguments");
	// Independently integrated triangles at the top corner also detect a clipping/area transcription mistake.
	require(std::abs(coverage(30, 25, {0, 0}) - 123.0 / 142) < 1e-10 &&
		std::abs(coverage(29, 25, {0, 0}) - 17.0 / 116) < 1e-10 && coverage(64, 64, {0, 0}) == 1 && coverage(0, 0, {0, 0}) == 0, "fixed analytic reference vectors failed");
	std::filesystem::create_directories(output);
	std::println("oracle=analytic-polygon-pixel-intersection edge.ratio.limit={} edge.mae.limit={} interior.limit={} coverage.limit={}", maxEdgeRatio, maxEdgeError, maxInteriorError, maxCoverageError);
	for (const auto backend : {RendererBackend::forward, RendererBackend::deferred, RendererBackend::hybrid_gi}) {
		for (const bool native : {false, true}) {
			if (unsupported && native) { continue; }
			std::vector<unsigned char> sheet(side * 4 * side * 8 * 3);
			if (unsupported) {
				// Both four-sample selections use the real Profiles-layer rejection and retain their exact startup mode.
				(void)run(backend, Antialiasing::msaa4_smaa, false, true, sheet);
			}
			const double single = unsupported ? 0 : run(backend, Antialiasing::none, native, false, sheet);
			const double multi = run(backend, Antialiasing::msaa4, native, unsupported, sheet);
			const auto path = output / std::format("{}-{}-{}.png", unsupported ? "unsupported" : "coverage", unsigned(backend), native ? "native" : "engine");
			require(stbi_write_png(path.string().c_str(), side * 4, side * 8, 3, sheet.data(), side * 4 * 3), "evidence write failed");
			if (!unsupported) {
				std::println("quality.backend={} native={} none.mae={} msaa4.mae={} ratio={} bypass_control.rejected={}", unsigned(backend), native, single, multi, multi / single, !improved(single, single));
				require(improved(single, multi) && !improved(single, single), "MSAA quality/bypassed-AA control failed");
			}
		}
	}
	return 0;
} catch (const std::exception &error) { std::println("IQ-01 FAIL: {}", error.what()); return 1; }
