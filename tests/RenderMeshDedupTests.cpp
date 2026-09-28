#include <ranges> // Keep range concepts visible when Clang merges engine imports with Microsoft's std module.
#include <vulkan/vulkan_core.h>

/**
 * @file
 * @brief Render-mesh CPU sharing, stable GPU cache, and dirty-upload coverage.
 *
 * Functional objects:
 * - main verifies imported instancing shares one CPU/GPU mesh and isolated edits upload once.
 */

import std;

import VEEngine;
import VVE.TestSupport;
import VEEngine.Simple;

/// @brief Verifies instances share mesh storage and mesh edits retain stable handle-keyed uploads.
int main() {
	auto engine = vve::test::hiddenEngine("render-mesh-dedup-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 1; }

	auto &render = engine.renderSystem();
	const auto scene = engine.assets().loadScene(std::filesystem::path{VVE_TEST_MATERIAL_MODEL});
	if (!scene) { return 2; }
	render.clearScene();

	// Borrow catalog storage only within this block; later scene operations retain no views.
	{
		const auto meshes = engine.assets().sceneMeshes(*scene);
		if (!meshes || meshes->size() != 1U) { return 18; }
		const auto mesh = meshes->front();
		const auto first_view = engine.assets().meshPositionsView(mesh);
		const auto second_view = engine.assets().meshPositionsView(mesh);
		static_assert(std::ranges::random_access_range<typename decltype(first_view)::value_type>);
		static_assert(std::same_as<std::ranges::range_reference_t<typename decltype(first_view)::value_type>, const vve::Vec3 &>);
		if (!first_view || !second_view || first_view->base().data() == nullptr ||
			first_view->base().data() != second_view->base().data() || first_view->size() != 3U || second_view->size() != 3U) { return 19; }
		/// @brief Checks each borrowed array against its owning query and rejects missing handles.
		const auto check_view = [&]<typename View, typename Copy>(View view, Copy copy) {
			const auto a = std::invoke(view, engine.assets(), mesh);
			const auto b = std::invoke(view, engine.assets(), mesh);
			const auto owned = std::invoke(copy, engine.assets(), mesh);
			const auto missing = std::invoke(view, engine.assets(), vve::MeshHandle{});
			return a && b && owned && a->size() == b->size() && a->size() == owned->size() &&
				std::ranges::equal(*a, *b, [](const auto &left, const auto &right) { return std::addressof(left) == std::addressof(right); }) &&
				std::ranges::equal(*a, *owned, []<typename T>(const T &left, const T &right) {
					if constexpr (std::integral<T>) { return left == right; }
					else { return vve::math::lengthSquared(vve::math::subtract(left, right)) == 0; }
				}) && (a->empty() || std::addressof((*a)[0]) != owned->data()) &&
				!missing && missing.error() == vve::Error::missing_object;
		};
		using Assets = vve::simple::AssetSystem;
		if (!check_view(&Assets::meshPositionsView, &Assets::meshPositions) ||
			!check_view(&Assets::meshNormalsView, &Assets::meshNormals) ||
			!check_view(&Assets::meshTexcoordsView, &Assets::meshTexcoords) ||
			!check_view(&Assets::meshTangentsView, &Assets::meshTangents) ||
			!check_view(&Assets::meshIndicesView, &Assets::meshIndices)) { return 20; }
		std::println("D13 mesh views: positions={} stableStorage=1 ownedQueriesIndependent=1 missingHandlesRejected=1", first_view->size());
	}

	const auto first = render.instantiateScene(*scene);
	const auto first_vertex_count = render.sceneVertexCount();
	const auto second = render.instantiateScene(*scene);
	const auto third = render.instantiateScene(*scene);
	if (!first || !second || !third || render.sceneMeshCount() != 1U ||
		render.sceneInstanceCount() != 3U || render.sceneVertexCount() != first_vertex_count) {
		return 3;
	}
	if (!engine.renderFrame() || render.gpuMeshCount() != 1U || render.gpuMeshUploadCount() != 1U) {
		return 4;
	}
	// Only the catalog and the deduplicated render mesh own the imported vertex buffer.
	const auto imported_meshes = engine.assets().sceneMeshes(*scene);
	if (!imported_meshes || imported_meshes->size() != 1U) { return 21; }
	const auto catalog_positions = engine.assets().meshPositions(imported_meshes->front());
	const auto catalog_view = engine.assets().meshPositionsView(imported_meshes->front());
	const auto imported_render_mesh = render.forward().meshes.begin()->first;
	const auto *shared_mesh = render.forward().findRenderMesh(imported_render_mesh);
	if (!catalog_positions || !catalog_view || !shared_mesh || !shared_mesh->vertices ||
		shared_mesh->vertices.use_count() != 2 || shared_mesh->vertices->data() != catalog_view->base().data()) { return 22; }
	std::println("D4b imported vertices: owners={} catalogAndRenderDataEqual=1", shared_mesh->vertices.use_count());
	std::cout << "triple sceneMeshCount=" << render.sceneMeshCount()
		<< " sceneInstanceCount=" << render.sceneInstanceCount()
		<< " sceneVertexCount=" << render.sceneVertexCount()
		<< " gpuMeshCount=" << render.gpuMeshCount()
		<< " gpuMeshUploadCount=" << render.gpuMeshUploadCount() << '\n';

	const auto uploads_before_removal = render.gpuMeshUploadCount();
	const auto material_uploads_before_removal = render.gpuMaterialUploadCount();
	const auto second_objects = render.sceneInstanceObjects(*second);
	if (!second_objects || second_objects->size() != 1U) { return 5; }
	if (const auto removed = render.removeObject(second_objects->front()); !removed) { return 5; }
	if (!engine.renderFrame() || render.gpuMeshCount() != 1U ||
		render.gpuMeshUploadCount() != uploads_before_removal || render.sceneInstanceCount() != 2U) {
		return 6;
	}
	if (render.gpuMaterialUploadCount() != material_uploads_before_removal) { return 10; }
	// With no lights, both shared instances draw in the forward pass with one vertex/index bind.
	const auto stats = render.forward().lastFrameDrawStats();
	std::println("R2 dedup lastFrameDrawStats={},{},{},{}", stats.forwardDraws, stats.shadowDraws,
		stats.vertexBufferBinds, stats.pipelineBinds);
	if (stats.forwardDraws != 2U || stats.shadowDraws != 0U || stats.vertexBufferBinds != 1U || stats.pipelineBinds != 1U) { return 13; }
	std::cout << "remove sceneInstanceCount=" << render.sceneInstanceCount()
		<< " gpuMeshCount=" << render.gpuMeshCount()
		<< " gpuMeshUploadCount=" << render.gpuMeshUploadCount()
		<< " gpuMaterialUploadCount=" << render.gpuMaterialUploadCount() << '\n';

	const auto material_uploads_before_addition = render.gpuMaterialUploadCount();
	const auto cuboid = render.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
		vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{.value = vve::Vec3{0.25F, 0.5F, 0.75F}});
	if (!cuboid) { return 7; }
	auto positions = vve::Vector<vve::Vec3>{};
	positions.reserve(24U);
	for (const auto index : std::views::iota(0U, 24U)) {
		const auto offset = static_cast<float>(index) * 0.001F;
		positions.push_back(vve::Vec3{-0.5F + offset, -0.5F, -0.5F});
	}
	if (const auto updated = render.setObjectMeshPositions(*cuboid, std::move(positions)); !updated) {
		return 8;
	}
	const auto vertex_count_before_upload = render.sceneVertexCount();
	if (!engine.renderFrame() || render.gpuMeshCount() != 2U ||
		render.gpuMeshUploadCount() != uploads_before_removal + 1U ||
		render.sceneVertexCount() != vertex_count_before_upload) {
		return 9;
	}
	if (render.gpuMaterialUploadCount() != material_uploads_before_addition + 1U) { return 11; }
	std::cout << "update sceneMeshCount=" << render.sceneMeshCount()
		<< " sceneInstanceCount=" << render.sceneInstanceCount()
		<< " sceneVertexCount=" << render.sceneVertexCount()
		<< " gpuMeshCount=" << render.gpuMeshCount()
		<< " gpuMeshUploadCount=" << render.gpuMeshUploadCount()
		<< " gpuMaterialUploadCount=" << render.gpuMaterialUploadCount() << '\n';
	// Editing a shared imported mesh grows the mesh vector; preserve the original and upload one isolated clone.
	const auto first_objects = render.sceneInstanceObjects(*first);
	const auto asset_meshes = engine.assets().sceneMeshes(*scene);
	if (!first_objects || first_objects->size() != 1U || !asset_meshes || asset_meshes->size() != 1U) { return 14; }
	auto shared_positions = engine.assets().meshPositions(asset_meshes->front());
	if (!shared_positions || shared_positions->empty()) { return 15; }
	const auto original_mesh = render.forward().meshes.begin()->first;
	const auto original_minimum = render.forward().meshes.at(original_mesh).localBox.minimum.value;
	const auto uploads_before_clone = render.gpuMeshUploadCount();
	std::set<vve::simple::RenderMeshHandle> original_mesh_handles{};
	for (const auto &[handle, mesh] : render.forward().meshes) { original_mesh_handles.insert(handle); }
	for (auto &position : *shared_positions) { position.x += 0.25F; }
	if (!render.setObjectMeshPositions(first_objects->front(), std::move(*shared_positions)) ||
		render.sceneMeshCount() != 3U || !engine.renderFrame() || render.gpuMeshCount() != 3U ||
		render.gpuMeshUploadCount() != uploads_before_clone + 1U) { return 16; }
	const auto retained_minimum = render.forward().meshes.at(original_mesh).localBox.minimum.value;
	if (retained_minimum.x != original_minimum.x || retained_minimum.y != original_minimum.y ||
		retained_minimum.z != original_minimum.z) { return 17; }
	std::println("Vector growth: shared mesh edit isolated; sceneMeshCount={} gpuMeshCount={}",
		render.sceneMeshCount(), render.gpuMeshCount());
	// Detaching preserves every catalog attribute and leaves the other instance on the original buffer.
	const auto after_edit = engine.assets().meshPositions(imported_meshes->front());
	const auto after_view = engine.assets().meshPositionsView(imported_meshes->front());
	shared_mesh = render.forward().findRenderMesh(imported_render_mesh);
	if (!after_edit || !after_view || !shared_mesh || shared_mesh->vertices.use_count() != 2 ||
		shared_mesh->vertices->data() != catalog_view->base().data() || after_view->base().data() != catalog_view->base().data() ||
		!std::ranges::equal(*catalog_positions, *after_edit, [](const auto &a, const auto &b) {
			return vve::math::lengthSquared(vve::math::subtract(a, b)) == 0;
		})) { return 23; }
	const auto cloned = std::ranges::find_if(render.forward().meshes, [&](const auto &entry) { return !original_mesh_handles.contains(entry.first); });
	if (cloned == render.forward().meshes.end()) { return 24; }
	const auto *edited_mesh = render.forward().findRenderMesh(cloned->first);
	if (!edited_mesh || !edited_mesh->vertices || edited_mesh->vertices.use_count() != 1 ||
		edited_mesh->vertices->data() == shared_mesh->vertices->data() || edited_mesh->vertices->size() != catalog_positions->size()) { return 24; }
	// Positions change; normal, UV and tangent components retain the catalog's exact values.
	for (std::size_t i{}; i < catalog_positions->size(); ++i) {
		const auto &source = (*shared_mesh->vertices)[i];
		const auto &edited = (*edited_mesh->vertices)[i];
		if (edited.position.x != source.position.x + 0.25F || edited.position.y != source.position.y || edited.position.z != source.position.z ||
			vve::math::lengthSquared(vve::math::subtract(edited.normal, source.normal)) != 0 ||
			vve::math::lengthSquared(vve::math::subtract(edited.uv, source.uv)) != 0 ||
			vve::math::lengthSquared(vve::math::subtract(edited.tangent, source.tangent)) != 0) { return 25; }
	}
	// A later edit keeps the already-private allocation and restores the original position values.
	const auto *private_data = edited_mesh->vertices->data();
	if (!render.setObjectMeshPositions(first_objects->front(), *catalog_positions) ||
		edited_mesh->vertices->data() != private_data || edited_mesh->vertices.use_count() != 1 ||
		!engine.renderFrame()) { return 26; }
	for (std::size_t i{}; i < catalog_positions->size(); ++i) {
		if (vve::math::lengthSquared(vve::math::subtract((*edited_mesh->vertices)[i].position, (*catalog_positions)[i])) != 0 ||
			vve::math::lengthSquared(vve::math::subtract((*shared_mesh->vertices)[i].position, (*catalog_positions)[i])) != 0) { return 26; }
	}
	std::println("D4b copy-on-write: catalogUnchanged=1 isolatedVertices=1 attributesPreserved=1 privateBufferReused=1");
#ifndef NDEBUG
	// Include mesh uploads and removals in the validation error check.
	const auto &renderer = render.forward();
	std::cout << "RenderMeshDedupTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 12; }
#endif
	return 0;
}
