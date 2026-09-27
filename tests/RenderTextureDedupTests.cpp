#include <vulkan/vulkan_core.h>

/**
 * @file
 * @brief RenderScene and Vulkan texture-table deduplication coverage.
 *
 * Functional objects:
 * - main verifies canonical deduplication, removal and clear lifetime, and backend-cap fallback.
 */

import std;

import VEEngine;
import VEEngine.Simple;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;

/// @brief Verifies canonical paths share one decoded CPU entry and one stable GPU image.
int main() {
	auto engine = vve::simple::Engine{
		vve::ApplicationName{"render-texture-dedup-tests"},
		vve::WindowSetups{vve::WindowSetup{}
			.id("main")
			.title("render-texture-dedup-tests")
			.extent(vve::PixelExtent{.width = 64, .height = 64})
			.renderer(vve::RendererId{.value = "forward"})
			.visible(false)}};
	if (!engine.init()) { return 1; }

	auto &render = engine.renderSystem();
	render.clearScene();
	const auto texture_a = std::filesystem::path{VVE_TEST_TEXTURE_A};
	const auto texture_b = std::filesystem::path{VVE_TEST_TEXTURE_B};
	std::error_code error{};
	const auto relative_a = std::filesystem::relative(texture_a, std::filesystem::current_path(), error);
	if (error || relative_a.empty()) { return 2; }
	const auto alternate_a = relative_a.parent_path() / ".." / relative_a.parent_path().filename() /
		relative_a.filename();

	const auto minimum = vve::Vec3{-0.5F, -0.5F, -0.5F};
	const auto maximum = vve::Vec3{0.5F, 0.5F, 0.5F};
	auto first_object = vve::RenderObjectHandle{};
	for (const auto &path : std::array{relative_a, alternate_a, texture_a}) {
		const auto object = render.addTexturedCuboid(minimum, maximum, path);
		if (!object) { return 3; }
		if (!first_object.valid()) { first_object = *object; }
	}
	if (render.sceneTextureCount() != 1U || render.gpuTextureCount() != 0U) { return 4; }
	if (!engine.renderFrame() || render.sceneTextureCount() != 1U || render.gpuTextureCount() != 1U) { return 5; }

	const auto first_image = render.forward().objectTextures[0].image;
	const auto texture_b_object = render.addTexturedCuboid(minimum, maximum, texture_b);
	if (first_image == VK_NULL_HANDLE || !texture_b_object ||
		render.sceneTextureCount() != 2U || render.gpuTextureCount() != 1U) {
		return 6;
	}
	if (!engine.renderFrame() || render.gpuTextureCount() != 2U ||
		render.forward().objectTextures[0].image != first_image) {
		return 7;
	}
	const auto second_image = render.forward().objectTextures[1].image;
	const auto repeated_object = render.addTexturedCuboid(minimum, maximum, alternate_a);
	if (second_image == VK_NULL_HANDLE || !repeated_object ||
		render.sceneTextureCount() != 2U) {
		return 8;
	}
	if (!engine.renderFrame() || render.gpuTextureCount() != 2U ||
		render.forward().objectTextures[0].image != first_image ||
		render.forward().objectTextures[1].image != second_image) {
		return 9;
	}

	std::cout << "dedup sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';
	if (!render.removeObject(first_object) || !engine.renderFrame() ||
		render.sceneTextureCount() != 2U || render.gpuTextureCount() != 2U ||
		render.forward().objectTextures[0].image == VK_NULL_HANDLE ||
		render.forward().objectTextures[1].image == VK_NULL_HANDLE ||
		!std::ranges::all_of(render.renderMaterials(), [](const auto &material) {
			return material.base_color_texture_index != vve::simple::kNoRenderTexture;
		})) {
		return 10;
	}
	std::cout << "remove sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';

	render.clearScene();
	const auto replacement = render.addTexturedCuboid(minimum, maximum, texture_b);
	if (!replacement || !engine.renderFrame() || render.sceneTextureCount() != 1U ||
		render.gpuTextureCount() != 1U || render.forward().objectTextures[0].image == VK_NULL_HANDLE ||
		render.sceneInstanceCount() != 1U ||
		render.renderMaterials().front().base_color_texture_index != 0U) {
		return 11;
	}
	std::cout << "clear sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';

	render.clearScene();
	const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto cap_directory = std::filesystem::temp_directory_path() /
		("vve-render-texture-cap-" + unique_suffix);
	if (!std::filesystem::create_directories(cap_directory, error) || error) { return 12; }
	for (std::size_t index{}; index < vve::simple::kMaxSceneTextures + 1U; ++index) {
		const auto path = cap_directory / ("texture-" + std::to_string(index) + ".ppm");
		auto output = std::ofstream{path, std::ios::binary};
		const auto pixels = std::array<char, 12U>{
			static_cast<char>(index), static_cast<char>(index + 1U), static_cast<char>(index + 2U),
			static_cast<char>(index + 3U), static_cast<char>(index + 4U), static_cast<char>(index + 5U),
			static_cast<char>(index + 6U), static_cast<char>(index + 7U), static_cast<char>(index + 8U),
			static_cast<char>(index + 9U), static_cast<char>(index + 10U), static_cast<char>(index + 11U)};
		output << "P6\n2 2\n255\n";
		output.write(pixels.data(), static_cast<std::streamsize>(pixels.size()));
		output.close();
		if (!output || !render.addTexturedCuboid(minimum, maximum, path)) {
			std::filesystem::remove_all(cap_directory, error);
			return 13;
		}
	}
	if (render.sceneTextureCount() != vve::simple::kMaxSceneTextures + 1U ||
		render.renderMaterials().back().base_color_texture_index < vve::simple::kMaxSceneTextures ||
		!engine.renderFrame() || render.gpuTextureCount() != vve::simple::kMaxSceneTextures) {
		std::filesystem::remove_all(cap_directory, error);
		return 14;
	}
	std::cout << "cap sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';
	std::filesystem::remove_all(cap_directory, error);
	if (error) { return 15; }
	return 0;
}
