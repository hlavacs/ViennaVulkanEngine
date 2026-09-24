#include <vulkan/vulkan_core.h>

/**
 * @file
 * @brief Imported material factors, semantic texture indices, and reuse coverage.
 *
 * Functional objects:
 * - main imports an OBJ material, verifies canonical typed paths, instantiates twice, and checks one-time GPU upload.
 */

import std;

import VEEngine;
import VEEngine.Simple;
import VEEngine.Simple.Scene;

/// @brief Verifies imported material textures are decoded once and cached across scene instances.
int main(int argc, char **argv) {
	auto engine = vve::simple::Engine{
		vve::ApplicationName{"render-material-import-tests"},
		vve::WindowSetups{vve::WindowSetup{}
			.id("main")
			.title("render-material-import-tests")
			.extent(vve::PixelExtent{.width = 64, .height = 64})
			.renderer(vve::RendererId{.value = "forward"})
			.visible(false)}};
	if (!engine.init()) { return 1; }

	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	const bool fixture_model = argc < 2;
	const auto model_path = fixture_model ? std::filesystem::path{VVE_TEST_MATERIAL_MODEL} : std::filesystem::path{argv[1]};
	const auto scene = assets.loadScene(model_path);
	if (!scene) { return 2; }
	const auto material_handles = assets.sceneMaterials(*scene);
	if (!material_handles) { return 3; }

	auto unique_paths = std::set<std::filesystem::path>{};
	bool asset_has_base_and_normal{};
	bool fixture_texture_counts_match{};
	std::size_t fixture_texture_handle_count{};
	std::size_t fixture_texture_source_count{};
	for (const auto material : *material_handles) {
		const auto sources = assets.materialTextureSources(material);
		const auto textures = assets.materialTextures(material);
		if (!sources || !textures) { return 4; }
		if (fixture_model && !sources->empty()) {
			fixture_texture_handle_count = textures->size();
			fixture_texture_source_count = sources->size();
			fixture_texture_counts_match = textures->size() == 2U && sources->size() == 2U;
		}
		bool has_base{};
		bool has_normal{};
		for (const auto &[semantic, path] : *sources) {
			if (!path.is_absolute()) { return 5; }
			unique_paths.insert(path);
			has_base = has_base || semantic == vve::simple::MaterialTextureSemantic::base_color;
			has_normal = has_normal || semantic == vve::simple::MaterialTextureSemantic::normal;
		}
		asset_has_base_and_normal = asset_has_base_and_normal || (has_base && has_normal);
	}
	if ((fixture_model && (!asset_has_base_and_normal || !fixture_texture_counts_match)) || unique_paths.empty()) { return 6; }

	render.clearScene();
	const auto first_instance = render.instantiateScene(*scene);
	if (!first_instance) { return 7; }
	const auto first_texture_count = render.sceneTextureCount();
	const auto first_material_count = render.sceneMaterialCount();
	std::size_t linear_texture_count{};
	for (std::size_t index{}; index < first_texture_count; ++index) {
		const auto linear = render.sceneTextureIsLinear(index);
		if (!linear) { return 8; }
		linear_texture_count += *linear ? 1U : 0U;
	}
	const auto base_normal_material = std::ranges::find_if(render.renderMaterials(), [](const auto &material) {
		return material.base_color_texture_index != vve::simple::kNoRenderTexture &&
			material.normal_texture_index != vve::simple::kNoRenderTexture &&
			material.base_color_texture_source.is_absolute();
	});
	const auto base_material = std::ranges::find_if(render.renderMaterials(), [](const auto &material) {
		return material.base_color_texture_index != vve::simple::kNoRenderTexture &&
			material.base_color_texture_source.is_absolute();
	});
	const bool render_has_base_and_normal = base_normal_material != render.renderMaterials().end();
	const auto imported_render_material = fixture_model ? base_normal_material : base_material;
	const bool factor_matches = !fixture_model || (imported_render_material != render.renderMaterials().end() &&
		std::abs(imported_render_material->base_color.value.x - 0.25F) <= 0.001F &&
		std::abs(imported_render_material->base_color.value.y - 0.50F) <= 0.001F &&
		std::abs(imported_render_material->base_color.value.z - 0.75F) <= 0.001F);
	if (fixture_model) {
		if (!render_has_base_and_normal) { return 8; }
		const auto diffuse_linear = render.sceneTextureIsLinear(base_normal_material->base_color_texture_index);
		const auto bump_linear = render.sceneTextureIsLinear(base_normal_material->normal_texture_index);
		if (!diffuse_linear || !bump_linear || *diffuse_linear || !*bump_linear || linear_texture_count != 1U) { return 8; }
	}
	if (imported_render_material == render.renderMaterials().end() ||
			(fixture_model && !render_has_base_and_normal) || first_texture_count == 0U ||
			first_texture_count != unique_paths.size() || !factor_matches) {
		return 8;
	}

	const auto second_instance = render.instantiateScene(*scene);
	if (!second_instance || render.sceneTextureCount() != first_texture_count ||
			render.sceneMaterialCount() != first_material_count) {
		return 9;
	}
	if (!engine.renderFrame()) { return 10; }
	const auto expected_gpu_textures = std::min(first_texture_count, vve::simple::kMaxSceneTextures);
	if (render.gpuTextureCount() != expected_gpu_textures ||
		render.gpuMaterialCount() != render.sceneMaterialCount() ||
		(!fixture_model && first_texture_count > 8U && render.gpuTextureCount() <= 8U)) {
		return 11;
	}
	const auto zero_base_color_materials = std::ranges::count_if(render.renderMaterials(), [](const auto &material) {
		return material.base_color.value.x == 0.0F && material.base_color.value.y == 0.0F &&
			material.base_color.value.z == 0.0F;
	});
	const auto materials_with_base_color_texture = std::ranges::count_if(render.renderMaterials(), [](const auto &material) {
		return material.base_color_texture_index != vve::simple::kNoRenderTexture;
	});
	const auto materials_with_normal_texture = std::ranges::count_if(render.renderMaterials(), [](const auto &material) {
		return material.normal_texture_index != vve::simple::kNoRenderTexture;
	});

	std::cout << "RenderMaterialImportTests model=" << model_path.filename().string()
				 << " sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount()
				 << " sceneMaterialCount=" << render.sceneMaterialCount()
				 << " gpuMaterialCount=" << render.gpuMaterialCount()
				 << " uniqueTexturePathCount=" << unique_paths.size()
				 << " assetTextureHandleCount=" << fixture_texture_handle_count
				 << " materialTextureSourceCount=" << fixture_texture_source_count
				 << " linearTextures=" << linear_texture_count
				 << " baseNormalMaterial=" << std::boolalpha << render_has_base_and_normal
				 << " zeroBaseColorMaterials=" << zero_base_color_materials
				 << " materialsWithBaseColorTexture=" << materials_with_base_color_texture
				 << " materialsWithNormalTexture=" << materials_with_normal_texture
				 << " baseColorFactor=" << imported_render_material->base_color.value.x << ','
				 << imported_render_material->base_color.value.y << ',' << imported_render_material->base_color.value.z << '\n';
	return 0;
}
