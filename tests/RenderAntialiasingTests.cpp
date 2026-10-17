/** @file @brief Checks real coverage resolves, two windows, GUI, post-processing and resized MSAA lifetimes. */
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>

import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {

/// @brief Independently queries the actual HDR/depth format capabilities, including framebuffer limits.
[[nodiscard]] bool supportsFourSamples(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device, &properties);
	VkImageFormatProperties colour{}, depth{};
	const auto c = vkGetPhysicalDeviceImageFormatProperties(device, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0U, &colour);
	const auto d = vkGetPhysicalDeviceImageFormatProperties(device, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TYPE_2D,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0U, &depth);
	return c == VK_SUCCESS && d == VK_SUCCESS && (colour.sampleCounts & depth.sampleCounts &
		properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0U;
}

/// @brief Counts softened green-channel edges; red gain and the GUI marker verify independent downstream passes.
[[nodiscard]] auto capture(vve::simple::RenderSystem &render, vve::simple::WindowTarget &target,
	const std::filesystem::path &path) -> std::optional<std::size_t> {
	if (!render.captureFrameToPng(target.handle, path)) { return {}; }
	const auto pixels = vve::test::imagePixels(path);
	const auto width = target.swapchain.extent.width, height = target.swapchain.extent.height;
	if (!pixels || pixels->size() != width * height * 4U) { return {}; }
	const auto *centre = vve::test::pixelAt(pixels->data(), width, width / 2U, height / 2U);
	if (std::abs(static_cast<int>(centre[0]) - 137) > 3 || centre[1] < 250 || centre[2] < 250) { return {}; }
	const auto *corner = vve::test::pixelAt(pixels->data(), width, 4, 4);
	const int grey = render.forward().device.swapchainMutableFormat ? 128 : 188;
	if (target.guiWindow ? std::abs(static_cast<int>(corner[1]) - grey) > 3 : corner[1] > 3) { return {}; }
	std::size_t edges{};
	// Exclude the GUI marker; intermediate green can only come from triangle edge coverage.
	for (auto pixel : std::views::iota(static_cast<std::size_t>(16U * width), pixels->size() / 4U)) {
		const auto offset = pixel * 4U;
		if ((*pixels)[offset + 1U] > 8U && (*pixels)[offset + 1U] < 247U) { ++edges; }
	}
	std::println("[RenderAntialiasingTests] capture={} size={}x{} centre={},{},{} edges={}",
		path.filename().string(), width, height, centre[0], centre[1], centre[2], edges);
	return edges;
}

/// @brief Runs one retained startup selection through frame slots, resize, capture and complete teardown.
[[nodiscard]] auto checkMode(vve::Antialiasing mode, bool &supports_msaa) -> std::optional<std::size_t> {
	using namespace vve;
	auto engine = simple::Engine{simple::Windows{.value = {
		WindowDesc{.id = "a", .extent = {128, 128}, .visible = false},
		WindowDesc{.id = "b", .extent = {128, 128}, .visible = false}}}};
	if (!engine.init()) { return {}; }
	auto &render = engine.renderSystem();
	if (!render.setAntialiasing(mode) || render.antialiasing() != mode) { return {}; }
	render.setBackgroundColor(LinearColor{.value = Vec3{0, 0, 0}});
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 0, 4}}, Position{}));
	const auto triangle = render.addMesh(MeshDescriptor{.positions = {{-1.25F, -1.1F, 0}, {1.3F, -1, 0}, {0.1F, 1.2F, 0}}, .indices = {0, 1, 2}});
	if (!triangle || !render.setObjectUnlit(*triangle, true)) { return {}; }
	render.setDirectionalLight(Direction{}, LinearColor{}, LightIntensity{}, LinearColor{});
	render.setPostProcessSetup([](vvppl::PostProcessing &chain) { chain.addColorGrade().gain[0] = 0.25F; });
	engine.gui().draw([] { ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, {12, 12}, IM_COL32(128, 128, 128, 255)); });
	const auto first = engine.renderFrame();
	if (mode == Antialiasing::msaa4 && !supports_msaa) {
		std::println("[RenderAntialiasingTests] unsupported_msaa_error={}", first ? "none" : errorName(first.error()));
		return !first && first.error() == Error::platform_error ? std::optional<std::size_t>{0U} : std::nullopt;
	}
	if (!first || render.lastRenderedWindowCount() != 2U || !engine.gui().ready()) { return {}; }
	auto &renderer = render.forward();
	if (mode == Antialiasing::none) { supports_msaa = supportsFourSamples(renderer.physicalDevice.physicalDevice); }
	const auto locked = render.setAntialiasing(Antialiasing::none);
	if (locked || locked.error() != Error::already_initialized) { return {}; }
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	std::size_t edges{};
	// Recreate only one window first, then resize it; every target must retain matching attachments.
	for (const auto stage : {0, 1, 2}) {
		auto &target = renderer.targets.front();
		if (stage == 1 && renderer.recreateSwapchain(target, target.swapchain.extent) != VK_SUCCESS) { return {}; }
		if (stage == 2 && (!SDL_SetWindowSize(target.window, 160, 96) || !SDL_SyncWindow(target.window))) { return {}; }
		if (!engine.renderFrame() || !engine.renderFrame()) { return {}; }
		for (auto &window : renderer.targets) {
			const auto sample = mode == Antialiasing::msaa4 ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
			if (window.depthImage.samples != sample || window.hdrImage.samples != VK_SAMPLE_COUNT_1_BIT ||
				(window.msaaImage.image != VK_NULL_HANDLE) != (mode == Antialiasing::msaa4) ||
				(mode == Antialiasing::msaa4 && window.msaaImage.samples != sample)) { return {}; }
			const auto count = capture(render, window, directory / std::format("{}-{}-{}.png", static_cast<int>(mode), stage, window.handle.value));
			if (!count || (mode == Antialiasing::none ? *count != 0U : *count < 20U)) { return {}; }
			edges += *count;
		}
	}
	if (renderer.forwardPipelineCreateCount() != 1U || renderer.dirShadowArray.samples != VK_SAMPLE_COUNT_1_BIT) { return {}; }
	std::println("[RenderAntialiasingTests] validation_active={}", renderer.validationActive());
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderAntialiasingTests] mode={} supports4x={} total_edges={} validation_errors={}",
		static_cast<int>(mode), supports_msaa, edges, renderer.validationErrorCount());
	return renderer.validationErrorCount() == 0U ? std::optional{edges} : std::nullopt;
}

} // namespace

/// @brief Verifies the public forwarding contract and visible multisample coverage against the unchanged default.
int main() {
	using namespace vve;
	Engine<> facade;
	auto world = facade.world();
	auto &render = world.get<RenderSystem>();
	const auto invalid = render.setAntialiasing(static_cast<Antialiasing>(99));
	if (render.antialiasing() != Antialiasing::none || invalid || invalid.error() != Error::invalid_argument ||
		!render.setAntialiasing(Antialiasing::msaa4) || render.antialiasing() != Antialiasing::msaa4) { return 1; }
	bool supported{};
	const auto single = checkMode(Antialiasing::none, supported);
	const auto multi = checkMode(Antialiasing::msaa4, supported);
	return single && multi && (!supported || *multi > *single) ? 0 : 2;
}
