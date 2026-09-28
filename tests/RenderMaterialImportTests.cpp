#include <vulkan/vulkan_core.h>

/**
 * @file
 * @brief Imported material factors, semantic texture indices, and reuse coverage.
 *
 * Functional objects:
 * - hasDistinctAssetTextureCounts checks shared source paths, empty scenes and missing handles.
 * - main imports an OBJ material, verifies canonical typed paths, instantiates twice, and checks one-time GPU upload.
 */

import std;

import VEEngine;
import VVE.TestSupport;
import VEEngine.Simple;
import VEEngine.Simple.Scene;

/// @brief Checks derived texture counts for shared paths, empty scenes and unknown handles without rendering.
bool hasDistinctAssetTextureCounts(vve::simple::AssetSystem &assets) {
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	// One source is shared by two materials and by two semantics within the first material.
	{
		std::ofstream material{directory / "shared_textures.mtl"};
		material << "newmtl A\nmap_Kd shared.png\nmap_Kn shared.png\nnewmtl B\nmap_Kd shared.png\n";
		std::ofstream model{directory / "shared_textures.obj"};
		model << "mtllib shared_textures.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n"
			<< "usemtl A\nf 1 2 3\nusemtl B\nf 1 3 2\n";
	}
	const auto scene = assets.loadScene(directory / "shared_textures.obj");
	if (!scene) { return false; }
	const auto materials = assets.sceneMaterials(*scene);
	if (!materials) { return false; }
	std::size_t references{};
	// Confirm all three authored references survived import before checking their single identity.
	for (const auto handle : *materials) {
		const auto sources = assets.materialTextureSources(handle);
		if (!sources) { return false; }
		references += sources->size();
	}
	const auto count = assets.sceneTextureCount(*scene);
	const auto empty = assets.addScene(vve::ObjectName{.value = "empty-textures"});
	const auto missing = assets.sceneTextureCount(vve::SceneHandle{});
	std::cout << "RenderMaterialImportTests sharedSourceReferences=" << references
		<< " sharedSceneTextureCount=" << count.value_or(0U) << '\n';
	return references == 3U && count == 1U && empty && assets.sceneTextureCount(*empty) == 0U &&
		!missing && missing.error() == vve::Error::missing_object;
}

/// @brief Verifies imported material textures are decoded once and cached across scene instances.
int main(int argc, char **argv) {
	auto engine = vve::test::hiddenEngine("render-material-import-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 1; }

	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	if (!hasDistinctAssetTextureCounts(assets)) { return 14; }
	const bool fixture_model = argc < 2;
	const auto model_path = fixture_model ? std::filesystem::path{VVE_TEST_MATERIAL_MODEL} : std::filesystem::path{argv[1]};
	const auto scene = assets.loadScene(model_path);
	if (!scene) { return 2; }
	const auto material_handles = assets.sceneMaterials(*scene);
	if (!material_handles) { return 3; }

	auto unique_paths = std::set<std::filesystem::path>{};
	bool asset_has_base_and_normal{};
	bool fixture_texture_counts_match{};
	std::size_t fixture_texture_source_count{};
	// Asset counts derive from source paths; repeated materials or semantics must not inflate them.
	for (const auto material : *material_handles) {
		const auto sources = assets.materialTextureSources(material);
		if (!sources) { return 4; }
		if (fixture_model && !sources->empty()) {
			fixture_texture_source_count = sources->size();
			fixture_texture_counts_match = sources->size() == 2U;
		}
		bool has_base{};
		bool has_normal{};
		for (const auto &source : *sources) {
			if (!source.path.is_absolute()) { return 5; }
			unique_paths.insert(source.path);
			has_base = has_base || source.semantic == vve::simple::MaterialTextureSemantic::base_color;
			has_normal = has_normal || source.semantic == vve::simple::MaterialTextureSemantic::normal;
		}
		asset_has_base_and_normal = asset_has_base_and_normal || (has_base && has_normal);
	}
	if ((fixture_model && (!asset_has_base_and_normal || !fixture_texture_counts_match)) || unique_paths.empty()) { return 6; }
	const auto asset_texture_count = assets.sceneTextureCount(*scene);
	std::cout << "RenderMaterialImportTests assetSceneTextureCount=" << asset_texture_count.value_or(0U)
		<< " unique_paths=" << unique_paths.size() << '\n';
	if (!asset_texture_count || *asset_texture_count != unique_paths.size()) { return 13; }

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
	const auto base_normal_material = std::ranges::find_if(render.renderMaterials(), [&render](const auto &material) {
		return material.base_color_texture_index != vve::simple::kNoTexture &&
			material.normal_texture_index != vve::simple::kNoTexture &&
			render.sceneTextureIsLinear(material.base_color_texture_index) == false &&
			render.sceneTextureIsLinear(material.normal_texture_index) == true;
	});
	const auto base_material = std::ranges::find_if(render.renderMaterials(), [&render](const auto &material) {
		return material.base_color_texture_index != vve::simple::kNoTexture &&
			render.sceneTextureIsLinear(material.base_color_texture_index) == false;
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
		const auto normal_linear = render.sceneTextureIsLinear(base_normal_material->normal_texture_index);
		if (!diffuse_linear || !normal_linear || *diffuse_linear || !*normal_linear || linear_texture_count != 1U) { return 8; }
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
		return material.base_color_texture_index != vve::simple::kNoTexture;
	});
	const auto materials_with_normal_texture = std::ranges::count_if(render.renderMaterials(), [](const auto &material) {
		return material.normal_texture_index != vve::simple::kNoTexture;
	});

	std::cout << "RenderMaterialImportTests model=" << model_path.filename().string()
				 << " sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount()
				 << " sceneMaterialCount=" << render.sceneMaterialCount()
				 << " gpuMaterialCount=" << render.gpuMaterialCount()
				 << " uniqueTexturePathCount=" << unique_paths.size()
				 << " assetSceneTextureCount=" << *asset_texture_count
				 << " materialTextureSourceCount=" << fixture_texture_source_count
				 << " linearTextures=" << linear_texture_count
				 << " baseNormalMaterial=" << std::boolalpha << render_has_base_and_normal
				 << " zeroBaseColorMaterials=" << zero_base_color_materials
				 << " materialsWithBaseColorTexture=" << materials_with_base_color_texture
				 << " materialsWithNormalTexture=" << materials_with_normal_texture
				 << " baseColorFactor=" << imported_render_material->base_color.value.x << ','
				 << imported_render_material->base_color.value.y << ',' << imported_render_material->base_color.value.z << '\n';
#ifndef NDEBUG
	// Include imported texture and material uploads in the validation error check.
	const auto &renderer = render.forward();
	std::cout << "RenderMaterialImportTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 12; }
#endif
	return 0;
}
