/**
 * @file
 * @brief Checks OBJ HEIGHT-map rejection, coloured normal fallback and explicit normal-map precedence.
 * Fixtures are generated in the test directory; the engine creates no windows or Vulkan device.
 */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine.Simple;
import VVEngine.Simple.RenderResources;
import VVEngine.Simple.Scene;

namespace {

/// @brief Writes a triangle and its material so each case goes through the actual Assimp OBJ importer.
[[nodiscard]] bool writeModel(const std::filesystem::path &directory, std::string_view name, std::string_view maps) {
	auto material = std::ofstream{directory / (std::string{name} + ".mtl")};
	material << "newmtl surface\nKd 1 1 1\n" << maps;
	material.close();
	auto object = std::ofstream{directory / (std::string{name} + ".obj")};
	object << "mtllib " << name << ".mtl\no triangle\nv -1 -1 0\nv 1 -1 0\nv 0 1 0\n"
		"vt 0 0\nvt 1 0\nvt 0.5 1\nusemtl surface\nf 1/1 2/2 3/3\n";
	object.close();
	return material.good() && object.good();
}

/// @brief Loads one fixture without initializing the engine or its default window configuration.
[[nodiscard]] bool instantiate(vve::simple::Engine &engine, const std::filesystem::path &path) {
	const auto scene = engine.assets().loadScene(path);
	const bool loaded = scene && engine.renderSystem().instantiateScene(*scene).has_value();
	std::println("[RenderHeightMapTests] fixture={} instantiated={}", path.filename().string(), loaded);
	return loaded;
}

} // namespace

/// @brief Compares the actual normal texture slot and colour space for the three OBJ map combinations.
int main() {
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	// Distinct solid PPMs let the explicit normal map and coloured HEIGHT fallback have separate identities.
	const std::array names{"grey", "colour", "normal"};
	const std::array colours{std::array<unsigned char, 3>{128, 128, 128},
		std::array<unsigned char, 3>{128, 64, 255}, std::array<unsigned char, 3>{128, 128, 255}};
	for (std::size_t index{}; index < names.size(); ++index) {
		auto image = std::ofstream{directory / (std::string{names[index]} + ".ppm"), std::ios::binary};
		image << "P6\n2 2\n255\n";
		for (const auto pixel : {0, 1, 2, 3}) {
			(void)pixel;
			image.write(reinterpret_cast<const char *>(colours[index].data()), 3);
		}
		image.close();
		if (!image) { return 1; }
	}
	if (!writeModel(directory, "grey", "map_bump grey.ppm\n") ||
		!writeModel(directory, "colour", "map_bump colour.ppm\n") ||
		!writeModel(directory, "normal", "map_Kn normal.ppm\n") ||
		!writeModel(directory, "both", "map_bump colour.ppm\nmap_Kn normal.ppm\n")) { return 2; }
	auto engine = vve::simple::Engine{};
	auto &render = engine.renderSystem();
	if (!instantiate(engine, directory / "grey.obj") || render.renderMaterials().size() != 1U) { return 3; }
	const auto grey_index = render.renderMaterials().back().normal_texture_index;
	std::println("[RenderHeightMapTests] grey_normal={} no_texture={} texture_slots={}",
		grey_index, vve::simple::kNoTexture, render.sceneTextureCount());
	if (grey_index != vve::simple::kNoTexture) { return 4; }
	if (render.sceneTextureCount() != 0U) { return 5; }
	render.clearScene();
	if (!instantiate(engine, directory / "colour.obj") || render.renderMaterials().size() != 1U) { return 6; }
	const auto colour_index = render.renderMaterials().back().normal_texture_index;
	const auto colour_linear = render.sceneTextureIsLinear(colour_index);
	std::println("[RenderHeightMapTests] colour_normal={} texture_slots={} linear={}",
		colour_index, render.sceneTextureCount(), colour_linear.value_or(false));
	if (colour_index == vve::simple::kNoTexture || render.sceneTextureCount() != 1U) { return 7; }
	if (!colour_linear || !*colour_linear) { return 8; }
	// Establish the explicit map's slot independently, then demand that a dual-map material uses it too.
	render.clearScene();
	if (!instantiate(engine, directory / "normal.obj") || render.renderMaterials().size() != 1U) { return 9; }
	const auto normal_index = render.renderMaterials().back().normal_texture_index;
	if (normal_index == vve::simple::kNoTexture || !render.sceneTextureIsLinear(normal_index).value_or(false)) { return 10; }
	if (!instantiate(engine, directory / "both.obj") || render.renderMaterials().size() != 2U) { return 11; }
	const auto both_index = render.renderMaterials().back().normal_texture_index;
	std::println("[RenderHeightMapTests] both_normal={} explicit_normal={} texture_slots={} windows={}",
		both_index, normal_index, render.sceneTextureCount(), engine.windowSystem().windowCount());
	if (both_index != normal_index) { return 12; }
	return 0;
}
