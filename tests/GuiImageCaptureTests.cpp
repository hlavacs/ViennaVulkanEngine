/**
 * @file
 * @brief Captures a HUD image drawn by ImGui over the whole window and checks its colour.
 *
 * HUD images reach the GUI pass as raw bytes, like ImGui's own colours: on the mutable-format path the capture shows
 * the file's bytes, on the sRGB fallback they are encoded once more, as GuiColorCaptureTests describes.
 */
#include <imgui.h>

import std;
import VVE.TestSupport;
import VVEngine.Simple;

/// @brief Loads a solid colour image before initialization and draws it once ImGui has its id.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto image = directory / "hud.ppm";
	{
		std::ofstream file{image, std::ios::binary};
		file << "P6\n4 4\n255\n";
		for ([[maybe_unused]] const auto texel : std::views::iota(0, 16)) { file << "\x80\x40\xc8"; }
		if (!file) { return 1; }
	}
	constexpr int extent = 64;
	auto engine = test::hiddenEngine("gui-image-capture-tests", PixelExtent{extent, extent});
	if (!engine.init()) { return 2; }
	const auto hud = engine.gui().loadImage(image);
	if (!hud) { return 3; }
	std::uint64_t drawn_id{};
	engine.gui().draw([&] {
		drawn_id = engine.gui().imageId(*hud);
		if (drawn_id == 0U) { return; }
		const auto *viewport = ImGui::GetMainViewport();
		ImGui::GetBackgroundDrawList()->AddImage(static_cast<ImTextureID>(drawn_id), viewport->Pos,
			ImVec2{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y});
	});
	const auto capture = directory / "hud.png";
	// The image uploads in the first frame and is registered before that frame's GUI callback.
	if (!engine.renderFrame() || drawn_id == 0U || !engine.renderSystem().captureFrameToPng(capture)) { return 4; }
	const auto pixels = test::imagePixels(capture);
	if (!pixels || pixels->size() != extent * extent * 4U) { return 5; }
	const auto *pixel = test::pixelAt(pixels->data(), extent, extent / 2, extent / 2);
	auto &renderer = engine.renderSystem().forward();
	const bool mutable_format = renderer.device.swapchainMutableFormat;
	// The sRGB fallback encodes the raw bytes once more.
	const auto encode = [](int value) {
		const double linear = value / 255.0;
		return static_cast<int>(std::lround(255.0 * (linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055)));
	};
	const std::array expected{mutable_format ? 128 : encode(128), mutable_format ? 64 : encode(64), mutable_format ? 200 : encode(200)};
	const bool ok = std::ranges::all_of(std::views::iota(0, 3), [&](int channel) { return std::abs(pixel[channel] - expected[channel]) <= 3; });
	engine.renderSystem().waitIdle();
	engine.gui().shutdownVulkan();
	engine.renderSystem().shutdown();
	std::println("[GuiImageCaptureTests] path={} rgb={},{},{} expected={},{},{} id_after_shutdown={} validation_errors={}",
		mutable_format ? "mutable-format" : "theme-only-fallback", pixel[0], pixel[1], pixel[2], expected[0], expected[1], expected[2],
		engine.gui().imageId(*hud), renderer.validationErrorCount());
	return ok && engine.gui().imageId(*hud) == 0U && renderer.validationErrorCount() == 0U ? 0 : 6;
}
