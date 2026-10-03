/**
 * @file
 * @brief Captures authored ImGui colours through the complete swapchain GUI pass.
 */
#include <imgui.h>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

import std;
import VVE.TestSupport;
import VVEngine.Simple;

/// @brief Requires an opaque sRGB grey to survive GUI rendering without a second encoding.
int main() {
	auto engine = vve::test::hiddenEngine("gui-color-capture-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 1; }
	engine.gui().draw([] {
		const auto *viewport = ImGui::GetMainViewport();
		ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos,
			ImVec2{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
			IM_COL32(128, 128, 128, 255));
	});
	if (!engine.renderFrame() || !engine.gui().ready()) { return 2; }

	auto &renderer = engine.renderSystem().forward();
	// Discover support independently so forgetting to enable an available extension cannot pass as fallback.
	std::uint32_t extension_count{};
	if (vkEnumerateDeviceExtensionProperties(renderer.physicalDevice.physicalDevice, nullptr, &extension_count, nullptr) != VK_SUCCESS) { return 8; }
	auto extensions = std::vector<VkExtensionProperties>(extension_count);
	if (vkEnumerateDeviceExtensionProperties(renderer.physicalDevice.physicalDevice, nullptr, &extension_count, extensions.data()) != VK_SUCCESS) { return 8; }
	const bool mutable_format = std::ranges::any_of(extensions, [](const auto &extension) {
		return std::string_view{extension.extensionName} == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME;
	});
	if (renderer.device.swapchainMutableFormat != mutable_format) { return 8; }
	const auto scene_format = renderer.targets.front().swapchain.imageFormat;
	if (scene_format != VK_FORMAT_B8G8R8A8_SRGB && scene_format != VK_FORMAT_R8G8B8A8_SRGB) { return 9; }
	const auto gui_format = mutable_format
		? (scene_format == VK_FORMAT_B8G8R8A8_SRGB ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM) : scene_format;

	// Exercise view replacement even on drivers that ignore resizing hidden windows.
	for (const bool recreated : {false, true}) {
		if (recreated && renderer.recreateSwapchain(renderer.targets.front(), renderer.currentWindowPixelExtent(renderer.targets.front())) != VK_SUCCESS) { return 10; }
		const auto info = engine.renderSystem().makeGuiInitInfo();
		if (!info || *info->PipelineRenderingCreateInfo.pColorAttachmentFormats != gui_format ||
			renderer.targets.front().swapchain.imageFormat != scene_format || renderer.targets.front().swapchain.guiFormat != gui_format ||
			renderer.targets.front().imageViews.ownedViews.size() != renderer.targets.front().swapchain.images.size() ||
			renderer.targets.front().imageViews.ownedGuiViews.size() != (mutable_format ? renderer.targets.front().swapchain.images.size() : 0U)) { return 11; }
		if (!engine.renderFrame()) { return 2; }

		// The existing PNG path renders and copies the final GUI-composited swapchain image.
		const auto path = std::filesystem::path{VVE_TEST_TMP_DIR} / (recreated ? "gui-grey-recreated.png" : "gui-grey.png");
		std::filesystem::create_directories(path.parent_path());
		if (!engine.renderSystem().captureFrameToPng(path)) { return 3; }
		int width{}, height{}, channels{};
		auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
			stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
		if (!pixels || width != 64 || height != 64) { return 4; }
		const auto *pixel = vve::test::pixelAt(pixels.get(), width, width / 2, height / 2);
		engine.renderSystem().waitIdle();
		std::println("[GuiColorCaptureTests] path={} recreated={} rgb={},{},{} validationActive={} validationErrorCount={}",
			mutable_format ? "mutable-format" : "theme-only-fallback", recreated,
			pixel[0], pixel[1], pixel[2], renderer.validationActive(), renderer.validationErrorCount());
		if (renderer.validationErrorCount() != 0U) { return 5; }
		// The fallback cannot correct custom draw colours; its theme correction has CPU coverage.
		for (const auto channel : std::views::iota(0U, 3U)) {
			if (mutable_format && std::abs(static_cast<int>(pixel[channel]) - 128) > 2) { return 6; }
			if (!mutable_format && std::abs(static_cast<int>(pixel[channel]) - 188) > 2) { return 6; }
		}
		if (pixel[3] != 255) { return 7; }
	}

	// Keep validation observable through view and swapchain destruction as well.
	engine.gui().shutdownVulkan();
	engine.renderSystem().shutdown();
	std::println("[GuiColorCaptureTests] cleanup_validation_errors={}", renderer.validationErrorCount());
	if (!renderer.targets.empty()) { return 12; }
	return renderer.validationErrorCount() == 0U ? 0 : 5;
}
